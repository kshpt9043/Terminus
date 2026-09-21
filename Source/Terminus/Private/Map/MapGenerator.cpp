// Fill out your copyright notice in the Description page of Project Settings.


#include "Map/MapGenerator.h"

#include "Net/UnrealNetwork.h"

AMapGenerator::AMapGenerator()
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

void AMapGenerator::BeginPlay()
{
    Super::BeginPlay();
    
    // ★ 오직 서버(Authority)에서만 맵을 생성합니다.
    if (HasAuthority())
    {
        Rooms = GenerateMap();

        // 서버 자신(Listen Server)의 UI 업데이트를 위해 델리게이트 알림
        OnMapGenerated.Broadcast(Rooms);
    }
}

TArray<FRoomNode> AMapGenerator::GenerateMap()
{
    TArray<FRoomNode> Map;
    bool bIsValidMap = false;
    int32 RetryCount = 0;
    const int32 MaxRetries = 200;

    while (!bIsValidMap && RetryCount < MaxRetries)
    {
        RetryCount++;
        Map.Empty();
        int32 GlobalRoomId = 1;

        // ==========================================
        // [Pass 1] 모든 레벨의 방 생성 및 입장 인원 무작위 지정
        // ==========================================
        for (int32 Row = 0; Row < 10; ++Row)
        {
            // 10레벨: 보스방 (보스방은 전원 입장 가능하도록 설정)
            if (Row == 9)
            {
                Map.Add(CreateRoom(GlobalRoomId++, Row, 3, ERoomType::BOSS, CurrentPlayerCount));
                continue;
            }

            int32 CreatedCountInRow = 0;

            for (int32 Col = 0; Col < 6; ++Col)
            {
                if (FMath::RandRange(0.0f, 1.0f) > 0.6f)
                {
                    // 1레벨(Row 0)은 몬스터 방 고정, 그 외는 비율 기반 랜덤
                    ERoomType RoomType = (Row == 0) ? ERoomType::MONSTER : GetWeightedRandomRoomType();
                    
                    // 입장 수용 인원 (1명 ~ CurrentPlayerCount 사이)
                    int32 RandomMaxPlayers = FMath::RandRange(1, CurrentPlayerCount);
                    
                    Map.Add(CreateRoom(GlobalRoomId++, Row, Col, RoomType, RandomMaxPlayers));
                    CreatedCountInRow++;
                }
            }

            // 방이 0개 생성되는 것 방지
            if (CreatedCountInRow == 0)
            {
                int32 RandomCol = FMath::RandRange(0, 5);
                ERoomType RoomType = (Row == 0) ? ERoomType::MONSTER : GetWeightedRandomRoomType();
                int32 RandomMaxPlayers = FMath::RandRange(1, CurrentPlayerCount);

                Map.Add(CreateRoom(GlobalRoomId++, Row, RandomCol, RoomType, RandomMaxPlayers));
            }
        }

        // ==========================================
        // [Pass 2] 연결(Edge) 구축
        // ==========================================
        for (int32 Row = 0; Row < 9; ++Row)
        {
            TArray<int32> CurrentRowIndices;
            TArray<int32> NextRowIndices;

            for (int32 i = 0; i < Map.Num(); ++i)
            {
                if (Map[i].Row == Row) CurrentRowIndices.Add(i);
                else if (Map[i].Row == Row + 1) NextRowIndices.Add(i);
            }

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

            // 고립 방 역방향 연결
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
        // [Pass 3] 맵 통과 가능성 및 기획 조건 검증
        // ==========================================
        bool bPathValid = ValidatePathToBoss(Map);
        bool bCapacityValid = ValidatePlayerCapacity(Map);

        bIsValidMap = bPathValid && bCapacityValid;
    }

    UE_LOG(LogTemp, Log, TEXT("[MapGenerator] Map Generated after %d Retries."), RetryCount);
    return Map;
}

FRoomNode AMapGenerator::CreateRoom(int32 RoomId, int32 Row, int32 Col, ERoomType Type, int32 MaxPlayers)
{
    FRoomNode Room;
    Room.RoomId = RoomId;
    Room.Row = Row;
    Room.Col = Col;
    Room.Type = Type;
    Room.MaxPlayers = MaxPlayers;
    return Room;
}

// 가중치 확률 추첨
ERoomType AMapGenerator::GetWeightedRandomRoomType()
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

// BFS를 통한 보스방 도달 경로 검증
bool AMapGenerator::ValidatePathToBoss(const TArray<FRoomNode>& InMap)
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

        if (CurrentNode->Type == ERoomType::BOSS || CurrentNode->Row == 9)
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

// 몬스터/가디언 방에서 2개 이상으로 갈라질 때 인원수 합산 조건 검증
bool AMapGenerator::ValidatePlayerCapacity(const TArray<FRoomNode>& InMap)
{
    TMap<int32, const FRoomNode*> RoomLookup;
    for (const FRoomNode& Node : InMap)
    {
        RoomLookup.Add(Node.RoomId, &Node);
    }

    for (const FRoomNode& SourceNode : InMap)
    {
        // 몬스터 방 또는 가디언 방일 때 검사
        if (SourceNode.Type == ERoomType::MONSTER || SourceNode.Type == ERoomType::GUARDIAN)
        {
            // 2개 이상의 방으로 갈라지는 경우
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

                // 갈라진 방들의 수용 인원 합이 현재 전체 플레이어 인원보다 적으면 검증 실패
                if (CombinedCapacity < CurrentPlayerCount)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

// 네트워크 복제 속성 등록
void AMapGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMapGenerator, Rooms);
}

// ★ 클라이언트가 서버로부터 Rooms 데이터 수신을 완료했을 때 실행됨
void AMapGenerator::OnRep_Rooms()
{
    // 데이터가 수신되었으므로 클라이언트 UI에 맵을 그리라고 알림
    OnMapGenerated.Broadcast(Rooms);
}

