// Fill out your copyright notice in the Description page of Project Settings.


#include "Map/MapManager.h"
#include "Player/TerminusPlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Game/TerminusRunSubsystem.h"
#include "Dungeon/DungeonAreaSubsystem.h"
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

    // 0. 구역에서 방이 진행 중이면 지도 선택 불가 (전 구역이 끝나야 다음 선택)
    if (const UDungeonAreaSubsystem* Areas = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
    {
        if (Areas->IsAnyRoomInProgress())
        {
            Requester->Client_OnRoomSelectFailed(TEXT("방을 진행하는 중에는 다음 방을 고를 수 없습니다."));
            return;
        }
    }

    // 1. 유효 노드 검증
    if (!IsValidNextRoom(RequestingPS->GetRunState(), *TargetRoom))
    {
        Requester->Client_OnRoomSelectFailed(TEXT("이동할 수 없는 경로의 방입니다."));
        return;
    }

    // 2. 싱글은 기획상 "방을 선택할 경우 바로 입장한다". 정원/전원 대기는 멀티 규칙이라 안 탐
    if (IsSinglePlayerRun())
    {
        RequestingPS->SetSelectedRoomId(RoomId);
        EnterSelectedRooms({ RequestingPS });
        return;
    }

    // 3. 토글 처리 (같은 방 다시 누르면 선택 취소)
    if (RequestingPS->GetSelectedRoomId() == RoomId)
    {
        RequestingPS->SetSelectedRoomId(-1);
        return;
    }

    // 4. 인원 수용 검증. 나를 뺀 나머지 중 이 방을 고른 사람 수
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

    // 5. 선택 완료. 다른 플레이어 화면에도 이 선택이 표시된다(RunState 복제)
    RequestingPS->SetSelectedRoomId(RoomId);

    // 6. 전원 선택이 끝났으면 각자 고른 방으로 입장
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
    // 정원을 랜덤으로 뽑고 조건에 맞을 때까지 다시 뽑는 방식이라 인원이 늘수록 성공률이 급락한다.
    // 4인 기준 1회 성공률이 약 2% 라서 200회로는 1.8% 확률로 유효한 지도를 못 만든다(실측).
    // 1000회면 실패가 사실상 사라지고(4000판 중 0회, 최대 458회 시도) 비용도 무시할 수준
    const int32 MaxRetries = 1000;

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

                    RowRooms.Add(CreateRoom(GlobalRoomId++, Row, Col, RoomType, GetRoomCapacity(RoomType)));
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

                    RowRooms.Add(CreateRoom(GlobalRoomId++, Row, RandomCol, RoomType, GetRoomCapacity(RoomType)));
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

int32 AMapManager::GetRoomCapacity(ERoomType Type) const
{
    // 기획: 인원 제한은 몬스터/가디언 방에만 붙는다.
    // 상점이나 휴식터에 정원 1 이 걸리면 파티가 아무 이유 없이 쪼개진다
    const bool bLimited = (Type == ERoomType::MONSTER || Type == ERoomType::GUARDIAN);

    return bLimited ? FMath::RandRange(1, CurrentPlayerCount) : CurrentPlayerCount;
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

    // 방 단위: 그 방에 들어간 인원이 전원 다음 방으로 넘어갈 수 있어야 함
    //
    // 타입(몬스터/가디언)이나 분기 개수(2개 이상)로 거르면 구멍이 남는다.
    // 연결이 1개뿐인 방도 검사해야 한다 - 정원 4인 방이 정원 1인 방 하나로만 이어지면 3명이 갇힌다
    for (const FRoomNode& SourceNode : InMap)
    {
        // 보스방(마지막 레벨)은 나가는 길이 없는 게 정상
        if (SourceNode.ConnectedRoomIds.Num() == 0) continue;

        int32 CombinedCapacity = 0;
        for (int32 TargetId : SourceNode.ConnectedRoomIds)
        {
            if (const FRoomNode** TargetPtr = RoomLookup.Find(TargetId))
            {
                CombinedCapacity += (*TargetPtr)->MaxPlayers;
            }
        }

        // 이 방에 실제로 들어올 수 있는 최대 인원만큼은 빠져나갈 수 있어야 한다
        const int32 IncomingPlayers = FMath::Min(SourceNode.MaxPlayers, CurrentPlayerCount);

        if (CombinedCapacity < IncomingPlayers)
        {
            return false;
        }

        // 기획: 길이 2개 이상으로 갈라질 때는 정원 합이 "인원보다 커야" 한다.
        // 합이 인원과 같으면 분할이 한 가지로 강제되어 방을 고를 여지가 없어진다
        if (SourceNode.ConnectedRoomIds.Num() >= 2 && CombinedCapacity <= CurrentPlayerCount)
        {
            return false;
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

bool AMapManager::IsSinglePlayerRun() const
{
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;

    // GameState 가 아직 없으면 판단이 안 되니 멀티로 취급(전원 대기 쪽이 안전)
    return GS ? GS->PlayerArray.Num() <= 1 : false;
}

void AMapManager::EnterSelectedRooms(const TArray<ATerminusPlayerState*>& Players)
{
    if (!HasAuthority()) return;

    // 플레이어마다 다른 방일 수 있어서 ServerTravel(서버 전체 이동) 대신
    // 한 레벨 안의 구역을 방마다 하나씩 배정한다
    UDungeonAreaSubsystem* Areas = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>();
    if (Areas && Areas->HasAreas() && Areas->StartSelectedRooms(Players, Rooms))
    {
        return;
    }

    // 구역이 없는(또는 모자란) 레벨: 콘텐츠 없이 바로 클리어 처리. 구역 배치 전에도 지도 진행은 되게
    for (ATerminusPlayerState* PS : Players)
    {
        if (!PS) continue;

        const int32 SelectedId = PS->GetSelectedRoomId();
        const FRoomNode* Room = Rooms.FindByPredicate([SelectedId](const FRoomNode& Node) {
            return Node.RoomId == SelectedId;
        });

        if (!Room) continue;

        UE_LOG(LogTemp, Log, TEXT("[Map] 구역 없음 -> %s 즉시 %d번 방(Row %d, %s) 클리어 처리"),
            *PS->GetPlayerName(), Room->RoomId, Room->Row, *UEnum::GetValueAsString(Room->Type));

        PS->AdvanceToRoom(Room->RoomId, Room->Row);
    }
}

void AMapManager::CheckAllPlayersReadyAndStart()
{
    if (!HasAuthority()) return;

    const AGameStateBase* GS = GetWorld()->GetGameState();
    if (!GS) return;

    TArray<ATerminusPlayerState*> Players;
    int32 ReadyPlayers = 0;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
        {
            Players.Add(TPS);

            if (TPS->GetSelectedRoomId() != -1)
            {
                ReadyPlayers++;
            }
        }
    }

    if (Players.Num() == 0 || ReadyPlayers < Players.Num()) return;

    UE_LOG(LogTemp, Log, TEXT("[Map] 전원 방 선택 완료(%d명). 각자 고른 방으로 입장"), Players.Num());

    // 기획: "모든 플레이어가 선택을 완료 했을 경우, 각자가 선택한 방으로 입장하게 된다"
    EnterSelectedRooms(Players);
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

