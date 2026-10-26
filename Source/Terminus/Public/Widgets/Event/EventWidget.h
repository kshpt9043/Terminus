#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
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
 * 이벤트 방 화면 (10-24 기획 엑셀 DT_Event / DT_EventChoice). 이벤트 이름 + 상황 설명 + 선택지 카드
 *  - 멀티도 같은 이벤트를 보고 각자 고름. 같은 방 전원이 고르면 지도로 (쉬는 가디언이면 전투)
 *  - 스킬을 받는데 강화 칸이 꽉 찼으면 바꿀 칸, 유물을 받는데 유물 칸이 꽉 찼으면 버릴 유물을 먼저 고름
 *  - 대가를 못 치르는 선택지는 눌리지 않고 아래 줄에 이유가 보임
 *  - 고르면 서버가 결과(기획 결과 문구 + 바뀐 것)를 알려 줌
 * 띄우는 건 PC 의 Client_ShowEvent
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 EventClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UEventWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& InTitle, const FText& InSituation, const TArray<FEventOption>& InOptions);

	// 서버가 알려 준 결과 ("획득: 은빛 방패")
	void ShowResult(const FText& Result);

	void HandleCardClicked(int32 Index);

protected:
	virtual void NativeOnInitialized() override;

	// 이벤트 이름
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 상황 설명. 없으면 MessageText 에 같이 씀
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SituationText;

	// 안내 / 결과
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	// 후보 카드 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CardBox;

	// 칸 고르기 (강화 스킬 칸 / 내 유물). 공통 패널
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UItemPickPanelWidget> PickPanel;

	UPROPERTY(EditAnywhere, Category = "Event")
	FVector2D CardSize = FVector2D(260.f, 340.f);

	// 선택지 카드 하나. 비우면 프로젝트 설정(Terminus UI) -> C++ 기본 모양. 받을 스킬 / 유물이 정해져 있으면 ItemSlot 에 보임
	// ChoiceId: EEventOutcomeType 이름 (GiveCurrencyRange / GiveOwnClassSkill / StartWeakGuardianBattle ...)
	UPROPERTY(EditAnywhere, Category = "Event")
	TSubclassOf<UChoiceCardWidget> CardClass;

	UPROPERTY(EditAnywhere, Category = "Event")
	TSubclassOf<UItemSlotWidget> SlotClass;

	// 기본 레이아웃일 때 칸 고르기 패널 (비우면 프로젝트 설정(Terminus UI) -> C++ 기본)
	UPROPERTY(EditAnywhere, Category = "Event")
	TSubclassOf<UItemPickPanelWidget> PickPanelClass;

private:
	TArray<FEventOption> Options;
	FText Situation;
	int32 PendingIndex = INDEX_NONE;   // 칸을 고르는 중인 선택지
	bool bChosen = false;

	void SetMessage(const FString& Message);


	void BuildDefaultLayout();
	void RebuildCards();
	void ShowPick(const FEventOption& Option, bool bSkillSlot);
	void Send(int32 Index, int32 ReplaceSlot, FName RelicRow);

	void HandleSkillSlotPicked(UItemSlotWidget* ClickedSlot);
	void HandleRelicPicked(UItemSlotWidget* ClickedSlot);

	void HandlePickCancel();
};
