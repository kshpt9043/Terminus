// Fill out your copyright notice in the Description page of Project Settings.


#include "Map/MapGenerator.h"

#include "Net/UnrealNetwork.h"

AMapGenerator::AMapGenerator()
{ 
    PrimaryActorTick.bCanEverTick = false;

    // ★ 네트워크 복제(Replication) 활성화
    bReplicates = true;
    bAlwaysRelevant = true; // 모든 클라이언트에 항상 데이터 전송되도록 설정
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
    const int32 MaxRetries = 100; // 안전장치 (최대 재시도 횟수)

    while (!bIsValidMap && RetryCount < MaxRetries)
    {
        RetryCount++;
        Map.Empty();
        int32 GlobalRoomId = 1;

        // ==========================================
        // [Pass 1] 방 생성
        // ==========================================
        for (int32 Row = 0; Row < 10; ++Row)
        {
            if (Row == 9)
            {
                Map.Add(CreateRoom(GlobalRoomId++, Row, 3, ERoomType::BOSS));
                continue;
            }

            int32 CreatedCountInRow = 0;
            for (int32 Col = 0; Col < 6; ++Col)
            {
                if (FMath::RandRange(0.0f, 1.0f) > 0.6f)
                {
                    ERoomType RoomType = (Row == 0) ? ERoomType::MONSTER : GetRandomNormalRoomType();
                    Map.Add(CreateRoom(GlobalRoomId++, Row, Col, RoomType));
                    CreatedCountInRow++;
                }
            }

            if (CreatedCountInRow == 0)
            {
                int32 RandomCol = FMath::RandRange(0, 5);
                ERoomType RoomType = (Row == 0) ? ERoomType::MONSTER : GetRandomNormalRoomType();
                Map.Add(CreateRoom(GlobalRoomId++, Row, RandomCol, RoomType));
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
        // [Pass 3] 경로 검증 (Validation)
        // ==========================================
        bIsValidMap = ValidatePathToBoss(Map);

        if (!bIsValidMap)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MapGenerator] Invalid Map generated! Retrying... (%d)"), RetryCount);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[MapGenerator] Map Generation Completed in %d attempt(s). Total Rooms: %d"), RetryCount, Map.Num());

    return Map;
}

FRoomNode AMapGenerator::CreateRoom(int32 RoomId, int32 Row, int32 Col, ERoomType Type)
{
    FRoomNode Room;
    Room.RoomId = RoomId;
    Room.Row = Row;
    Room.Col = Col;
    Room.Type = Type;
    return Room;
}

// BOSS(5)를 제외한 일반 방 타입(0~4) 랜덤 반환
ERoomType AMapGenerator::GetRandomNormalRoomType()
{
    uint8 RandomIndex = static_cast<uint8>(FMath::RandRange(0, 4));
    return static_cast<ERoomType>(RandomIndex);
}

bool AMapGenerator::ValidatePathToBoss(const TArray<FRoomNode>& InMap)
{
    if (InMap.Num() == 0) return false;

    // RoomId를 Key로 하여 빠르게 Node 데이터를 찾기 위한 Map 생성
    TMap<int32, const FRoomNode*> RoomLookup;
    for (const FRoomNode& Node : InMap)
    {
        RoomLookup.Add(Node.RoomId, &Node);
    }

    // 1. Queue와 방문 여부(Visited) 집합 준비
    TQueue<int32> SearchQueue;
    TSet<int32> VisitedRoomIds;

    // 2. 1레벨(Row == 0)의 모든 시작 방들을 큐에 삽입
    for (const FRoomNode& Node : InMap)
    {
        if (Node.Row == 0)
        {
            SearchQueue.Enqueue(Node.RoomId);
            VisitedRoomIds.Add(Node.RoomId);
        }
    }

    // 3. BFS (너비 우선 탐색) 수행
    while (!SearchQueue.IsEmpty())
    {
        int32 CurrentRoomId;
        SearchQueue.Dequeue(CurrentRoomId);

        const FRoomNode** CurrentNodePtr = RoomLookup.Find(CurrentRoomId);
        if (!CurrentNodePtr || !(*CurrentNodePtr)) continue;

        const FRoomNode* CurrentNode = *CurrentNodePtr;

        // 보스방(Row == 9 또는 ERoomType::BOSS)에 도달했다면 검증 성공!
        if (CurrentNode->Type == ERoomType::BOSS || CurrentNode->Row == 9)
        {
            return true; // 경로 존재 확인!
        }

        // 연결된 다음 방들을 큐에 방문 추가
        for (int32 ConnectedId : CurrentNode->ConnectedRoomIds)
        {
            if (!VisitedRoomIds.Contains(ConnectedId))
            {
                VisitedRoomIds.Add(ConnectedId);
                SearchQueue.Enqueue(ConnectedId);
            }
        }
    }

    // 큐를 다 돌 때까지 보스방에 도달하지 못했다면 연결 실패
    return false;
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

