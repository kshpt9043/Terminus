#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "SpectateWidget.generated.h"

class ADungeonArea;
class UPanelWidget;
class UTextBlock;
class USpectateWidget;

// 구역 버튼마다 어느 구역을 볼지 넘기는 중계 (UButton 의 OnClicked 는 인자가 없음). Area 가 비어 있으면 내 구역
UCLASS()
class TERMINUS_API USpectateClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<USpectateWidget> Owner;
	TWeakObjectPtr<ADungeonArea> Area;
	bool bMine = false;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 관전 바 (기획: 한쪽 파티가 먼저 끝날 경우 관전 가능). 화면 위쪽 가운데
 *  - 내 싸움이 끝났거나(승리 / 클리어 / 전멸) 내가 쓰러졌고, 다른 구역이 아직 진행 중일 때만 보임
 *  - 구역 버튼을 누르면 그 구역 화면으로 (이 컴퓨터만. 서버는 모름). 전투 HUD 는 '관전 중' 으로 보여 주고 스킬은 못 씀
 *  - 보던 구역이 끝나거나 내가 다시 싸워야 하면 내 구역으로 돌아옴
 * PC 가 구역에 처음 들어갈 때 만들어 두고, 보일지는 스스로 판단
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 SpectateClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API USpectateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void HandleAreaClicked(ADungeonArea* Area, bool bMine);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LabelText;

	// 구역 버튼이 들어갈 곳 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> ButtonBox;

private:
	// 숨긴(Collapsed) 위젯은 NativeTick 이 안 불려서 타이머로 판단
	FTimerHandle RefreshHandle;
	FString ShownKey;

	UPROPERTY()
	TArray<TObjectPtr<USpectateClickRelay>> Relays;

	void BuildDefaultLayout();
	void Refresh();
	void RebuildButtons(ADungeonArea* MyArea, const TArray<ADungeonArea*>& Others, ADungeonArea* Viewed);
};
