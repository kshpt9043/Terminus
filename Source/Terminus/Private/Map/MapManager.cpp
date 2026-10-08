// Fill out your copyright notice in the Description page of Project Settings.


#include "Map/MapManager.h"
#include "Player/TerminusPlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Game/DungeonGameMode.h"
#include "Game/TerminusRunSubsystem.h"
#include "Dungeon/DungeonAreaSubsystem.h"
#include "Dungeon/DungeonThemeData.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Game/TerminusSaveSubsystem.h"
#include "TimerManager.h"

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

    // 런 시작 강화 스킬을 아직 안 골랐으면 출발 불가 (고르는 화면이 지도를 덮고 있지만 서버에서도 막음)
    if (!RequestingPS->HasChosenStartSkill() || !RequestingPS->HasChosenStartRelics())
    {
        Requester->Client_OnRoomSelectFailed(TEXT("시작 강화 스킬과 유물을 먼저 골라야 합니다."));
        return;
    }

    // 행선지 투표 중엔 방을 못 고름
    if (IsFloorVoteActive())
    {
        Requester->Client_OnRoomSelectFailed(TEXT("다음 행선지를 고르는 중입니다."));
        return;
    }

    // 이공간(누가 나가서 돌아오길 기다리는 중)에선 방을 못 고름
    if (const ADungeonGameMode* GM = GetWorld()->GetAuthGameMode<ADungeonGameMode>())
    {
        if (GM->HasDepartedPlayers())
        {
            Requester->Client_OnRoomSelectFailed(TEXT("나간 플레이어가 돌아올 때까지 방을 고를 수 없습니다."));
            return;
        }
    }

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
            if (Run->GetFloor() > 0)
            {
                CurrentFloor = Run->GetFloor();   // 세이브에서 이어하는 런 / 층을 넘어간 런
            }
            if (UDungeonThemeData* Saved = Cast<UDungeonThemeData>(Run->GetThemePath().TryLoad()))
            {
                FloorTheme = Saved;
            }
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

            // 첫 층 테마 (계층 후보가 있으면 거기서)
            if (UDungeonThemeData* Theme = PickThemeForFloor(CurrentFloor))
            {
                FloorTheme = Theme;
            }

            Rooms = GenerateMap();

            if (Run)
            {
                Run->SetRooms(Rooms);
                Run->SetFloor(CurrentFloor);
                Run->SetThemePath(FSoftObjectPath(FloorTheme.Get()));
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

    // 퀘스트 방은 테마가 시작되는 층에만 (기획: 테마에 들어갈 때 퀘스트를 받음). 없는 층은 그 줄을 빼고 일반 방부터
    const bool bQuestRow = HasQuestRoom(CurrentFloor);
    const int32 FirstNormalRow = bQuestRow ? 1 : 0;   // 일반 방이 시작되는 줄 (몬스터 고정)
    const int32 ValidTotalLevels = FMath::Max(2, bQuestRow ? TotalLevels : TotalLevels - 1);
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
        // 중간 레벨 범위: 몬스터 고정 줄 다음 ~ (LastRowIndex - 1)
        int32 MinMiddleRow = FirstNormalRow + 1;
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
        // [Pass 1] TotalLevels(Row 0 ~ LastRowIndex) 방 자리 생성
        //  타입은 여기서 정하지 않음 -> Pass 1.5 에서 지도 전체를 보고 고르게 나눔
        //  (방마다 따로 뽑으면 한쪽에 몰리거나 상점이 0개인 지도가 나옴)
        // ==========================================
        for (int32 Row = 0; Row < ValidTotalLevels; ++Row)
        {
            // 1. [1레벨 / Row 0] 퀘스트방 1개 고정 (퀘스트 방이 있는 층만)
            if (bQuestRow && Row == 0)
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
                // 가운데 열에. 위아래 줄의 방이 전부 이 방으로 모이는데, 끝 열에 있으면
                // 반대쪽 끝에서 지도를 가로지르는 긴 선이 생김 (지도에서도 가운데에 그려짐)
                const int32 MiddleCol = FMath::RandRange((ValidMaxRooms - 1) / 2, ValidMaxRooms / 2);

                // 타입 / 정원은 Pass 1.5 에서
                Map.Add(CreateRoom(GlobalRoomId++, Row, MiddleCol));
                continue;
            }

            // 4. [나머지 일반 레벨: 최소 2개 이상 방 생성 보장]
            // 첫 일반 줄(FirstNormalRow: 퀘스트 층은 Row 1, 아니면 Row 0)은 몬스터방 고정. 나머지 타입 / 정원은 Pass 1.5 에서
            TArray<FRoomNode> RowRooms;
            for (int32 Col = 0; Col < ValidMaxRooms; ++Col)
            {
                if (FMath::RandRange(0.0f, 1.0f) > 0.6f)
                {
                    RowRooms.Add(CreateRoom(GlobalRoomId++, Row, Col));
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
                    RowRooms.Add(CreateRoom(GlobalRoomId++, Row, RandomCol));
                }
            }

            Map.Append(RowRooms);
        }

        // ==========================================
        // [Pass 2] 레벨 간 연결(Edge) 구축. 선끼리 교차하지 않게
        //  (예전 방식은 열 차이 1 이하를 전부 이어서 4장 중 3장꼴로 선이 엇갈렸음)
        //  타입보다 먼저: 방 배치 규칙이 경로(앞뒤 방 / 갈림길)를 보고 정해지기 때문 (Slay the Spire 도 이 순서)
        //  연결은 열 위치만 보므로 타입 없이 만들 수 있음
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

            ConnectRowsWithoutCrossing(Map, CurrentRowIndices, NextRowIndices);
        }

        // ==========================================
        // [Pass 2.5] 방 타입 / 정원
        // ==========================================
        // 보스 직전 레벨은 전부 휴식터 (기획: UI 레퍼런스 "보스방 직전 방들은 휴식터"). 비율 배분에는 안 셈
        // 배분보다 먼저 정해야 함 -> 배분이 경로 규칙을 검사할 때 이 줄도 보게
        const int32 RestRow = LastRowIndex - 1;
        const bool bHasRestRow = RestRow >= FirstNormalRow + 1;   // 몬스터 고정 줄보다 짧은 지도면 생략

        if (bHasRestRow)
        {
            for (FRoomNode& Node : Map)
            {
                if (Node.Row == RestRow) Node.Type = ERoomType::BREAK;
            }
        }

        // 첫 일반 줄 다음 ~ 휴식터 줄 앞까지 비율대로 고르게 배분. 퀘스트 / 첫 일반 줄(몬스터) / 휴식터 줄 / 보스는 고정
        // 휴식터 줄 바로 앞 줄에는 휴식터 금지 (Slay the Spire 14층 규칙과 같음)
        DistributeRoomTypes(Map, FirstNormalRow + 1, bHasRestRow ? RestRow - 1 : LastRowIndex - 1, bHasRestRow ? RestRow - 1 : INDEX_NONE);

        for (FRoomNode& Node : Map)
        {
            // 퀘스트 / 보스는 생성할 때 전원 수용으로 이미 정함
            if ((bQuestRow && Node.Row == 0) || Node.Row == LastRowIndex) continue;

            // 방이 하나뿐인 층은 파티 전원이 여길 지나가야 함 -> 정원 = 전체 인원
            Node.MaxPlayers = ChosenSingleRoomRows.Contains(Node.Row)
                ? CurrentPlayerCount
                : GetRoomCapacity(Node.Type);
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

int32 AMapManager::GetLevelCount() const
{
    int32 MaxRow = INDEX_NONE;
    for (const FRoomNode& Node : Rooms)
    {
        MaxRow = FMath::Max(MaxRow, Node.Row);
    }
    return MaxRow >= 0 ? MaxRow + 1 : TotalLevels;
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

void AMapManager::ConnectRowsWithoutCrossing(TArray<FRoomNode>& Map, TArray<int32> Lower, TArray<int32> Upper)
{
    if (Lower.Num() == 0 || Upper.Num() == 0) return;

    // 두 줄을 열 순서로. 선 A(아래 a1 -> 위 b1), B(a2 -> b2) 는 a1 < a2 인데 b1 > b2 일 때만 교차한다
    auto ByCol = [&Map](int32 A, int32 B) { return Map[A].Col < Map[B].Col; };
    Lower.Sort(ByCol);
    Upper.Sort(ByCol);

    // ------------------------------------------------------------------
    // 1. 계단식 연결: 양쪽 맨 왼쪽에서 출발해 "아래만 다음 / 위만 다음 / 둘 다 다음" 중
    //    열 거리가 가까운 쪽으로 한 칸씩 나아가며 잇는다. 양쪽 맨 오른쪽에 닿으면 끝
    //    -> 순서가 뒤집히는 일이 없으니 교차가 없고, 모든 방이 위아래로 최소 하나씩 연결됨
    // ------------------------------------------------------------------
    constexpr float StepRandomness = 0.6f;   // 열 거리가 비슷한 후보 사이에서 매번 같은 모양이 안 나오게

    int32 i = 0, j = 0;
    const int32 LastI = Lower.Num() - 1;
    const int32 LastJ = Upper.Num() - 1;

    while (true)
    {
        Map[Lower[i]].ConnectedRoomIds.AddUnique(Map[Upper[j]].RoomId);
        if (i == LastI && j == LastJ) break;

        int32 BestI = i, BestJ = j;
        float BestScore = TNumericLimits<float>::Max();

        auto Consider = [&](int32 NI, int32 NJ)
        {
            const float Score = FMath::Abs(Map[Lower[NI]].Col - Map[Upper[NJ]].Col) + FMath::FRandRange(0.f, StepRandomness);
            if (Score < BestScore)
            {
                BestScore = Score;
                BestI = NI;
                BestJ = NJ;
            }
        };

        if (i < LastI) Consider(i + 1, j);
        if (j < LastJ) Consider(i, j + 1);
        if (i < LastI && j < LastJ) Consider(i + 1, j + 1);

        i = BestI;
        j = BestJ;
    }

    // ------------------------------------------------------------------
    // 2. 갈림길 보강: 열 차이 1 이하인 방끼리, 기존 선과 교차하지 않으면 전부 추가 (순서는 랜덤)
    //    계단식만 쓰면 갈림길이 29% 로 줄고 정원 조건 맞추기도 어려워짐 (시뮬레이션: 추가 후 39%)
    // ------------------------------------------------------------------
    TArray<TPair<int32, int32>> Candidates;
    for (int32 A : Lower)
    {
        for (int32 B : Upper)
        {
            if (FMath::Abs(Map[A].Col - Map[B].Col) <= 1 && !Map[A].ConnectedRoomIds.Contains(Map[B].RoomId))
            {
                Candidates.Add({ A, B });
            }
        }
    }

    for (int32 k = Candidates.Num() - 1; k > 0; --k)
    {
        Candidates.Swap(k, FMath::RandRange(0, k));
    }

    for (const TPair<int32, int32>& Cand : Candidates)
    {
        const int32 ACol = Map[Cand.Key].Col;
        const int32 BCol = Map[Cand.Value].Col;

        bool bCrosses = false;
        for (int32 Src : Lower)
        {
            for (int32 DstId : Map[Src].ConnectedRoomIds)
            {
                const int32* DstIdx = Upper.FindByPredicate([&Map, DstId](int32 U) { return Map[U].RoomId == DstId; });
                if (!DstIdx) continue;

                if ((Map[Src].Col - ACol) * (Map[*DstIdx].Col - BCol) < 0)
                {
                    bCrosses = true;
                    break;
                }
            }
            if (bCrosses) break;
        }

        if (!bCrosses)
        {
            Map[Cand.Key].ConnectedRoomIds.Add(Map[Cand.Value].RoomId);
        }
    }
}

void AMapManager::DistributeRoomTypes(TArray<FRoomNode>& Map, int32 FirstRow, int32 LastRow, int32 NoRestRow)
{
    // 대상 방들을 층 순서로 줄 세움. 같은 층 안에서는 섞음 (늘 왼쪽 방부터 특수방이 되지 않게)
    TArray<int32> Order;
    for (int32 i = 0; i < Map.Num(); ++i)
    {
        if (Map[i].Row >= FirstRow && Map[i].Row <= LastRow)
        {
            Order.Add(i);
        }
    }

    const int32 N = Order.Num();
    if (N == 0) return;

    for (int32 i = N - 1; i > 0; --i)
    {
        Order.Swap(i, FMath::RandRange(0, i));
    }
    Order.StableSort([&Map](int32 A, int32 B) { return Map[A].Row < Map[B].Row; });

    // ------------------------------------------------------------------
    // 1. 타입별 개수를 먼저 확정. 비율 × 방 수를 내림하고, 남는 칸은 소수부가 큰 타입부터
    //    예) 방 21개 -> 몬스터 12 / 이벤트 3 / 휴식 2 / 상점 2 / 가디언 2
    //    방마다 추첨하던 때는 상점 0개인 지도가 10% 가까이 나왔음
    // ------------------------------------------------------------------
    struct FQuota
    {
        ERoomType Type;
        int32 Count;
        float Remainder;
        float TieBreak;     // 소수부가 같은 타입끼리(10% 셋) 순서를 랜덤으로
    };

    float TotalWeight = 0.0f;
    for (const TPair<ERoomType, float>& Pair : RoomTypeWeights)
    {
        if (Pair.Value > 0.0f) TotalWeight += Pair.Value;
    }

    if (TotalWeight <= 0.0f)
    {
        for (int32 Idx : Order) Map[Idx].Type = ERoomType::MONSTER;
        return;
    }

    TArray<FQuota> Quotas;
    int32 Assigned = 0;
    for (const TPair<ERoomType, float>& Pair : RoomTypeWeights)
    {
        if (Pair.Value <= 0.0f) continue;

        const float Exact = N * Pair.Value / TotalWeight;
        const int32 Count = FMath::FloorToInt(Exact);
        Quotas.Add({ Pair.Key, Count, Exact - Count, FMath::FRand() });
        Assigned += Count;
    }

    Quotas.Sort([](const FQuota& A, const FQuota& B)
    {
        return A.Remainder != B.Remainder ? A.Remainder > B.Remainder : A.TieBreak > B.TieBreak;
    });

    for (int32 i = 0; Assigned < N; i = (i + 1) % Quotas.Num())
    {
        Quotas[i].Count++;
        Assigned++;
    }

    // ------------------------------------------------------------------
    // 2. 타입마다 등간격으로 목표 위치를 잡음. 시작점은 타입마다 랜덤
    //    - 등간격: 한 타입이 초반/후반 한쪽에 몰리지 않음
    //    - 타입별 랜덤 시작점: 비율이 같은 타입들(휴식/상점/가디언)이 같은 층에 뭉치지 않고,
    //      지도마다 위치가 달라짐 (시작점이 같으면 매번 4층, 8층에 상점이 나오는 식이 됨)
    //    - 약간의 흔들림: 너무 규칙적이지 않게. 크게 주면 다시 몰림 (시뮬레이션으로 0.15 선택)
    // ------------------------------------------------------------------
    constexpr float SpreadJitter = 0.15f;

    struct FTarget
    {
        float Position;
        float TieBreak;
        ERoomType Type;
    };

    TArray<FTarget> Targets;
    for (const FQuota& Q : Quotas)
    {
        if (Q.Count <= 0) continue;

        const float Spacing = static_cast<float>(N) / Q.Count;
        const float Offset = FMath::FRand();

        for (int32 k = 0; k < Q.Count; ++k)
        {
            const float Jitter = FMath::FRandRange(-SpreadJitter, SpreadJitter) * Spacing;
            Targets.Add({ (k + Offset) * Spacing + Jitter, FMath::FRand(), Q.Type });
        }
    }

    Targets.Sort([](const FTarget& A, const FTarget& B)
    {
        return A.Position != B.Position ? A.Position < B.Position : A.TieBreak < B.TieBreak;
    });

    // 3. 목표 위치 순서대로 아래층 방부터 배정 (개수 합 = 방 수라 딱 맞음)
    for (int32 i = 0; i < N; ++i)
    {
        Map[Order[i]].Type = Targets[i].Type;
    }

    // ------------------------------------------------------------------
    // 4. 경로 규칙 (Slay the Spire 와 같음. 초반 층 휴식/가디언 금지만 뺌)
    //    - 연속 금지: 휴식/상점/가디언은 바로 앞 방이나 바로 다음 방이 같은 종류면 안 됨
    //    - 갈림길 금지: 같은 방에서 갈라지는 방들끼리 휴식/상점/가디언/이벤트가 겹치면 안 됨
    //    - 보스 직전 휴식터 줄의 바로 앞 줄(NoRestRow)에는 휴식터 금지
    //    걸리는 특수방은 둬도 되는 가장 가까운 몬스터방과 맞바꿈
    //    지도 전체 그래프를 봄 -> 배분 대상 밖의 보스 직전 휴식터 줄도 연속 검사에 걸림
    // ------------------------------------------------------------------
    TMap<int32, int32> IndexById;
    for (int32 i = 0; i < Map.Num(); ++i)
    {
        IndexById.Add(Map[i].RoomId, i);
    }

    // 부모 = 나로 들어오는 방, 자식 = 내가 가는 방, 형제 = 같은 부모에서 갈라진 다른 방
    TArray<TArray<int32>> ParentsOf, ChildrenOf, SiblingsOf;
    ParentsOf.SetNum(Map.Num());
    ChildrenOf.SetNum(Map.Num());
    SiblingsOf.SetNum(Map.Num());

    for (int32 i = 0; i < Map.Num(); ++i)
    {
        for (const int32 ToId : Map[i].ConnectedRoomIds)
        {
            if (const int32* To = IndexById.Find(ToId))
            {
                ChildrenOf[i].Add(*To);
                ParentsOf[*To].Add(i);
            }
        }
    }

    for (int32 i = 0; i < Map.Num(); ++i)
    {
        for (const int32 P : ParentsOf[i])
        {
            for (const int32 C : ChildrenOf[P])
            {
                if (C != i) SiblingsOf[i].AddUnique(C);
            }
        }
    }

    auto IsNoRepeatType = [](ERoomType T)
    {
        return T == ERoomType::BREAK || T == ERoomType::STORE || T == ERoomType::GUARDIAN;
    };
    auto IsNoSiblingType = [](ERoomType T)
    {
        return T == ERoomType::BREAK || T == ERoomType::STORE || T == ERoomType::GUARDIAN || T == ERoomType::EVENT;
    };

    // At 자리에 Type 을 둬도 되는가. Ignore = 지금 옮기려는 방 자신 (자리를 비운다고 보고 검사에서 뺌)
    auto IsAllowed = [&](ERoomType Type, int32 At, int32 Ignore)
    {
        if (Type == ERoomType::MONSTER) return true;   // 몬스터는 빈자리 채우는 용도라 늘 허용 (Slay the Spire 와 같음)

        if (Type == ERoomType::BREAK && Map[At].Row == NoRestRow) return false;

        if (IsNoRepeatType(Type))
        {
            for (const int32 O : ParentsOf[At])  { if (O != Ignore && Map[O].Type == Type) return false; }
            for (const int32 O : ChildrenOf[At]) { if (O != Ignore && Map[O].Type == Type) return false; }
        }

        if (IsNoSiblingType(Type))
        {
            for (const int32 O : SiblingsOf[At]) { if (O != Ignore && Map[O].Type == Type) return false; }
        }

        return true;
    };

    for (int32 Pass = 0; Pass < 6; ++Pass)
    {
        bool bChanged = false;

        for (int32 Pos = 0; Pos < N; ++Pos)
        {
            const int32 RoomIdx = Order[Pos];
            const ERoomType Type = Map[RoomIdx].Type;
            const int32 Row = Map[RoomIdx].Row;
            if (IsAllowed(Type, RoomIdx, RoomIdx)) continue;

            // 이 타입을 둬도 되는 몬스터방 중 가장 가까운 층 (같은 층도 됨)
            TArray<int32> Candidates;
            int32 BestDist = MAX_int32;
            for (int32 CandIdx : Order)
            {
                const FRoomNode& Cand = Map[CandIdx];
                if (CandIdx == RoomIdx || Cand.Type != ERoomType::MONSTER) continue;
                if (!IsAllowed(Type, CandIdx, RoomIdx)) continue;

                const int32 Dist = FMath::Abs(Cand.Row - Row);
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    Candidates.Reset();
                }
                if (Dist == BestDist)
                {
                    Candidates.Add(CandIdx);
                }
            }

            if (Candidates.Num() > 0)
            {
                const int32 SwapIdx = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
                Map[SwapIdx].Type = Type;
            }
            else
            {
                // 둘 곳이 아예 없으면 몬스터방으로 (그 타입 개수가 하나 줄어듦). 규칙 위반보다 낫다
                UE_LOG(LogTemp, Log, TEXT("[MapGenerator] %s 를 규칙에 맞게 둘 곳이 없어 몬스터방으로 바꿈 (Row %d)"),
                    *UEnum::GetValueAsString(Type), Row);
            }

            Map[RoomIdx].Type = ERoomType::MONSTER;
            bChanged = true;
        }

        if (!bChanged) break;
    }

    // ------------------------------------------------------------------
    // 5. 갈림길에서 몬스터끼리 겹치는 것 줄이기 ("몬스터 vs 몬스터" 선택지)
    //    Slay the Spire 도 될 수 있으면 피하고 자리가 없으면 몬스터로 채움 -> 여기서도 가능할 때만
    //    겹친 몬스터방을 2줄 이내의 특수방과 맞바꿔 보고, 규칙을 안 깨면서 겹침이 줄 때만 바꿈
    //    (멀리서 끌어오면 위에서 고르게 퍼뜨린 분포가 무너짐. 시뮬레이션: 지도당 1.7쌍 -> 0.3쌍)
    // ------------------------------------------------------------------
    TSet<int32> InOrder(Order);

    auto CountMonsterSiblingPairs = [&]()
    {
        int32 Pairs = 0;
        for (const int32 Idx : Order)
        {
            if (Map[Idx].Type != ERoomType::MONSTER) continue;
            for (const int32 S : SiblingsOf[Idx])
            {
                if (S > Idx && InOrder.Contains(S) && Map[S].Type == ERoomType::MONSTER) ++Pairs;
            }
        }
        return Pairs;
    };

    constexpr int32 MaxSwapRowDistance = 2;

    for (int32 Pass = 0; Pass < 3; ++Pass)
    {
        bool bImproved = false;

        for (const int32 MonsterIdx : Order)
        {
            if (Map[MonsterIdx].Type != ERoomType::MONSTER) continue;

            const bool bHasMonsterSibling = SiblingsOf[MonsterIdx].ContainsByPredicate([&](int32 S)
            {
                return InOrder.Contains(S) && Map[S].Type == ERoomType::MONSTER;
            });
            if (!bHasMonsterSibling) continue;

            const int32 BasePairs = CountMonsterSiblingPairs();
            int32 BestIdx = INDEX_NONE;
            int32 BestGain = 0;
            int32 BestDist = MAX_int32;

            for (const int32 SpecialIdx : Order)
            {
                const ERoomType SpecialType = Map[SpecialIdx].Type;
                if (SpecialType == ERoomType::MONSTER) continue;

                const int32 Dist = FMath::Abs(Map[SpecialIdx].Row - Map[MonsterIdx].Row);
                if (Dist > MaxSwapRowDistance) continue;
                if (!IsAllowed(SpecialType, MonsterIdx, SpecialIdx)) continue;

                // 맞바꿔 보고 겹침이 얼마나 주는지
                Map[SpecialIdx].Type = ERoomType::MONSTER;
                Map[MonsterIdx].Type = SpecialType;
                const int32 Gain = BasePairs - CountMonsterSiblingPairs();
                Map[SpecialIdx].Type = SpecialType;
                Map[MonsterIdx].Type = ERoomType::MONSTER;

                if (Gain > BestGain || (Gain > 0 && Gain == BestGain && Dist < BestDist))
                {
                    BestIdx = SpecialIdx;
                    BestGain = Gain;
                    BestDist = Dist;
                }
            }

            if (BestIdx != INDEX_NONE)
            {
                Map[MonsterIdx].Type = Map[BestIdx].Type;
                Map[BestIdx].Type = ERoomType::MONSTER;
                bImproved = true;
            }
        }

        if (!bImproved) break;
    }
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

    // 멀티에서 누가 나가 혼자 남은 경우도 멀티 (나간 사람을 기다려야 함)
    if (const ADungeonGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADungeonGameMode>() : nullptr)
    {
        if (GM->HasDepartedPlayers()) return false;
    }

    // GameState 가 아직 없으면 판단이 안 되니 멀티로 취급(전원 대기 쪽이 안전)
    return GS ? GS->PlayerArray.Num() <= 1 : false;
}

void AMapManager::EnterSelectedRooms(const TArray<ATerminusPlayerState*>& Players)
{
    if (!HasAuthority()) return;

    // 플레이어마다 다른 방일 수 있어서 ServerTravel(서버 전체 이동) 대신
    // 한 레벨 안의 구역을 방마다 하나씩 배정한다
    UDungeonAreaSubsystem* Areas = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>();
    if (Areas && Areas->HasAreas() && Areas->StartSelectedRooms(Players, Rooms, FloorTheme))
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

    // 진행 중에 나간 사람이 돌아올 때까지는 출발하지 않음 (돌아와서 방을 고르면 그때 다시 여기로 옴)
    if (const ADungeonGameMode* GM = GetWorld()->GetAuthGameMode<ADungeonGameMode>())
    {
        if (GM->HasDepartedPlayers())
        {
            FChatMessage Notice;
            Notice.Kind = EChatMessageKind::System;
            Notice.Text = FString::Printf(TEXT("%s 님이 돌아올 때까지 출발할 수 없습니다."), *FString::Join(GM->GetDepartedNames(), TEXT(", ")));
            ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
            return;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Map] 전원 방 선택 완료(%d명). 각자 고른 방으로 입장"), Players.Num());

    // 기획: "모든 플레이어가 선택을 완료 했을 경우, 각자가 선택한 방으로 입장하게 된다"
    EnterSelectedRooms(Players);
}

// 네트워크 복제 속성 등록
void AMapManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMapManager, Rooms);
    DOREPLIFETIME(AMapManager, CurrentFloor);
    DOREPLIFETIME(AMapManager, FloorTheme);
    DOREPLIFETIME(AMapManager, VoteState);
}

// =====================================================================
// 층 진행
// =====================================================================

UDungeonThemeData* AMapManager::PickThemeForFloor(int32 Floor) const
{
    const int32 Tier = (FMath::Max(1, Floor) - 1) / 2;
    const int32 PrevTier = (FMath::Max(1, Floor - 1) - 1) / 2;

    // 같은 계층의 두 번째 층이면 지금 테마 그대로
    if (Floor > 1 && Tier == PrevTier && FloorTheme)
    {
        return FloorTheme;
    }

    if (TierThemes.IsValidIndex(Tier))
    {
        TArray<UDungeonThemeData*> Candidates;
        for (UDungeonThemeData* Theme : TierThemes[Tier].Themes)
        {
            if (Theme) Candidates.Add(Theme);
        }
        if (Candidates.Num() > 0)
        {
            return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
        }
    }

    // 후보가 없으면 지금 테마 (레벨에 지정한 것)
    return FloorTheme;
}

bool AMapManager::HandleBossCleared()
{
    if (!HasAuthority()) return false;

    UE_LOG(LogTemp, Log, TEXT("[Map] %d층 보스 클리어"), CurrentFloor);

    // 테마가 바뀌는 층(2 / 4 / 6층)에서만 행선지를 고름. 나머지는 바로 다음 층
    if (IsThemeEndFloor(CurrentFloor))
    {
        StartFloorVote();
        return false;
    }

    AdvanceFloor();
    return true;
}

// =====================================================================
// 테마 끝 층 선택 (투표)
// =====================================================================

void AMapManager::StartFloorVote()
{
    const AGameStateBase* GS = GetWorld()->GetGameState();
    const bool bSingle = IsSinglePlayerRun();

    FloorVotes.Reset();
    const int32 NextId = VoteState.VoteId + 1;
    VoteState = FFloorVoteState();
    VoteState.VoteId = NextId;
    VoteState.Phase = EFloorVotePhase::Voting;
    VoteState.Floor = CurrentFloor;

    // 마지막 층(콘텐츠 끝)이면 다음 층 없음. 배신은 멀티만
    if (CurrentFloor < LastFloor) VoteState.Options.Add(EFloorChoice::NextFloor);
    VoteState.Options.Add(EFloorChoice::Escape);
    if (!bSingle) VoteState.Options.Add(EFloorChoice::Betray);

    VoteState.Counts.SetNumZeroed(VoteState.Options.Num());
    VoteState.TotalVoters = GS ? FMath::Max(1, GS->PlayerArray.Num()) : 1;
    VoteState.PhaseEndTime = (GS ? GS->GetServerWorldTimeSeconds() : 0.f) + VoteSeconds;

    GetWorldTimerManager().SetTimer(VoteTimer, this, &AMapManager::EndFloorVoting, VoteSeconds, false);

    FChatMessage Notice;
    Notice.Kind = EChatMessageKind::System;
    Notice.Text = bSingle
        ? FString::Printf(TEXT("%d층 보스를 쓰러뜨렸습니다. 다음 행선지를 고르세요."), CurrentFloor)
        : FString::Printf(TEXT("%d층 보스를 쓰러뜨렸습니다. %d초 동안 다음 행선지를 투표합니다."), CurrentFloor, FMath::RoundToInt(VoteSeconds));
    ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);

    UE_LOG(LogTemp, Log, TEXT("[Map] %d층 행선지 투표 시작 (선택지 %d개, %d명)"), CurrentFloor, VoteState.Options.Num(), VoteState.TotalVoters);
    NotifyVoteChanged();
}

bool AMapManager::CastFloorVote(APlayerState* Voter, EFloorChoice Choice)
{
    if (!HasAuthority() || !Voter || VoteState.Phase != EFloorVotePhase::Voting) return false;
    if (FloorVotes.Contains(Voter)) return false;   // 한 번만

    const int32 Index = VoteState.Options.IndexOfByKey(Choice);
    if (Index == INDEX_NONE) return false;

    // 배신은 선착순 1명
    if (Choice == EFloorChoice::Betray && VoteState.Counts[Index] > 0) return false;

    FloorVotes.Add(Voter, Choice);
    ++VoteState.Counts[Index];
    ++VoteState.VotedCount;

    UE_LOG(LogTemp, Log, TEXT("[Map] 투표 %d / %d"), VoteState.VotedCount, VoteState.TotalVoters);

    // 배신이 나오면 바로 끝 (그 1명 vs 나머지), 전원 골랐으면 공개로
    if (Choice == EFloorChoice::Betray || VoteState.VotedCount >= VoteState.TotalVoters)
    {
        EndFloorVoting();
    }
    else
    {
        NotifyVoteChanged();
    }
    return true;
}

void AMapManager::EndFloorVoting()
{
    if (VoteState.Phase != EFloorVotePhase::Voting) return;
    GetWorldTimerManager().ClearTimer(VoteTimer);

    // 혼자면 공개할 게 없음
    if (VoteState.TotalVoters <= 1)
    {
        ResolveFloorVote();
        return;
    }

    const AGameStateBase* GS = GetWorld()->GetGameState();
    VoteState.Phase = EFloorVotePhase::Revealing;
    VoteState.PhaseEndTime = (GS ? GS->GetServerWorldTimeSeconds() : 0.f) + RevealSeconds;
    GetWorldTimerManager().SetTimer(VoteTimer, this, &AMapManager::ResolveFloorVote, RevealSeconds, false);
    NotifyVoteChanged();
}

void AMapManager::ResolveFloorVote()
{
    GetWorldTimerManager().ClearTimer(VoteTimer);

    // 누가 무엇을 골랐는지 공개
    VoteState.RevealedVotes.Reset();
    bool bBetrayed = false;
    for (const TPair<TWeakObjectPtr<APlayerState>, EFloorChoice>& Pair : FloorVotes)
    {
        if (APlayerState* PS = Pair.Key.Get())
        {
            FFloorVoteEntry& Entry = VoteState.RevealedVotes.AddDefaulted_GetRef();
            Entry.Player = PS;
            Entry.Choice = Pair.Value;
        }
        bBetrayed |= Pair.Value == EFloorChoice::Betray;
    }

    // 결과: 배신이 있으면 배신. 아니면 다수결, 동점이면 그중 랜덤 (아무도 안 골랐으면 배신 빼고 랜덤)
    EFloorChoice Result = EFloorChoice::None;
    if (bBetrayed)
    {
        Result = EFloorChoice::Betray;
    }
    else
    {
        int32 Best = -1;
        TArray<EFloorChoice> Tied;
        for (int32 i = 0; i < VoteState.Options.Num(); ++i)
        {
            if (VoteState.Options[i] == EFloorChoice::Betray) continue;

            const int32 Count = VoteState.Counts.IsValidIndex(i) ? VoteState.Counts[i] : 0;
            if (Count > Best)
            {
                Best = Count;
                Tied.Reset();
            }
            if (Count == Best)
            {
                Tied.Add(VoteState.Options[i]);
            }
        }
        if (Tied.Num() > 0)
        {
            Result = Tied[FMath::RandRange(0, Tied.Num() - 1)];
        }
    }

    const AGameStateBase* GS = GetWorld()->GetGameState();
    VoteState.Result = Result;
    VoteState.Phase = EFloorVotePhase::Done;
    VoteState.PhaseEndTime = (GS ? GS->GetServerWorldTimeSeconds() : 0.f) + ResultSeconds;
    NotifyVoteChanged();

    UE_LOG(LogTemp, Log, TEXT("[Map] 투표 결과: %s"), *UEnum::GetValueAsString(Result));

    // 결과를 잠깐 보여주고 적용 (혼자면 바로)
    const float Delay = VoteState.TotalVoters <= 1 ? 0.5f : ResultSeconds;
    GetWorldTimerManager().SetTimer(VoteTimer, this, &AMapManager::ApplyFloorChoice, Delay, false);
}

void AMapManager::ApplyFloorChoice()
{
    const EFloorChoice Result = VoteState.Result;

    // 투표 화면 닫기
    VoteState.Phase = EFloorVotePhase::None;
    NotifyVoteChanged();

    switch (Result)
    {
    case EFloorChoice::NextFloor:
        AdvanceFloor();
        break;

    case EFloorChoice::Betray:
        // TODO: 배신 전투 (배신자 1명 vs 나머지). 아직 없어서 탈출로 처리
        EndRunByEscape(TEXT("배신이 선택되었습니다. (배신 전투는 준비 중이라 탈출로 처리합니다)"));
        break;

    case EFloorChoice::Escape:
    default:
        EndRunByEscape(TEXT("던전을 탈출했습니다."));
        break;
    }
}

void AMapManager::NotifyVoteChanged()
{
    // 리슨 서버 자신은 OnRep 이 안 불려서 직접
    OnRep_VoteState();
}

void AMapManager::OnRep_VoteState()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(It->Get());
        if (PC && PC->IsLocalController())
        {
            PC->UpdateFloorVote(VoteState);
        }
    }
}

void AMapManager::EndRunByEscape(const FString& Reason)
{
    EndRun(Reason, false);
}

void AMapManager::EndRunByDeath(const FString& Reason)
{
    EndRun(Reason, true);
}

void AMapManager::EndRun(const FString& Reason, bool bDeath)
{
    // 런이 끝남 -> 세이브 삭제 (정산 내용은 각자 프로필에 따로 저장되니 여기서 바로 지워도 됨)
    if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
    {
        if (const UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
        {
            Save->DeleteRunSave(Run->GetSaveSlot());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Map] 런 종료: %s"), *Reason);

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(It->Get()))
        {
            PC->Client_BeginSettlement(FText::FromString(Reason), bDeath);
        }
    }
}

void AMapManager::AdvanceFloor()
{
    if (!HasAuthority()) return;

    ++CurrentFloor;
    if (UDungeonThemeData* Theme = PickThemeForFloor(CurrentFloor))
    {
        FloorTheme = Theme;
    }

    // 새 지도 (인원은 이 런 그대로)
    Rooms = GenerateMap();

    if (UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
    {
        Run->SetRooms(Rooms);
        Run->SetFloor(CurrentFloor);
        Run->SetThemePath(FSoftObjectPath(FloorTheme.Get()));
    }

    // 전원: 지도 처음으로 + 체력 전부 회복
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
            {
                TPS->BeginFloor();
            }
        }
    }

    // 리슨 서버 자신의 지도 화면 (손님은 Rooms 복제 -> OnRep_Rooms)
    OnMapGenerated.Broadcast(Rooms);

    // 층 도착 화면
    const FText Title = FText::FromString(FString::Printf(TEXT("%d층"), CurrentFloor));
    FString Sub = GetTierName(CurrentFloor).ToString();
    if (FloorTheme && !FloorTheme->DisplayName.IsEmpty())
    {
        Sub += FString::Printf(TEXT(" · %s"), *FloorTheme->DisplayName.ToString());
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(It->Get()))
        {
            PC->Client_ShowFloorTitle(Title, FText::FromString(Sub));
        }
    }

    FChatMessage Notice;
    Notice.Kind = EChatMessageKind::System;
    Notice.Text = FString::Printf(TEXT("%d층(%s)에 도착했습니다. 체력이 모두 회복되었습니다."), CurrentFloor, *Sub);
    ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);

    UE_LOG(LogTemp, Log, TEXT("[Map] %d층으로 (%s), 방 %d개"), CurrentFloor, *Sub, Rooms.Num());

    // 새 층에서 이어하게 저장
    if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
    {
        Save->SaveCurrentRun(GetWorld());
    }
}

FText AMapManager::GetTierName(int32 Floor)
{
    // 테마 하나가 2개 층
    switch ((FMath::Max(1, Floor) - 1) / 2)
    {
    case 0:  return FText::FromString(TEXT("표층"));
    case 1:  return FText::FromString(TEXT("중층"));
    default: return FText::FromString(TEXT("심층"));
    }
}

// ★ 클라이언트가 서버로부터 Rooms 데이터 수신을 완료했을 때 실행됨
void AMapManager::OnRep_Rooms()
{
    UE_LOG(LogTemp, Warning, TEXT("Map Generated!"))
    // 데이터가 수신되었으므로 클라이언트 UI에 맵을 그리라고 알림
    OnMapGenerated.Broadcast(Rooms);
}

