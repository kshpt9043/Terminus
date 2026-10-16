#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "MapScreenWidget.generated.h"

class ADungeonArea;
class UMapCanvasWidget;
class UMapTopBarWidget;
class URelicBarWidget;

/**
 * 지도 화면 전체. 상단바 + 지도를 한 화면에 담는 베이스 (WBP_MapScreen 의 부모)
 *
 * 흐름: 게임 시작 -> 시작 스킬 고르기 -> (서버 확정) -> PC 가 이 화면을 띄움
 * 던전 구역에 들어가면 화면째 숨기고, 지도로 돌아오면 다시 보임 (위젯은 그대로라 스크롤 위치도 유지)
 *
 * WBP 에 아래 이름으로 둘 것
 *  - MapCanvas : WBP_MapCanvasWidget (필수)
 *  - MapTopBar : WBP_MapTopBar 또는 MapTopBarWidget (선택). 위쪽에 바 높이만큼
 *  - HintText / HintSubText : 왼쪽 아래 안내 (선택). "다음 방을 고르세요" / "2 / 3명 선택 · 마우스 휠로 지도 이동"
 */
UCLASS()
class TERMINUS_API UMapScreenWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	UMapCanvasWidget* GetMapCanvas() const { return MapCanvas; }
	UMapTopBarWidget* GetMapTopBar() const { return MapTopBar; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UMapCanvasWidget> MapCanvas;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UMapTopBarWidget> MapTopBar;

	// 보유 유물 줄. 전투 HUD 와 같은 자리(오른쪽 아래). WBP 에 없으면 루트가 캔버스일 때 C++ 이 만듦
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<URelicBarWidget> RelicBar;

	// 왼쪽 아래 안내
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> HintText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> HintSubText;

	// RelicBar 를 안 뒀을 때 만들 유물 줄 (비우면 프로젝트 설정(Terminus UI) -> C++ 기본)
	UPROPERTY(EditAnywhere, Category = "Map Screen")
	TSubclassOf<URelicBarWidget> RelicBarClass;

private:
	// 숨겼다가 되돌릴 원래 가시성
	ESlateVisibility VisibleState = ESlateVisibility::SelfHitTestInvisible;

	UFUNCTION()
	void HandleViewAreaChanged(ADungeonArea* NewArea);

	// 안내 문구 (매 틱, 바뀌었을 때만 다시 넣음)
	void RefreshHint();

	TWeakObjectPtr<class AMapManager> CachedMapManager;
};
