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
	if (!MapCanvasPanel || !RoomWidgetClass) return;

	// 기존 위젯 초기화
	MapCanvasPanel->ClearChildren();
	CreatedRoomWidgets.Empty();

	for (const FRoomNode& Node : MapData)
	{
		// 1. 방 위젯 동적 생성
		URoomNodeWidget* NewRoomWidget = CreateWidget<URoomNodeWidget>(this, RoomWidgetClass);
		if (!NewRoomWidget) continue;

		// 2. 방 데이터 및 아이콘 전달
		NewRoomWidget->SetupRoomNode(Node, RoomTypeIcons);

		// 3. 클릭 이벤트 연결
		NewRoomWidget->OnRoomNodeClicked.AddDynamic(this, &UMapCanvasWidget::HandleRoomClicked);

		// 4. CanvasPanel에 자식으로 추가
		UCanvasPanelSlot* CanvasSlot = MapCanvasPanel->AddChildToCanvas(NewRoomWidget);
		if (CanvasSlot)
		{
			// 5. 위치 계산 (아래쪽 Row=0부터 위쪽 Row=9로 올라가는 구도 예시)
			float PositionX = Node.Col * CellSize.X + CanvasOffset.X;
			float PositionY = (9 - Node.Row) * CellSize.Y + CanvasOffset.Y;

			CanvasSlot->SetPosition(FVector2D(PositionX, PositionY));
			CanvasSlot->SetAutoSize(true); // 위젯 원본 크기에 맞춤
		}

		// 클릭/선 그리기 참조용으로 저장
		CreatedRoomWidgets.Add(Node.RoomId, NewRoomWidget);
		
		if (ScrollBox)
		{
			// 한 프레임 뒤에 ScrollToEnd 호출
			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				if (ScrollBox)
				{
					ScrollBox->ScrollToEnd();
				}
			});
		}
	}
}

void UMapCanvasWidget::HandleRoomClicked(int32 ClickedRoomId)
{
	UE_LOG(LogTemp, Log, TEXT("Room Clicked ID: %d"), ClickedRoomId);
	// TODO: 클릭된 방으로 플레이어 이동 또는 방 정보 팝업 띄우기 로직 구현
}