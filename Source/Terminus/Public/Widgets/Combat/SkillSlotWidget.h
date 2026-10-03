#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillSlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class USkillTooltipWidget;
struct FSkillRow;

DECLARE_DELEGATE_OneParam(FOnSkillSlotClicked, int32 /*SlotIndex*/);

/**
 * 전투 HUD 스킬 칸 하나 (기본 스킬 / 강화 스킬 공용)
 * 전투 HUD 가 SkillBox / EnhanceSkillBox 에 SkillSlotClass 로 만들어 채움
 *
 * 칸에는 아이콘과 이름만. 자세한 정보(종류, 대상, 비용, 설명)는 마우스를 올리면 뜨는 툴팁(TooltipClass)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_SkillSlot) 를 만들고 아래 이름으로 위젯을 둘 것
 * 하나도 없으면(C++ 클래스 그대로) C++ 이 기본 모양을 만든다
 */
UCLASS()
class TERMINUS_API USkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 전투 칸 번호 (0~2 기본, 3~ 강화). 눌렸을 때 이 번호로 알림
	void SetSlotIndex(int32 InIndex) { SlotIndex = InIndex; }
	int32 GetSlotIndex() const { return SlotIndex; }

	// 이 칸의 스킬. nullptr 이면 빈 칸. 바뀌었을 때만 다시 그림
	void SetSkill(const FSkillRow* InSkill);

	// 지금 쓸 수 있는지 (내 턴 + 비용 충분)
	void SetUsable(bool bInUsable);

	// 대상 고르는 중인 칸이면 강조
	void SetPending(bool bInPending);

	FOnSkillSlotClicked OnSlotClicked;

protected:
	virtual void NativeOnInitialized() override;

	// 칸 전체 버튼
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> SlotButton;

	// 스킬 아이콘 (프로젝트 설정 Terminus Data 의 SkillIcons / SkillTypeIcons). 없으면 숨김
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Icon;

	// 스킬 이름
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;

	// 마우스를 올리면 뜨는 정보 창. 비워 두면 C++ 기본 툴팁
	UPROPERTY(EditAnywhere, Category = "Skill Slot")
	TSubclassOf<USkillTooltipWidget> TooltipClass;

	// 빈 칸일 때 이름 자리에 쓸 글자 (비우면 아무것도 안 씀)
	UPROPERTY(EditAnywhere, Category = "Skill Slot")
	FText EmptyLabel;

	// 버튼 색 (평소 / 대상 고르는 중)
	UPROPERTY(EditAnywhere, Category = "Skill Slot")
	FLinearColor NormalColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Skill Slot")
	FLinearColor PendingColor = FLinearColor(1.f, 0.85f, 0.3f);

private:
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<USkillTooltipWidget> Tooltip;

	// 지금 그려진 상태. 같은 값이면 다시 안 그림 (매 틱 불려서)
	const FSkillRow* ShownSkill = nullptr;
	bool bShownOnce = false;
	TOptional<bool> bShownUsable;
	TOptional<bool> bShownPending;

	void BuildDefaultLayout();

	UFUNCTION()
	void HandleClicked();
};
