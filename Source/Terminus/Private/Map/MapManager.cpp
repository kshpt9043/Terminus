// Fill out your copyright notice in the Description page of Project Settings.


#include "Map/MapManager.h"
#include "Player/TerminusPlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Game/TerminusRunSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

AMapManager::AMapManager()
{ 
    PrimaryActorTick.bCanEverTick = false;

    // ★ 네트워크 복제(Replication) 활성화
    bReplicates = true;
    bAlwaysRelevant = true; // 모든 클라이언트에 항상 데이터 전송되도록 설정
    
    // 기획 비율 기본값 세팅 (디테일 패널에서 변경 가능)
    RoomTypeWeights.Add(ERoomType::MONSTER, 55.0f);
    RoomTypeWeights.Add(ERoomType::BREAK, 10.0f);
    RoomTypeWeights.Add(ERoomType::STORE, 10.0f);
    RoomTypeWeights.Add(ERoomType::GUARDIAN, 10.0f);
    RoomTypeWeights.Add(ERoomType::EVENT, 15.0f);
}

void AMapManager::HandleSelectRoomRequest(ATerminusPlayerController* Requester, int32 RoomId)
{
    if (!HasAuthority() || !Requester) return;

    // 요청자 PS 는 서버가 PC 에서 직접 꺼냄. 클라가 남의 PS 를 넘겨 조작하는 걸 막기 위해
    ATerminusPlayerState* RequestingPS = Requester->GetPlayerState<ATerminusPlayerState>();
    if (!RequestingPS) return;

    const FRoomNode* TargetRoom = Rooms.FindByPredicate([RoomId](const FRoomNode& Node) {
        return Node.RoomId == RoomId;
    });

    if (!TargetRoom) return;

    // 1. 유효 노드 검증
    if (!IsValidNextRoom(RequestingPS->GetRunState(), *TargetRoom))
    {
        Requester->Client_OnRoomSelectFailed(TEXT("이동할 수 없는 경로의 방입니다."));
        return;
    }

    // ★ [테스트] 클릭한 방을 즉시 클리어한 것으로 처리하고 끝. 아래 선택 흐름은 안 탐
    if (bTestInstantClear)
    {
        UE_LOG(LogTemp, Log, TEXT("[Test] Room %d (Row %d) Cleared! Moving to next level..."), RoomId, TargetRoom->Row);
        RequestingPS->Test_ClearAndMoveToRoom(TargetRoom->RoomId, TargetRoom->Row);
        return;
    }

    // 2. 토글 처리 (같은 방 다시 누르면 선택 취소)
    if (RequestingPS->GetSelectedRoomId() == RoomId)
    {
        RequestingPS->SetSelectedRoomId(-1);
        return;
    }

    // 3. 인원 수용 검증. 나를 뺀 나머지 중 이 방을 고른 사람 수
    //    (다른 방에서 옮겨오는 경우 내 기존 선택은 세면 안 됨)
    int32 CurrentlySelectedCount = 0;
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS);
            if (TPS && TPS != RequestingPS && TPS->GetSelectedRoomId() == RoomId)
            {
                CurrentlySelectedCount++;
            }
        }
    }

    if (CurrentlySelectedCount >= TargetRoom->MaxPlayers)
    {
        Requester->Client_OnRoomSelectFailed(TEXT("선택한 방의 정원이 가득 찼습니다!"));
        return;
    }

    // 4. 선택 완료
    RequestingPS->SetSelectedRoomId(RoomId);

    // 5. 전원 확인 후 던전 진입
    CheckAllPlayersReadyAndStart();
}

void AMapManager::BeginPlay()
{
    Super::BeginPlay();
    
    // 클라가 자기 손으로 스폰한 MapManager 는 복제본이 아니라 클라 로컬 액터 -> 클라가 Authority 를 가짐
    // 그대로 두면 클라가 자기 지도를 따로 뽑아서 서버 지도와 달라짐 (레벨 BP 가 서버/클라 양쪽에서 스폰할 때)
    // 진짜는 서버가 스폰해서 복제로 내려오는 것 하나뿐이라 로컬 것은 치움
    if (GetNetMode() == NM_Client && GetLocalRole() == ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MapManager] 클라에서 로컬로 스폰된 MapManager 제거. 스폰은 서버에서만 할 것 (레벨 BP 에 Switch Has Authority)"));
        Destroy();
        return;
    }
    
    // ★ 오직 서버(Authority)에서만 맵을 생성합니다.
    if (HasAuthority())
    {
        // 지도 원본은 런 서브시스템(GameInstance)에 있음 -> 전투 갔다 돌아와도 같은 지도
        UGameInstance* GI = GetGameInstance();
        UTerminusRunSubsystem* Run = GI ? GI->GetSubsystem<UTerminusRunSubsystem>() : nullptr;

        if (Run && Run->HasMap())
        {
            Rooms = Run->GetRooms();
            UE_LOG(LogTemp, Log, TEXT("[MapGenerator] 이번 런의 기존 지도 재사용 (방 %d개)"), Rooms.Num());
        }
        else
        {
            // 주점에서 확정된 실제 인원. 주점을 안 거쳤으면(0) 디테일 패널 값 그대로
            if (Run && Run->GetPartySize() > 0)
            {
                CurrentPlayerCount = Run->GetPartySize();
            }
            CurrentPlayerCount = FMath::Max(1, CurrentPlayerCount);

            Rooms = GenerateMap();

            if (Run)
            {
                Run->SetRooms(Rooms);
            }
        }

        // 서버 자신(Listen Server)의 UI 업데이트를 위해 델리게이트 알림
        OnMapGenerated.Broadcast(Rooms);
    }
}

TArray<FRoomNode> AMapManager::GenerateMap()
{
    TArray<FRoomNode> Map;
    bool bIsValidMap = false;
    int32 RetryCount = 0;
    const int32 MaxRetries = 200;

    const int32 ValidTotalLevels = FMath::Max(2, TotalLevels);
    const int32 LastRowIndex = ValidTotalLevels - 1; // 보스방 Row
    const int32 ValidMaxRooms = FMath::Max(1, MaxRoomsPerRow);
    const int32 CenterCol = ValidMaxRooms / 2;

    // 1개 방만 가질 레벨들의 집합 (최대 2개, 연속 불가)
    TSet<int32> ChosenSingleRoomRows;

    while (!bIsValidMap && RetryCount < MaxRetries)
    {
        RetryCount++;
        Map.Empty();
        ChosenSingleRoomRows.Empty();
        int32 GlobalRoomId = 1;

        // ==========================================
        // [Pass 0] 1개 방을 가질 레벨 추첨 (최대 2개, 연속X)
        // ==========================================
        // 중간 레벨 범위: Row 2 ~ (LastRowIndex - 1)
        int32 MinMiddleRow = 2;
        int32 MaxMiddleRow = LastRowIndex - 1;

        if (MinMiddleRow <= MaxMiddleRow)
        {
            // 1개만 정할지, 2개까지 정할지 무작위 결정 (1 또는 2)
            int32 TargetSingleCount = FMath::RandRange(1, 2);

            // 후보 레벨 목록 생성
            TArray<int32> AvailableRows;
            for (int32 r = MinMiddleRow; r <= MaxMiddleRow; ++r)
            {
                AvailableRows.Add(r);
            }

            for (int32 i = 0; i < TargetSingleCount && AvailableRows.Num() > 0; ++i)
            {
                int32 RandomIdx = FMath::RandRange(0, AvailableRows.Num() - 1);
                int32 SelectedRow = AvailableRows[RandomIdx];
                ChosenSingleRoomRows.Add(SelectedRow);

                // ★ 연속 출현 방지: 선택된 Row 및 바로 인접한 Row(±1)를 후보에서 제거
                AvailableRows.RemoveAll([SelectedRow](int32 RowVal) {
                    return FMath::Abs(RowVal - SelectedRow) <= 1;
                });
            }
        }

        // ==========================================
        // [Pass 1] TotalLevels(Row 0 ~ LastRowIndex) 방 생성
        // ==========================================
        for (int32 Row = 0; Row < ValidTotalLevels; ++Row)
        {
            // 1. [1레벨 / Row 0] 퀘스트방 1개 고정
            if (Row == 0)
            {
                Map.Add(CreateRoom(GlobalRoomId++, Row, CenterCol, ERoomType::QUEST, CurrentPlayerCount));
                continue;
            }

            // 2. [마지막 레벨 / LastRowIndex] 보스방 1개 고정
            if (Row == LastRowIndex)
            {
                Map.Add(CreateRoom(GlobalRoomId++, Row, CenterCol, ERoomType::BOSS, CurrentPlayerCount));
                continue;
            }

            // 3. [1개 방 레벨로 당첨된 레벨들]
            if (ChosenSingleRoomRows.Contains(Row))
            {
                int32 RandomCol = FMath::RandRange(0, ValidMaxRooms - 1);
                ERoomType RoomType = GetWeightedRandomRoomType();

                // 이 층엔 방이 하나뿐이라 파티 전원이 여길 지나가야 함 -> 정원 = 전체 인원
                Map.Add(CreateRoom(GlobalRoomId++, Row, RandomCol, RoomType, CurrentPlayerCount));
                continue;
            }

            // 4. [나머지 일반 레벨 (2레벨 포함): 최소 2개 이상 방 생성 보장]
            TArray<FRoomNode> RowRooms;
            for (int32 Col = 0; Col < ValidMaxRooms; ++Col)
            {
                if (FMath::RandRange(0.0f, 1.0f) > 0.6f)
                {
                    ERoomType RoomType = (Row == 1) ? ERoomType::MONSTER : GetWeightedRandomRoomType();
                    int32 RandomMaxPlayers = FMath::RandRange(1, CurrentPlayerCount);

                    RowRooms.Add(CreateRoom(GlobalRoomId++, Row, Col, RoomType, RandomMaxPlayers));
                }
            }

            // 방이 2개 미만으로 뽑혔다면 무조건 2개가 되도록 추가 생성
            while (RowRooms.Num() < 2 && ValidMaxRooms >= 2)
            {
                int32 RandomCol = FMath::RandRange(0, ValidMaxRooms - 1);
                
                bool bAlreadyExists = RowRooms.ContainsByPredicate([RandomCol](const FRoomNode& Room) {
                    return Room.Col == RandomCol;
                });

                if (!bAlreadyExists)
                {
                    ERoomType RoomType = (Row == 1) ? ERoomType::MONSTER : GetWeightedRandomRoomType();
                    int32 RandomMaxPlayers = FMath::RandRange(1, CurrentPlayerCount);

                    RowRooms.Add(CreateRoom(GlobalRoomId++, Row, RandomCol, RoomType, RandomMaxPlayers));
                }
            }

            Map.Append(RowRooms);
        }

        // ==========================================
        // [Pass 2] 레벨 간 연결(Edge) 구축
        // ==========================================
        for (int32 Row = 0; Row < LastRowIndex; ++Row)
        {
            TArray<int32> CurrentRowIndices;
            TArray<int32> NextRowIndices;

            for (int32 i = 0; i < Map.Num(); ++i)
            {
                if (Map[i].Row == Row) CurrentRowIndices.Add(i);
                else if (Map[i].Row == Row + 1) NextRowIndices.Add(i);
            }

            // 정방향 연결
            for (int32 CurrIdx : CurrentRowIndices)
            {
                bool bConnected = false;
                for (int32 NextIdx : NextRowIndices)
                {
                    if (FMath::Abs(Map[CurrIdx].Col - Map[NextIdx].Col) <= 1)
                    {
                        Map[CurrIdx].ConnectedRoomIds.AddUnique(Map[NextIdx].RoomId);
                        bConnected = true;
                    }
                }

                if (!bConnected && NextRowIndices.Num() > 0)
                {
                    int32 ClosestNextIdx = NextRowIndices[0];
                    int32 MinColDist = FMath::Abs(Map[CurrIdx].Col - Map[ClosestNextIdx].Col);

                    for (int32 NextIdx : NextRowIndices)
                    {
                        int32 Dist = FMath::Abs(Map[CurrIdx].Col - Map[NextIdx].Col);
                        if (Dist < MinColDist)
                        {
                            MinColDist = Dist;
                            ClosestNextIdx = NextIdx;
                        }
                    }
                    Map[CurrIdx].ConnectedRoomIds.AddUnique(Map[ClosestNextIdx].RoomId);
                }
            }

            // 역방향 (고립 방 방지) 연결
            for (int32 NextIdx : NextRowIndices)
            {
                bool bIsTargeted = false;
                for (int32 CurrIdx : CurrentRowIndices)
                {
                    if (Map[CurrIdx].ConnectedRoomIds.Contains(Map[NextIdx].RoomId))
                    {
                        bIsTargeted = true;
                        break;
                    }
                }

                if (!bIsTargeted && CurrentRowIndices.Num() > 0)
                {
                    int32 ClosestCurrIdx = CurrentRowIndices[0];
                    int32 MinColDist = FMath::Abs(Map[NextIdx].Col - Map[ClosestCurrIdx].Col);

                    for (int32 CurrIdx : CurrentRowIndices)
                    {
                        int32 Dist = FMath::Abs(Map[NextIdx].Col - Map[CurrIdx].Col);
                        if (Dist < MinColDist)
                        {
                            MinColDist = Dist;
                            ClosestCurrIdx = CurrIdx;
                        }
                    }
                    Map[ClosestCurrIdx].ConnectedRoomIds.AddUnique(Map[NextIdx].RoomId);
                }
            }
        }

        // ==========================================
        // [Pass 3] 경로 및 분기 수용 조건 검증
        // ==========================================
        bool bPathValid = ValidatePathToBoss(Map, LastRowIndex);
        bool bCapacityValid = ValidatePlayerCapacity(Map);

        bIsValidMap = bPathValid && bCapacityValid;
    }

    if (!bIsValidMap)
    {
        UE_LOG(LogTemp, Error, TEXT("[MapGenerator] %d번 재시도해도 조건을 만족하는 지도를 못 만듦. 마지막 지도를 그대로 씀 (진행 불가 구간이 있을 수 있음)"), MaxRetries);
    }

    FString SingleRowsStr = "";
    for (int32 SingleRow : ChosenSingleRoomRows)
    {
        SingleRowsStr += FString::Printf(TEXT("%d "), SingleRow);
    }

    UE_LOG(LogTemp, Log, TEXT("[MapGenerator] Map Generated with SingleRoomRows at [ %s] (Attempts: %d)"), *SingleRowsStr, RetryCount);
    return Map;
}

FRoomNode AMapManager::CreateRoom(int32 RoomId, int32 Row, int32 Col, ERoomType Type, int32 MaxPlayers)
{
    FRoomNode Room;
    Room.RoomId = RoomId;
    Room.Row = Row;
    Room.Col = Col;
    Room.Type = Type;
    Room.MaxPlayers = MaxPlayers;
    return Room;
}

ERoomType AMapManager::GetWeightedRandomRoomType()
{
    float TotalWeight = 0.0f;
    for (const auto& Pair : RoomTypeWeights)
    {
        TotalWeight += Pair.Value;
    }

    if (TotalWeight <= 0.0f) return ERoomType::MONSTER;

    float RandomValue = FMath::RandRange(0.0f, TotalWeight);
    float AccumulatedWeight = 0.0f;

    for (const auto& Pair : RoomTypeWeights)
    {
        AccumulatedWeight += Pair.Value;
        if (RandomValue <= AccumulatedWeight)
        {
            return Pair.Key;
        }
    }

    return ERoomType::MONSTER;
}

bool AMapManager::ValidatePathToBoss(const TArray<FRoomNode>& InMap, int32 LastRowIndex)
{
    if (InMap.Num() == 0) return false;

    TMap<int32, const FRoomNode*> RoomLookup;
    for (const FRoomNode& Node : InMap) RoomLookup.Add(Node.RoomId, &Node);

    TQueue<int32> SearchQueue;
    TSet<int32> VisitedRoomIds;

    for (const FRoomNode& Node : InMap)
    {
        if (Node.Row == 0)
        {
            SearchQueue.Enqueue(Node.RoomId);
            VisitedRoomIds.Add(Node.RoomId);
        }
    }

    while (!SearchQueue.IsEmpty())
    {
        int32 CurrentRoomId;
        SearchQueue.Dequeue(CurrentRoomId);

        const FRoomNode** CurrentNodePtr = RoomLookup.Find(CurrentRoomId);
        if (!CurrentNodePtr || !(*CurrentNodePtr)) continue;

        const FRoomNode* CurrentNode = *CurrentNodePtr;

        if (CurrentNode->Type == ERoomType::BOSS || CurrentNode->Row == LastRowIndex)
        {
            return true;
        }

        for (int32 ConnectedId : CurrentNode->ConnectedRoomIds)
        {
            if (!VisitedRoomIds.Contains(ConnectedId))
            {
                VisitedRoomIds.Add(ConnectedId);
                SearchQueue.Enqueue(ConnectedId);
            }
        }
    }

    return false;
}

bool AMapManager::ValidatePlayerCapacity(const TArray<FRoomNode>& InMap)
{
    TMap<int32, const FRoomNode*> RoomLookup;
    TMap<int32, int32> CapacityByRow;
    for (const FRoomNode& Node : InMap)
    {
        RoomLookup.Add(Node.RoomId, &Node);
        CapacityByRow.FindOrAdd(Node.Row) += Node.MaxPlayers;
    }

    // 층 단위: 한 층의 정원 합이 인원보다 적으면 누군가는 그 층을 못 지나감
    for (const TPair<int32, int32>& Pair : CapacityByRow)
    {
        if (Pair.Value < CurrentPlayerCount)
        {
            return false;
        }
    }

    for (const FRoomNode& SourceNode : InMap)
    {
        if (SourceNode.Type == ERoomType::MONSTER || SourceNode.Type == ERoomType::GUARDIAN)
        {
            if (SourceNode.ConnectedRoomIds.Num() >= 2)
            {
                int32 CombinedCapacity = 0;
                for (int32 TargetId : SourceNode.ConnectedRoomIds)
                {
                    if (const FRoomNode** TargetPtr = RoomLookup.Find(TargetId))
                    {
                        CombinedCapacity += (*TargetPtr)->MaxPlayers;
                    }
                }

                if (CombinedCapacity < CurrentPlayerCount)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool AMapManager::IsValidNextRoom(const FRunState& PlayerRunState, const FRoomNode& TargetRoom)
{
    // 시작 단계(1레벨)면 퀘스트방(Row == 0)만 선택 가능
    if (PlayerRunState.CurrentMapLevel == 0)
    {
        return TargetRoom.Row == 0;
    }

    // 2레벨 이후: 현재 방의 ConnectedRoomIds에 포함된 방인가?
    const FRoomNode* CurrentRoom = Rooms.FindByPredicate([&PlayerRunState](const FRoomNode& Node) {
        return Node.RoomId == PlayerRunState.CurrentRoomId;
    });

    if (!CurrentRoom) return false;

    return CurrentRoom->ConnectedRoomIds.Contains(TargetRoom.RoomId);
}

void AMapManager::CheckAllPlayersReadyAndStart()
{
    if (!HasAuthority()) return;

    const AGameStateBase* GS = GetWorld()->GetGameState();
    if (!GS) return;

    int32 TotalPlayers = GS->PlayerArray.Num();
    int32 ReadyPlayers = 0;

    for (APlayerState* PS : GS->PlayerArray)
    {
        const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS);
        if (TPS && TPS->GetSelectedRoomId() != -1)
        {
            ReadyPlayers++;
        }
    }

    if (ReadyPlayers >= TotalPlayers && TotalPlayers > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("모든 플레이어 방 선택 완료! 던전으로 이동합니다."));
        GetWorld()->ServerTravel("/Game/Maps/Lv_DungeonPlay?listen");
    }
}

// 네트워크 복제 속성 등록
void AMapManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMapManager, Rooms);
}

// ★ 클라이언트가 서버로부터 Rooms 데이터 수신을 완료했을 때 실행됨
void AMapManager::OnRep_Rooms()
{
    UE_LOG(LogTemp, Warning, TEXT("Map Generated!"))
    // 데이터가 수신되었으므로 클라이언트 UI에 맵을 그리라고 알림
    OnMapGenerated.Broadcast(Rooms);
}

