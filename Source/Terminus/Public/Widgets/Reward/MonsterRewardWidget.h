#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/RewardTypes.h"
#include "MonsterRewardWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class USkillCardWidget;
class UItemSlotWidget;

/**
 * 방 클리어 보상 화면 (몬스터 / 가디언 / 보스). 이름은 처음 만든 몬스터방 그대로 (블루프린트 연결 유지)
 *  - 던전 재화: 이미 받음 -> 받은 양만 보여줌
 *  - 픽업 스킬 후보 중 하나 / 유물 후보 중 하나를 각각 고를 수 있음 (안 골라도 됨)
 *  - 강화 칸 / 유물 칸이 꽉 찼으면 바꿀 칸을 고름 (취소하면 후보로 돌아감). 직업 기본 유물(패시브)은 못 바꿈
 *  - 고른 쪽 후보는 사라지고 결과가 남음. '다음으로' 를 누르면 서버에 알리고 닫힘. 구역의 모든 플레이어가 누르면 지도로
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항). 루트는 화면 전체
 */
UCLASS()
class TERMINUS_API UMonsterRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FRoomRewardOffer& InOffer);

protected:
	virtual void NativeOnInitialized() override;

	// "전투 승리!" / "가디언 처치!" / "보스 처치!"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "던전 재화 +18"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CurrencyText;

	// 픽업 스킬: 제목 + 카드 칸 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SkillSection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CardBox;

	// 유물: 제목 + 카드 칸
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RelicSection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RelicBox;

	// 강화 칸이 꽉 찼을 때: 바꿀 스킬 고르기
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ReplacePanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> ReplaceBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ReplaceCancelButton;

	// 유물 칸이 꽉 찼을 때: 바꿀 유물 고르기
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RelicReplacePanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RelicReplaceBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RelicReplaceCancelButton;

	// 고른 결과 ("획득: 성스러운 돌진 / 은빛 방패")
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
	FRoomRewardOffer Offer;

	// 스킬
	FName ChosenSkill;
	FName PendingSkill;          // 카드를 골랐는데 바꿀 칸을 고르는 중
	int32 ReplaceSlot = INDEX_NONE;

	// 유물
	FName ChosenRelic;
	FName PendingRelic;
	FName ReplaceRelic;

	bool bFinished = false;

	void BuildDefaultLayout();
	void BuildSkillCards();
	void BuildRelicCards();
	void Refresh();                 // 섹션 / 바꾸기 패널 / 결과 표시
	void ShowSkillReplace();
	void ShowRelicReplace();

	UItemSlotWidget* MakeSlot();

	void HandleCardClicked(FName SkillRow);
	void HandleRelicClicked(UItemSlotWidget* ClickedSlot);
	void HandleReplaceSlotClicked(UItemSlotWidget* ClickedSlot);
	void HandleRelicReplaceClicked(UItemSlotWidget* ClickedSlot);

	UFUNCTION() void HandleReplaceCancel();
	UFUNCTION() void HandleRelicReplaceCancel();
	UFUNCTION() void HandleNextClicked();
};
