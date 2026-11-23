#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "SkillTooltipWidget.generated.h"

class UBorder;
class UTextBlock;
struct FSkillRow;

/**
 * 스킬 정보 툴팁 (마우스를 올리면 뜨는 작은 창). 스킬 칸 등에서 SetToolTip 으로 붙여 씀
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_SkillTooltip) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 하나도 없으면(C++ 클래스 그대로) C++ 이 기본 모양을 만든다
 */
UCLASS()
class TERMINUS_API USkillTooltipWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	// Original = 강화 전 DT 행. InSkill 과 BaseValue 가 다르면 설명의 "(5+공격)" 같은 숫자를 강화된 값으로 바꿔 보여 줌
	void SetSkill(const FSkillRow* InSkill, const FSkillRow* Original = nullptr);

protected:
	virtual void NativeOnInitialized() override;

	// 스킬 이름
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;

	// "공격 · 적 1명"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TypeText;

	// "에너지 1" / "스킬 에너지 1"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CostText;

	// 위쪽 3px 종류 색 띠 (선택). 기본 모양은 바깥 판의 바탕색이 띠
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> TierBand;

	// 설명 (DT 의 Description_KR)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescText;

	// 상태이상을 거는 스킬이면 오른쪽에 붙는 두 번째 창 (상태 이름 + 효과). 없으면 C++ 이 만들어 붙임 (WBP 도)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> StatusPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

	// 기본 모양의 폭 (설명이 길면 줄바꿈)
	UPROPERTY(EditAnywhere, Category = "Skill Tooltip")
	float DefaultWidth = 320.f;

private:
	void BuildDefaultLayout();
	void BuildStatusPanel();   // 루트를 [원래 툴팁 | 상태이상 창] 가로 줄로 감쌈
};
