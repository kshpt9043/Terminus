#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillTooltipWidget.generated.h"

class UTextBlock;
struct FSkillRow;

/**
 * 스킬 정보 툴팁 (마우스를 올리면 뜨는 작은 창). 스킬 칸 등에서 SetToolTip 으로 붙여 씀
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_SkillTooltip) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 하나도 없으면(C++ 클래스 그대로) C++ 이 기본 모양을 만든다
 */
UCLASS()
class TERMINUS_API USkillTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSkill(const FSkillRow* InSkill);

protected:
	virtual void NativeOnInitialized() override;

	// 스킬 이름
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;

	// "공격 · 적 1명"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TypeText;

	// "에너지 1" / "스킬 에너지 1"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CostText;

	// 설명 (DT 의 Description_KR)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescText;

	// 기본 모양의 최대 폭 (설명이 길면 줄바꿈)
	UPROPERTY(EditAnywhere, Category = "Skill Tooltip")
	float DefaultWidth = 280.f;

private:
	void BuildDefaultLayout();
};
