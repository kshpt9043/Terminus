#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapScreenWidget.generated.h"

class ADungeonArea;
class UMapCanvasWidget;
class UMapTopBarWidget;

/**
 * 지도 화면 전체. 상단바 + 지도를 한 화면에 담는 베이스 (WBP_MapScreen 의 부모)
 *
 * 흐름: 게임 시작 -> 시작 스킬 고르기 -> (서버 확정) -> PC 가 이 화면을 띄움
 * 던전 구역에 들어가면 화면째 숨기고, 지도로 돌아오면 다시 보임 (위젯은 그대로라 스크롤 위치도 유지)
 *
 * WBP 에 아래 이름으로 둘 것
 *  - MapCanvas : WBP_MapCanvasWidget (필수)
 *  - MapTopBar : WBP_MapTopBar 또는 MapTopBarWidget (선택). 위쪽에 바 높이만큼
 */
UCLASS()
class TERMINUS_API UMapScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMapCanvasWidget* GetMapCanvas() const { return MapCanvas; }
	UMapTopBarWidget* GetMapTopBar() const { return MapTopBar; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UMapCanvasWidget> MapCanvas;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UMapTopBarWidget> MapTopBar;

private:
	// 숨겼다가 되돌릴 원래 가시성
	ESlateVisibility VisibleState = ESlateVisibility::SelfHitTestInvisible;

	UFUNCTION()
	void HandleViewAreaChanged(ADungeonArea* NewArea);
};
