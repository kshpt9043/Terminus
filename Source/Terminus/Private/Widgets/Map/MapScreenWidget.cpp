#include "Widgets/Map/MapScreenWidget.h"

#include "Player/TerminusPlayerController.h"
#include "Widgets/Map/MapCanvasWidget.h"

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
