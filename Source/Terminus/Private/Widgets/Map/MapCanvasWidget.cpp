// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/MapCanvasWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Map/RoomNodeWidget.h"
#include "Components/ScrollBox.h"
#include "GameFramework/GameStateBase.h"
#include "Player/TerminusPlayerController.h"
#include "TimerManager.h"
#include "EngineUtils.h"

void UMapCanvasWidget::NativeConstruct()
{
	Super::NativeConstruct();

    // 구역 진입 / 복귀에 맞춰 지도 숨기기
    VisibleState = GetVisibility();
    if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
    {
        PC->OnViewAreaChanged.AddUniqueDynamic(this, &UMapCanvasWidget::HandleViewAreaChanged);
        HandleViewAreaChanged(PC->GetViewedArea());
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
    if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
    {
        PC->OnViewAreaChanged.RemoveAll(this);
    }

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

    int32 MaxRow = 0;
    for (const FRoomNode& Node : MapData)
    {
        if (Node.Row > MaxRow) MaxRow = Node.Row;
    }

    UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(MapCanvasPanel->Slot);
    float PanelWidth = (PanelSlot && PanelSlot->GetSize().X > 0.0f) ? PanelSlot->GetSize().X : 800.0f;
    float CenterX = PanelWidth * 0.5f;

    TMap<int32, TArray<const FRoomNode*>> RoomsByRow;
    for (const FRoomNode& Node : MapData)
    {
        RoomsByRow.FindOrAdd(Node.Row).Add(&Node);
    }

    for (auto& Pair : RoomsByRow)
    {
        int32 Row = Pair.Key;
        TArray<const FRoomNode*>& RowNodes = Pair.Value;

        RowNodes.Sort([](const FRoomNode& A, const FRoomNode& B) { return A.Col < B.Col; });

        int32 NodeCount = RowNodes.Num();
        float SpacingX = CellSize.X;
        float RowTotalWidth = (NodeCount - 1) * SpacingX;
        float StartX = CenterX - (RowTotalWidth * 0.5f);

        for (int32 Index = 0; Index < NodeCount; ++Index)
        {
            const FRoomNode* Node = RowNodes[Index];
            if (!Node) continue;

            URoomNodeWidget* NewRoomWidget = CreateWidget<URoomNodeWidget>(this, RoomWidgetClass);
            if (!NewRoomWidget) continue;

            NewRoomWidget->SetupRoomNode(*Node, RoomTypeIcons);
            NewRoomWidget->OnRoomNodeClicked.AddDynamic(this, &UMapCanvasWidget::HandleRoomClicked);

            // 선택 가능 여부(경로 제한)는 아래 RefreshRoomStates 에서 한 번에 적용

            UCanvasPanelSlot* CanvasSlot = MapCanvasPanel->AddChildToCanvas(NewRoomWidget);
            if (CanvasSlot)
            {
                FRandomStream RoomRandomStream(Node->RoomId * 100 + Node->Row * 10 + Node->Col);

                float MaxOffsetX = CellSize.X * 0.25f;
                float RandomOffsetX = (NodeCount > 1) ? RoomRandomStream.FRandRange(-MaxOffsetX, MaxOffsetX) : 0.0f;

                float MaxOffsetY = CellSize.Y * 0.15f;
                float RandomOffsetY = (Node->Row == 0 || Node->Row == MaxRow) ? 0.0f : RoomRandomStream.FRandRange(-MaxOffsetY, MaxOffsetY);

                float PositionX = StartX + (Index * SpacingX) + RandomOffsetX;
                float PositionY = (MaxRow - Node->Row) * CellSize.Y + CanvasOffset.Y + RandomOffsetY;

                CanvasSlot->SetPosition(FVector2D(PositionX, PositionY));
                CanvasSlot->SetAutoSize(true);
                CanvasSlot->SetZOrder(1);
            }

            CreatedRoomWidgets.Add(Node->RoomId, NewRoomWidget);
        }
    }

    // 경로 제한 + 선택 표시 초기 적용
    RefreshRoomStates();

    // 스크롤은 이 위젯이 처음 지도를 그릴 때만 맨 아래(시작 지점)로.
    // 그 뒤로는 사용자가 보던 위치를 건드리지 않음
    const bool bScrollToStart = !bInitialScrollDone;
    bInitialScrollDone = true;

    // 약참조 람다. 다음 틱 전에 위젯이 사라지면 그냥 안 불림 ([this] 만 잡으면 댕글링)
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, bScrollToStart]()
    {
        if (bScrollToStart && ScrollBox) ScrollBox->ScrollToEnd();
        InvalidateLayoutAndVolatility();
    }));
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

void UMapCanvasWidget::HandleViewAreaChanged(ADungeonArea* NewArea)
{
    // 숨기기만 하고 위젯은 그대로 -> 돌아왔을 때 스크롤 위치도 그대로
    SetVisibility(NewArea ? ESlateVisibility::Collapsed : VisibleState);
}

void UMapCanvasWidget::OnPlayerRunStateChanged(const FRunState& NewRunState)
{
    // 현재 위치/선택이 바뀌어도 지도 모양은 그대로 -> 위젯 재생성 없이 상태만 갱신.
    // (예전엔 여기서 BuildMapUI 로 전부 다시 만들어서 스크롤이 맨 아래로 튀었음)
    RefreshRoomStates();
}

int32 UMapCanvasWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
                                    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // ★ 1. 자식 방 버튼들을 그리기 전 바탕 레이어에 선(Line)을 먼저 렌더링
    if (CreatedRoomWidgets.Num() > 0 && CachedMapData.Num() > 0 && MapCanvasPanel)
    {
        FGeometry PanelGeometry = MapCanvasPanel->GetCachedGeometry();

        if (PanelGeometry.GetLocalSize().X > 0.0f)
        {
            FLinearColor DrawLineColor = LineColor.A <= 0.0f ? FLinearColor::White : LineColor;
            float DrawLineThickness = LineThickness <= 0.0f ? 3.0f : LineThickness;

            for (const FRoomNode& SourceNode : CachedMapData)
            {
                URoomNodeWidget* const* SourceWidgetPtr = CreatedRoomWidgets.Find(SourceNode.RoomId);
                if (!SourceWidgetPtr || !(*SourceWidgetPtr)) continue;

                FGeometry SourceGeo = (*SourceWidgetPtr)->GetCachedGeometry();
                FVector2D StartPos = PanelGeometry.AbsoluteToLocal(SourceGeo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));

                for (int32 TargetId : SourceNode.ConnectedRoomIds)
                {
                    URoomNodeWidget* const* TargetWidgetPtr = CreatedRoomWidgets.Find(TargetId);
                    if (!TargetWidgetPtr || !(*TargetWidgetPtr)) continue;

                    FGeometry TargetGeo = (*TargetWidgetPtr)->GetCachedGeometry();
                    FVector2D EndPos = PanelGeometry.AbsoluteToLocal(TargetGeo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));

                    TArray<FVector2D> LinePoints;
                    LinePoints.Add(StartPos);
                    LinePoints.Add(EndPos);

                    FSlateDrawElement::MakeLines(
                        OutDrawElements,
                        LayerId,
                        PanelGeometry.ToPaintGeometry(),
                        LinePoints,
                        ESlateDrawEffect::None,
                        DrawLineColor,
                        true,
                        DrawLineThickness
                    );
                }
            }
        }
    }

    // ★ 2. 선을 다 그린 뒤 Super::NativePaint를 호출하여 자식 위젯(방 버튼)들을 선 위에 올림
    int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    return MaxLayer;
}
