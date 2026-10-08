#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RescueWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;

/**
 * 다른 방 동료가 전멸했을 때 (기획 사망 순서도). 살아남은 사람은 구출 / 난입 투표, 전멸한 사람은 기다림
 *  - 구출: 내 체력이 최대 체력의 HealthCost 만큼 줄고, 쓰러진 동료를 체력 1 로 데리고 다음 방으로 (보상 없음)
 *  - 난입: 전멸한 방에 들어가 남은 몬스터와 싸움 (몬스터 체력 그대로, 이기면 보상 + 쓰러진 동료 체력 1)
 *  - 다수결, 동점이면 랜덤. 시간 안에 안 고르면 기권
 * 띄우는 건 PC 의 Client_ShowRescue, 결과가 나오면 Client_CloseRescue
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 RescueClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API URescueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(bool bInChooser, const TArray<FString>& WipedNames, bool bInCanIntervene, float InHealthCost, float Seconds);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TimerText;

	// 선택지 (살아남은 사람만 보임)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> OptionBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RescueButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RescueDescText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> InterveneButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> InterveneDescText;

private:
	bool bChooser = false;
	bool bChosen = false;
	bool bCanIntervene = true;
	float TimeLeft = 0.f;

	void BuildDefaultLayout();
	void Choose(bool bIntervene);

	UFUNCTION() void HandleRescue();
	UFUNCTION() void HandleIntervene();
};
