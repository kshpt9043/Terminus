#include "Widgets/Map/MapScreenWidget.h"

#include "Player/TerminusPlayerController.h"
#include "Widgets/Map/MapCanvasWidget.h"
#include "Widgets/Relic/RelicBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Map/MapManager.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUISettings.h"

void UMapScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!RelicBar)
	{
		if (UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			RelicBar = CreateWidget<URelicBarWidget>(this, UTerminusUISettings::Pick(RelicBarClass, &UTerminusUISettings::RelicBar));
			if (UCanvasPanelSlot* CanvasSlot = RelicBar ? Root->AddChildToCanvas(RelicBar) : nullptr)
			{
				CanvasSlot->SetAnchors(FAnchors(1.f, 1.f));
				CanvasSlot->SetAlignment(FVector2D(1.f, 1.f));
				CanvasSlot->SetPosition(FVector2D(-28.f, -28.f));
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetZOrder(10);
			}
		}
	}
}

void UMapScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 구역 진입 / 복귀에 맞춰 화면째 숨기기
	VisibleState = GetVisibility();
	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->OnViewAreaChanged.AddUniqueDynamic(this, &UMapScreenWidget::HandleViewAreaChanged);
		HandleViewAreaChanged(PC->GetViewedArea());
	}
}

void UMapScreenWidget::NativeDestruct()
{
	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->OnViewAreaChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMapScreenWidget::HandleViewAreaChanged(ADungeonArea* NewArea)
{
	if (NewArea)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SetVisibility(VisibleState);

		// 방을 끝내고 지도로 돌아오면 이제 고를 줄이 가운데 오게
		if (MapCanvas) MapCanvas->CenterOnSelectableRooms();
	}
}

void UMapScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshHint();
}

void UMapScreenWidget::RefreshHint()
{
	if (!HintText && !HintSubText) return;

	const ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 고른 사람 수 / 전체
	int32 Chosen = 0;
	int32 Total = 0;
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
			{
				++Total;
				if (TPS->GetSelectedRoomId() != INDEX_NONE) ++Chosen;
			}
		}
	}

	const bool bMine = LocalPS && LocalPS->GetSelectedRoomId() != INDEX_NONE;
	TerminusUI::SetTextIfChanged(HintText, bMine && Total > 1 ? TEXT("동료의 선택을 기다리는 중") : TEXT("다음 방을 고르세요"));

	if (HintSubText)
	{
		if (!CachedMapManager.IsValid())
		{
			CachedMapManager = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
		}

		// CurrentMapLevel = 지금까지 지난 방 수 -> 다음에 들어갈 방은 그 다음
		const int32 Levels = CachedMapManager.IsValid() ? CachedMapManager->GetLevelCount() : 12;
		const int32 Next = FMath::Min((LocalPS ? LocalPS->GetCurrentMapLevel() : 0) + 1, Levels);

		FString Sub = FString::Printf(TEXT("다음 방 %d / %d"), Next, Levels);
		if (Total > 1)
		{
			Sub += FString::Printf(TEXT(" · %d / %d명 선택"), Chosen, Total);
		}
		Sub += TEXT(" · 마우스 휠로 지도 이동");
		TerminusUI::SetTextIfChanged(HintSubText, Sub);
	}
}
