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
	AMapGenerator* MapGen = Cast<AMapGenerator>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapGenerator::StaticClass()));
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
}

void UMapCanvasWidget::BuildMapUI(const TArray<FRoomNode>& MapData)
{
    if (!MapCanvasPanel || !RoomWidgetClass || MapData.Num() == 0) return;

    // 기존 위젯 및 데이터 초기화
    MapCanvasPanel->ClearChildren();
    CreatedRoomWidgets.Empty();
    CachedMapData = MapData;

    // 1. 전달받은 맵 데이터 중 가장 높은 Row(보스방 레벨) 탐색 (가변 TotalLevels 대응)
    int32 MaxRow = 0;
    for (const FRoomNode& Node : MapData)
    {
        if (Node.Row > MaxRow)
        {
            MaxRow = Node.Row;
        }
    }

    // 2. MapCanvasPanel 가로 중앙 좌표 구하기
    UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(MapCanvasPanel->Slot);
    float PanelWidth = (PanelSlot && PanelSlot->GetSize().X > 0) ? PanelSlot->GetSize().X : 800.0f;
    float CenterX = PanelWidth * 0.5f;

    // 3. Row(레벨)별로 방들을 그룹화
    TMap<int32, TArray<const FRoomNode*>> RoomsByRow;
    for (const FRoomNode& Node : MapData)
    {
        RoomsByRow.FindOrAdd(Node.Row).Add(&Node);
    }

    // 4. 레벨별 방 배치 처리
    for (auto& Pair : RoomsByRow)
    {
        int32 Row = Pair.Key;
        TArray<const FRoomNode*>& RowNodes = Pair.Value;

        // 같은 줄 내에서는 Col(열) 순서로 좌->우 정렬
        RowNodes.Sort([](const FRoomNode& A, const FRoomNode& B) {
            return A.Col < B.Col;
        });

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

            UCanvasPanelSlot* CanvasSlot = MapCanvasPanel->AddChildToCanvas(NewRoomWidget);
            if (CanvasSlot)
            {
                // 고유 Random Stream 생성 (UI 갱신 시에도 위치가 일정하게 불규칙함)
                FRandomStream RoomRandomStream(Node->RoomId * 100 + Node->Row * 10 + Node->Col);

                // 오프셋 설정 (버튼 겹침 방지)
                float MaxOffsetX = CellSize.X * 0.25f;
                float RandomOffsetX = (NodeCount > 1) ? RoomRandomStream.FRandRange(-MaxOffsetX, MaxOffsetX) : 0.0f;

                float MaxOffsetY = CellSize.Y * 0.15f;
                float RandomOffsetY = (Node->Row == 0 || Node->Row == MaxRow) ? 0.0f : RoomRandomStream.FRandRange(-MaxOffsetY, MaxOffsetY);

                // ★ [핵심] MaxRow 변수를 사용하여 10레벨, 12레벨, 15레벨 등 레벨 수에 맞춰 Y축 높이 자동 계산
                float PositionX = StartX + (Index * SpacingX) + RandomOffsetX;
                float PositionY = (MaxRow - Node->Row) * CellSize.Y + CanvasOffset.Y + RandomOffsetY;

                CanvasSlot->SetPosition(FVector2D(PositionX, PositionY));
                CanvasSlot->SetAutoSize(true);
                CanvasSlot->SetZOrder(1); // 방 버튼을 선(Line)보다 높은 Z-Order에 배치
            }

            CreatedRoomWidgets.Add(Node->RoomId, NewRoomWidget);
        }
    }

    // 5. 한 프레임 뒤에 Geometry 정착 후 스크롤 하단 이동 및 선 그리기 갱신
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (ScrollBox)
        {
            ScrollBox->ScrollToEnd();
        }
        InvalidateLayoutAndVolatility();
    });
}

void UMapCanvasWidget::HandleRoomClicked(int32 ClickedRoomId)
{
	UE_LOG(LogTemp, Log, TEXT("Room Clicked ID: %d"), ClickedRoomId);
	// TODO: 클릭된 방으로 플레이어 이동 또는 방 정보 팝업 띄우기 로직 구현
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
