#include "Widgets/Map/MapScreenWidget.h"

#include "Player/TerminusPlayerController.h"
#include "Widgets/Map/MapCanvasWidget.h"
#include "Widgets/Relic/RelicBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UMapScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!RelicBar)
	{
		if (UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			RelicBar = CreateWidget<URelicBarWidget>(this, URelicBarWidget::StaticClass());
			if (UCanvasPanelSlot* CanvasSlot = RelicBar ? Root->AddChildToCanvas(RelicBar) : nullptr)
			{
				CanvasSlot->SetAnchors(FAnchors(1.f, 1.f));
				CanvasSlot->SetAlignment(FVector2D(1.f, 1.f));
				CanvasSlot->SetPosition(FVector2D(-24.f, -24.f));
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
	}
}
