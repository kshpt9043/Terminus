// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/MapCanvasWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Map/RoomNodeWidget.h"
#include "Components/ScrollBox.h"

void UMapCanvasWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 월드에 존재하는 MapGenerator 액터를 찾아서 이벤트 연결
	AMapManager* MapGen = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass()));
	if (MapGen)
	{
		// 맵 생성 완료 델리게이트 바인딩
		MapGen->OnMapGenerated.AddDynamic(this, &UMapCanvasWidget::BuildMapUI);

		// 만약 위젯이 늦게 생성되어 맵 데이터가 이미 들어와 있는 상태라면 바로 그리기
		if (MapGen->Rooms.Num() > 0)
		{
			BuildMapUI(MapGen->Rooms);
		}
	}
    
    // 내 PlayerState의 RunState 변경 이벤트 바인딩
    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        ATerminusPlayerState* LocalPS = PC->GetPlayerState<ATerminusPlayerState>();
        if (LocalPS)
        {
            LocalPS->OnRunStateChanged.AddDynamic(this, &UMapCanvasWidget::OnPlayerRunStateChanged);
        }
    }
}

void UMapCanvasWidget::BuildMapUI(const TArray<FRoomNode>& MapData)
{
if (!MapCanvasPanel || !RoomWidgetClass || MapData.Num() == 0) return;

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

    // 내 PlayerState 정보 가져오기
    ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;

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

            // ★ [경로 제한] 내가 이동할 수 있는 다음 방인가 체크
            bool bIsSelectable = false;
            if (LocalPS)
            {
                if (LocalPS->GetCurrentMapLevel() == 0)
                {
                    // 시작(1레벨) 단계에서는 퀘스트방(Row == 0)만 선택 가능
                    bIsSelectable = (Node->Row == 0);
                }
                else
                {
                    // 2레벨 이후: 현재 클리어한 방(CurrentRoomId)과 연결된 방(ConnectedRoomIds)만 활성화
                    const FRoomNode* CurrentRoom = MapData.FindByPredicate([LocalPS](const FRoomNode& N) {
                        return N.RoomId == LocalPS->GetCurrentRoomId();
                    });

                    if (CurrentRoom)
                    {
                        bIsSelectable = CurrentRoom->ConnectedRoomIds.Contains(Node->RoomId);
                    }
                }
            }

            NewRoomWidget->SetRoomSelectable(bIsSelectable);

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

    // 초기 선택 상태 동기화 갱신
    RefreshAllRoomSelections();

    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (ScrollBox) ScrollBox->ScrollToEnd();
        InvalidateLayoutAndVolatility();
    });
}

void UMapCanvasWidget::RefreshAllRoomSelections()
{
    // 월드 내 모든 PlayerState 모으기
    TArray<ATerminusPlayerState*> AllPlayerStates;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->PlayerState)
        {
            if (ATerminusPlayerState* PS = Cast<ATerminusPlayerState>(PC->PlayerState))
            {
                AllPlayerStates.Add(PS);
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
	
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    ATerminusPlayerState* LocalPS = PC->GetPlayerState<ATerminusPlayerState>();
    AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass()));

    if (!LocalPS || !MapMgr) return;

    // 싱글플레이 모드: 바로 이동
    if (GetWorld()->GetNetMode() == ENetMode::NM_Standalone)
    {
        UGameplayStatics::OpenLevel(GetWorld(), "/Game/Maps/Lv_DungeonPlay");
        return;
    }

    // 멀티플레이 모드: 서버 RPC 요청 (서버에서 인원 수 및 연결 상태 검증 후 복제)
    MapMgr->Server_RequestSelectRoom(LocalPS, ClickedRoomId);
}

void UMapCanvasWidget::OnPlayerRunStateChanged(const FRunState& NewRunState)
{
    RefreshAllRoomSelections();
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
