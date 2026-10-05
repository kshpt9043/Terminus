#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MonsterRewardWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class USkillCardWidget;
class UItemSlotWidget;

/**
 * 몬스터방 보상 화면 (기획 UI 레퍼런스 > 보상 UI)
 *  - 던전 재화는 이미 받음 -> 받은 양만 보여줌
 *  - 랜덤 강화 스킬 카드 중 1장을 고를 수 있음 (안 골라도 됨)
 *  - 강화 칸이 꽉 찼으면 바꿀 칸을 고름 (취소하면 카드로 돌아감)
 *  - 고르면 카드가 사라지고 결과 + '다음으로' 만 남음. '다음으로' 를 누르면 서버에 알리고 닫힘
 *    구역의 모든 플레이어가 누르면 지도로
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항). 루트는 화면 전체
 */
UCLASS()
class TERMINUS_API UMonsterRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InCurrency, const TArray<FName>& InSkillOffers);

protected:
	virtual void NativeOnInitialized() override;

	// "전투 승리!"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "던전 재화 +18"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CurrencyText;

	// 스킬 카드가 들어갈 칸 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CardBox;

	// 강화 칸이 꽉 찼을 때: 바꿀 칸 고르기 (안내 + 지금 장착한 칸들 + 취소)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ReplacePanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> ReplaceBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ReplaceCancelButton;

	// 고른 결과 ("획득: 성스러운 돌진")
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> NextButton;

	// 카드 / 칸 위젯. 비워 두면 C++ 기본
	UPROPERTY(EditAnywhere, Category = "Reward")
	TSubclassOf<USkillCardWidget> CardClass;

	UPROPERTY(EditAnywhere, Category = "Reward")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(EditAnywhere, Category = "Reward")
	float CardSpacing = 24.f;

private:
	TArray<FName> Offers;
	FName ChosenSkill;
	FName PendingSkill;          // 카드를 골랐는데 바꿀 칸을 고르는 중
	int32 ReplaceSlot = INDEX_NONE;
	bool bFinished = false;

	void BuildDefaultLayout();
	void ShowCards();
	void ShowReplace();
	void ShowResult();

	void HandleCardClicked(FName SkillRow);
	void HandleReplaceSlotClicked(UItemSlotWidget* ClickedSlot);

	UFUNCTION() void HandleReplaceCancel();
	UFUNCTION() void HandleNextClicked();
};
