// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/MapCanvasWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Map/RoomNodeWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "GameFramework/GameStateBase.h"
#include "Player/TerminusPlayerController.h"
#include "TimerManager.h"
#include "EngineUtils.h"

void UMapCanvasWidget::NativeConstruct()
{
	Super::NativeConstruct();

    // 지도(캔버스)를 가로 가운데로. 지도 폭은 고정이라 화면이 넓어져도 가운데 유지
    // 지금 WBP 는 ScrollBox > Overlay > MapCanvasPanel 이라 Overlay 슬롯. 바로 ScrollBox 아래여도 되게 둘 다 처리
    if (MapCanvasPanel)
    {
        if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(MapCanvasPanel->Slot))
        {
            ScrollSlot->SetHorizontalAlignment(HAlign_Center);
        }
        else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(MapCanvasPanel->Slot))
        {
            OverlaySlot->SetHorizontalAlignment(HAlign_Center);
        }
    }

    // MapManager / PlayerState 바인딩. 클라에선 둘 다 복제가 위젯 생성보다 늦을 수 있어서
    // 지금 한 번 + 0.5초마다 다시 훑어서 새로 도착한 것을 묶음 (인원 변동도 같이 커버)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            BindTimer, this, &UMapCanvasWidget::TryBindPlayerStates,
            0.5f,   // 주기
            true,   // 반복
            0.f);   // 첫 호출은 바로
    }
}

void UMapCanvasWidget::NativeDestruct()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(BindTimer);

        // 위젯이 치워진 뒤에도 PS / MapManager 가 계속 이 위젯을 부르지 않게 풀어줌
        if (const AGameStateBase* GS = World->GetGameState())
        {
            for (APlayerState* PS : GS->PlayerArray)
            {
                if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
                {
                    TPS->OnRunStateChanged.RemoveAll(this);
                }
            }
        }

        if (AMapManager* MapGen = BoundMapManager.Get())
        {
            MapGen->OnMapGenerated.RemoveAll(this);
        }
        BoundMapManager.Reset();
    }

    Super::NativeDestruct();
}

AMapManager* UMapCanvasWidget::FindMapManager() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    const bool bIsClient = World->GetNetMode() == NM_Client;

    for (TActorIterator<AMapManager> It(World); It; ++It)
    {
        AMapManager* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed()) continue;

        // 클라에서 Authority 를 가진 MapManager = 클라가 직접 스폰한 로컬 액터 -> 서버 지도와 다름
        if (bIsClient && Candidate->GetLocalRole() == ROLE_Authority) continue;

        return Candidate;
    }
    return nullptr;
}

void UMapCanvasWidget::TryBindPlayerStates()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // MapManager: 아직 못 묶었으면 찾아서 묶음
    if (!BoundMapManager.IsValid())
    {
        if (AMapManager* MapGen = FindMapManager())
        {
            BoundMapManager = MapGen;
            MapGen->OnMapGenerated.AddUniqueDynamic(this, &UMapCanvasWidget::BuildMapUI);

            // 지도 데이터가 이미 와 있으면 바로 그리기 (OnRep 가 위젯 생성보다 먼저 끝난 경우)
            if (MapGen->Rooms.Num() > 0)
            {
                BuildMapUI(MapGen->Rooms);
            }
        }
    }

    ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;

    // 내 PS: 도착하면 묶고 방 상태를 갱신
    // (도착 전에 그린 지도는 현재 위치를 몰라서 Row 0 만 켜져 있음)
    if (LocalPS && !LocalPS->OnRunStateChanged.IsAlreadyBound(this, &UMapCanvasWidget::OnPlayerRunStateChanged))
    {
        // 내 PS 가 늦게 확인돼서 먼저 "다른 사람" 으로 묶였을 수 있음 -> 그 바인딩은 풀기
        LocalPS->OnRunStateChanged.RemoveDynamic(this, &UMapCanvasWidget::OnOtherRunStateChanged);
        LocalPS->OnRunStateChanged.AddDynamic(this, &UMapCanvasWidget::OnPlayerRunStateChanged);

        RefreshRoomStates();
    }

    // 다른 사람 PS: 선택 표시만 갱신하면 됨
    const AGameStateBase* GS = World->GetGameState();
    if (!GS) return;

    bool bNewlyBound = false;
    for (APlayerState* PS : GS->PlayerArray)
    {
        ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS);
        if (!TPS || TPS == LocalPS) continue;

        if (!TPS->OnRunStateChanged.IsAlreadyBound(this, &UMapCanvasWidget::OnOtherRunStateChanged))
        {
            TPS->OnRunStateChanged.AddDynamic(this, &UMapCanvasWidget::OnOtherRunStateChanged);
            bNewlyBound = true;
        }
    }

    // 새로 들어온 사람이 이미 방을 골라둔 상태일 수 있으니 한 번 갱신
    if (bNewlyBound)
    {
        RefreshAllRoomSelections();
    }
}

bool UMapCanvasWidget::IsSameMap(const TArray<FRoomNode>& A, const TArray<FRoomNode>& B)
{
    if (A.Num() != B.Num()) return false;

    // 복제된 배열은 서버와 순서가 같으므로 인덱스끼리 비교하면 됨
    for (int32 i = 0; i < A.Num(); ++i)
    {
        const FRoomNode& L = A[i];
        const FRoomNode& R = B[i];

        if (L.RoomId != R.RoomId || L.Row != R.Row || L.Col != R.Col || L.Type != R.Type
            || L.MaxPlayers != R.MaxPlayers || L.ConnectedRoomIds != R.ConnectedRoomIds)
        {
            return false;
        }
    }
    return true;
}

bool UMapCanvasWidget::IsRoomSelectable(const FRoomNode& Node, const ATerminusPlayerState* LocalPS) const
{
    // LocalPS 가 아직 도착 안 했거나, 1레벨(CurrentMapLevel == 0)일 때는 Row 0 (퀘스트방)만
    if (!LocalPS || LocalPS->GetCurrentMapLevel() == 0)
    {
        return Node.Row == 0;
    }

    // 2레벨 이후: 현재 방과 연결된 방만
    const int32 CurrentRoomId = LocalPS->GetCurrentRoomId();
    const FRoomNode* CurrentRoom = CachedMapData.FindByPredicate([CurrentRoomId](const FRoomNode& N) {
        return N.RoomId == CurrentRoomId;
    });

    return CurrentRoom && CurrentRoom->ConnectedRoomIds.Contains(Node.RoomId);
}

void UMapCanvasWidget::RefreshRoomStates()
{
    ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;

    // 위젯은 그대로 두고 상태만 넘김. 각 방 위젯이 자기 상태가 바뀌었을 때만 다시 그림
    for (const FRoomNode& Node : CachedMapData)
    {
        if (URoomNodeWidget** RoomWidget = CreatedRoomWidgets.Find(Node.RoomId))
        {
            if (*RoomWidget)
            {
                (*RoomWidget)->SetRoomSelectable(IsRoomSelectable(Node, LocalPS));
            }
        }
    }

    RefreshAllRoomSelections();
    RefreshEdges(LocalPS);
}

void UMapCanvasWidget::ComputeLayout(const TArray<FRoomNode>& MapData)
{
    RoomPositions.Reset();

    int32 MaxRow = 0;
    int32 MaxCol = 0;
    TMap<int32, TArray<const FRoomNode*>> RowRooms;
    for (const FRoomNode& Node : MapData)
    {
        MaxRow = FMath::Max(MaxRow, Node.Row);
        MaxCol = FMath::Max(MaxCol, Node.Col);
        RowRooms.FindOrAdd(Node.Row).Add(&Node);
    }

    // 격자 폭 / 높이. 방 중심이 놓이는 범위
    const float GridWidth = MaxCol * ColumnSpacing;
    const float GridHeight = MaxRow * RowSpacing;

    // 흔들림 한도: 이웃 방과 아이콘이 겹치지 않을 만큼만 (양쪽이 서로 다가와도 틈이 남게)
    const float JitterMaxX = FMath::Min(PositionJitter.X * ColumnSpacing, FMath::Max(0.f, (ColumnSpacing - RoomIconVisibleSize) * 0.5f - 10.f));
    const float JitterMaxY = FMath::Min(PositionJitter.Y * RowSpacing, FMath::Max(0.f, (RowSpacing - RoomIconVisibleSize) * 0.5f - 20.f));

    // 여백: 끝 열 방이 흔들려 나가도 아이콘 상자가 안 잘리게
    const FVector2D EdgePadding(
        FMath::Max(MapPadding.X, RoomIconCenter.X + JitterMaxX + 20.f),
        FMath::Max(MapPadding.Y, RoomIconCenter.Y + JitterMaxY + 20.f));

    MapContentSize = FVector2D(GridWidth, GridHeight) + EdgePadding * 2.f;

    for (TPair<int32, TArray<const FRoomNode*>>& Pair : RowRooms)
    {
        const int32 Row = Pair.Key;
        TArray<const FRoomNode*>& Rooms = Pair.Value;
        Rooms.Sort([](const FRoomNode& A, const FRoomNode& B) { return A.Col < B.Col; });
        const int32 Count = Rooms.Num();

        // 퀘스트(맨 아래) / 보스(맨 위) 는 지도 한가운데, 흔들림 없음
        const bool bAnchorRow = (Row == 0 || Row == MaxRow);

        // 방이 하나뿐인 줄도 가로는 한가운데 (위아래 방이 전부 모이는 자리라 끝에 있으면 선이 길게 가로지름)
        const bool bCenterX = bAnchorRow || Count == 1;

        // 이 줄을 가운데 기준으로 고르게 펼쳤을 때의 폭. 방 수에 비례 (4개면 전체 폭, 2개면 한 칸)
        const float SpreadSpan = (MaxCol > 0 && Count > 1) ? GridWidth * (Count - 1) / MaxCol : 0.f;

        for (int32 i = 0; i < Count; ++i)
        {
            const FRoomNode& Node = *Rooms[i];

            // 방마다 고정된 흔들림 -> 다시 그려도 같은 자리
            FRandomStream Stream(Node.RoomId * 100 + Node.Row * 10 + Node.Col);
            const float JitterX = bCenterX ? 0.f : Stream.FRandRange(-JitterMaxX, JitterMaxX);
            const float JitterY = bAnchorRow ? 0.f : Stream.FRandRange(-JitterMaxY, JitterMaxY);

            // 가로 = 열 위치와 "줄 가운데 기준 고른 간격" 사이. 둘 다 열 순서를 지키므로 선이 엇갈리지 않음
            float X = GridWidth * 0.5f;
            if (!bCenterX)
            {
                const float ColumnX = Node.Col * ColumnSpacing;
                const float SpreadX = GridWidth * 0.5f - SpreadSpan * 0.5f + SpreadSpan * i / (Count - 1);
                X = FMath::Lerp(ColumnX, SpreadX, RowSpread) + JitterX;
            }

            // 세로 = 아래가 1레벨
            const float Y = (MaxRow - Row) * RowSpacing + JitterY;

            RoomPositions.Add(Node.RoomId, FVector2D(X, Y) + EdgePadding);
        }
    }
}

void UMapCanvasWidget::RefreshEdges(const ATerminusPlayerState* LocalPS)
{
    if (!EdgeLayer) return;

    // 지나온 길 = 방문 순서상 연속한 두 방 + 지금 고른 다음 방으로 가는 길
    TSet<TPair<int32, int32>> TakenEdges;
    int32 CurrentRoomId = INDEX_NONE;

    if (LocalPS)
    {
        const FRunState Run = LocalPS->GetRunState();
        for (int32 k = 0; k + 1 < Run.VisitedRoomIds.Num(); ++k)
        {
            TakenEdges.Add({ Run.VisitedRoomIds[k], Run.VisitedRoomIds[k + 1] });
        }

        if (Run.CurrentMapLevel > 0)
        {
            CurrentRoomId = Run.CurrentRoomId;
            if (Run.SelectedRoomId != INDEX_NONE)
            {
                TakenEdges.Add({ Run.CurrentRoomId, Run.SelectedRoomId });
            }
        }
    }

    TArray<FMapEdgeDraw> Edges;
    for (const FRoomNode& Node : CachedMapData)
    {
        const FVector2D* From = RoomPositions.Find(Node.RoomId);
        if (!From) continue;

        for (const int32 ToId : Node.ConnectedRoomIds)
        {
            const FVector2D* To = RoomPositions.Find(ToId);
            if (!To) continue;

            FMapEdgeDraw& Edge = Edges.AddDefaulted_GetRef();
            Edge.From = *From;
            Edge.To = *To;

            if (TakenEdges.Contains({ Node.RoomId, ToId }))
            {
                Edge.State = EMapEdgeState::Taken;
            }
            else if (Node.RoomId == CurrentRoomId)
            {
                Edge.State = EMapEdgeState::Available;
            }
        }
    }

    EdgeLayer->SetEdges(MoveTemp(Edges));
}

void UMapCanvasWidget::BuildMapUI(const TArray<FRoomNode>& MapData)
{
    if (!MapCanvasPanel || !RoomWidgetClass || MapData.Num() == 0) return;

    // 같은 지도가 다시 들어온 경우(복제 재수신, 바인딩 시점 차이 등) 위젯을 새로 만들지 않음.
    // 새로 만들면 스크롤 위치가 날아가고 전체가 깜빡임
    if (CreatedRoomWidgets.Num() > 0 && IsSameMap(CachedMapData, MapData))
    {
        RefreshRoomStates();
        return;
    }

    MapCanvasPanel->ClearChildren();
    CreatedRoomWidgets.Empty();
    CachedMapData = MapData;

    ComputeLayout(MapData);

    // ------------------------------------------------------------------
    // 연결선 레이어: 캔버스 첫 자식(맨 뒤), 지도 전체 크기로 (0,0) 에.
    // 캔버스는 자식 위치+크기로 자기 크기를 정하므로 이게 곧 캔버스 크기 = 스크롤 범위가 됨
    // ------------------------------------------------------------------
    EdgeLayer = CreateWidget<UMapEdgeLayer>(this, UMapEdgeLayer::StaticClass());
    if (EdgeLayer)
    {
        EdgeLayer->SetEdgeStyle(EdgeStyle);
        EdgeLayer->SetVisibility(ESlateVisibility::HitTestInvisible);   // 클릭은 방 아이콘이 받게

        if (UCanvasPanelSlot* EdgeSlot = MapCanvasPanel->AddChildToCanvas(EdgeLayer))
        {
            EdgeSlot->SetAutoSize(false);
            EdgeSlot->SetPosition(FVector2D::ZeroVector);
            EdgeSlot->SetSize(MapContentSize);
            EdgeSlot->SetZOrder(0);
        }
    }

    // ------------------------------------------------------------------
    // 방 아이콘: 계산해 둔 좌표에 위젯의 RoomWidgetAlignment 지점(기본 가운데)을 맞춤
    // ------------------------------------------------------------------
    for (const FRoomNode& Node : MapData)
    {
        const FVector2D* Position = RoomPositions.Find(Node.RoomId);
        if (!Position) continue;

        URoomNodeWidget* NewRoomWidget = CreateWidget<URoomNodeWidget>(this, RoomWidgetClass);
        if (!NewRoomWidget) continue;

        NewRoomWidget->SetupRoomNode(Node, RoomTypeIcons);
        NewRoomWidget->OnRoomNodeClicked.AddDynamic(this, &UMapCanvasWidget::HandleRoomClicked);

        // 선택 가능 여부(경로 제한)는 아래 RefreshRoomStates 에서 한 번에 적용

        if (UCanvasPanelSlot* CanvasSlot = MapCanvasPanel->AddChildToCanvas(NewRoomWidget))
        {
            // 아이콘 중심이 방 좌표에 오게 왼쪽 위 기준으로 배치 (초상화가 붙어 폭이 늘어도 아이콘은 그대로)
            CanvasSlot->SetAutoSize(true);
            CanvasSlot->SetAlignment(FVector2D::ZeroVector);
            CanvasSlot->SetPosition(*Position - RoomIconCenter);
            CanvasSlot->SetZOrder(1);
        }

        CreatedRoomWidgets.Add(Node.RoomId, NewRoomWidget);
    }

    // 경로 제한 + 선택 표시 + 연결선 상태 초기 적용
    RefreshRoomStates();

    // 스크롤은 이 위젯이 처음 지도를 그릴 때만 맨 아래(시작 지점)로.
    // 그 뒤로는 사용자가 보던 위치를 건드리지 않음. 레이아웃이 잡힌 다음 틱에
    if (!bInitialScrollDone)
    {
        bInitialScrollDone = true;

        // 약참조 람다. 다음 틱 전에 위젯이 사라지면 그냥 안 불림 ([this] 만 잡으면 댕글링)
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (ScrollBox) ScrollBox->ScrollToEnd();
        }));
    }
}

void UMapCanvasWidget::RefreshAllRoomSelections()
{
    // 모든 PlayerState 모으기
    // PC 반복자는 클라에선 자기 PC 하나뿐이라 남의 선택이 안 보임 -> GameState 의 PlayerArray 는 전원 복제됨
    TArray<ATerminusPlayerState*> AllPlayerStates;
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
            {
                AllPlayerStates.Add(TPS);
            }
        }
    }

    ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;

    // 모든 방 위젯의 선택 상태(테두리/프로필 아이콘) 일괄 갱신
    for (auto& Pair : CreatedRoomWidgets)
    {
        if (URoomNodeWidget* RoomWidget = Pair.Value)
        {
            RoomWidget->RefreshSelectionState(AllPlayerStates, LocalPS);
        }
    }
}

void UMapCanvasWidget::HandleRoomClicked(int32 ClickedRoomId)
{
	UE_LOG(LogTemp, Log, TEXT("Room Clicked ID: %d"), ClickedRoomId);
	
    ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
    if (!PC) return;

    // 서버 소유 액터(MapManager)엔 클라가 RPC 를 못 쏴서 내 PC 를 거쳐 감
    // Standalone 에서도 Server RPC 는 그냥 로컬에서 바로 실행되므로 경로를 하나로 합침
    // 싱글은 즉시 입장, 멀티는 전원 선택 대기 - 판단은 서버(MapManager)가 함
    // 방 상태 갱신은 RunState 변경 -> OnPlayerRunStateChanged 가 알아서 함
    PC->Server_RequestSelectRoom(ClickedRoomId);
}

void UMapCanvasWidget::OnOtherRunStateChanged(const FRunState& NewRunState)
{
    RefreshAllRoomSelections();
}

void UMapCanvasWidget::OnPlayerRunStateChanged(const FRunState& NewRunState)
{
    // 현재 위치/선택이 바뀌어도 지도 모양은 그대로 -> 위젯 재생성 없이 상태만 갱신.
    // (예전엔 여기서 BuildMapUI 로 전부 다시 만들어서 스크롤이 맨 아래로 튀었음)
    RefreshRoomStates();
}
