#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/RewardTypes.h"
#include "EventWidget.generated.h"

class UButton;
class UChoiceCardWidget;
class UPanelWidget;
class UTextBlock;
class UItemSlotWidget;
class UItemPickPanelWidget;
class UEventWidget;

/**
 * 이벤트 방 화면. 기획: 이벤트 보상 중 몇 개(3개)를 뽑아 후보로 보여주고 하나를 고름 (사용자 결정 2026-10-06)
 *  - 멀티도 각자 자기 후보를 받고 각자 고름. 같은 방 전원이 고르면 지도로
 *  - 스킬 후보인데 강화 칸이 꽉 찼으면 바꿀 칸, '유물 변경(확정)' 이면 바꿀 내 유물을 먼저 고름
 *  - 고르면 서버가 결과(얻은 것)를 알려 줌
 * 띄우는 건 PC 의 Client_ShowEvent
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 EventClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UEventWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const TArray<FEventOption>& InOptions);

	// 서버가 알려 준 결과 ("획득: 은빛 방패")
	void ShowResult(const FText& Result);

	void HandleCardClicked(int32 Index);

protected:
	virtual void NativeOnInitialized() override;

	// "이벤트"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 안내 / 결과
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	// 후보 카드 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CardBox;

	// 칸 고르기 (강화 스킬 칸 / 내 유물). 공통 패널
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UItemPickPanelWidget> PickPanel;

	UPROPERTY(EditAnywhere, Category = "Event")
	FVector2D CardSize = FVector2D(250.f, 300.f);

	// 후보 카드 하나. 비우면 C++ 기본 모양. 스킬 후보면 ItemSlot 에 그 스킬이 보임
	// ChoiceId: EEventOptionType 이름 (Currency / ClassSkill / RelicSwapChosen ...)
	UPROPERTY(EditAnywhere, Category = "Event")
	TSubclassOf<UChoiceCardWidget> CardClass;

	UPROPERTY(EditAnywhere, Category = "Event")
	TSubclassOf<UItemSlotWidget> SlotClass;

	// 기본 레이아웃일 때 칸 고르기 패널 (비우면 C++ 기본)
	UPROPERTY(EditAnywhere, Category = "Event")
	TSubclassOf<UItemPickPanelWidget> PickPanelClass;

private:
	TArray<FEventOption> Options;
	int32 PendingIndex = INDEX_NONE;   // 칸을 고르는 중인 후보
	bool bChosen = false;


	void BuildDefaultLayout();
	void RebuildCards();
	void ShowPick(const FEventOption& Option);
	void Send(int32 Index, int32 ReplaceSlot, FName RelicRow);

	void HandleSkillSlotPicked(UItemSlotWidget* ClickedSlot);
	void HandleRelicPicked(UItemSlotWidget* ClickedSlot);

	void HandlePickCancel();
};
