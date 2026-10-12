#include "Widgets/Skill/SkillTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/SkillTypes.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/Common/TerminusUIText.h"
#include "Widgets/TerminusUIColors.h"

void USkillTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
}

void USkillTooltipWidget::BuildDefaultLayout()
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetMaxDesiredWidth(DefaultWidth);
	WidgetTree->RootWidget = Size;

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrushColor(FLinearColor(0.06f, 0.05f, 0.05f, 0.95f));
	Box->SetPadding(FMargin(12.f, 10.f));
	Size->SetContent(Box);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Column);

	NameText = TerminusUI::MakeWrapText(WidgetTree, TEXT("NameText"), TEXT(""), 17, FLinearColor::White, ETextJustify::Left);
	TypeText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TypeText"), TEXT(""), 12, TerminusUI::Color::TextMuted, ETextJustify::Left);
	CostText = TerminusUI::MakeWrapText(WidgetTree, TEXT("CostText"), TEXT(""), 12, FLinearColor(0.7f, 0.6f, 1.f), ETextJustify::Left);
	DescText = TerminusUI::MakeWrapText(WidgetTree, TEXT("DescText"), TEXT(""), 14, FLinearColor(1.f, 0.95f, 0.85f), ETextJustify::Left);

	Column->AddChildToVerticalBox(NameText);
	Column->AddChildToVerticalBox(TypeText);
	Column->AddChildToVerticalBox(CostText);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(DescText))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
}

void USkillTooltipWidget::SetSkill(const FSkillRow* InSkill)
{
	if (!InSkill) return;

	if (NameText)
	{
		NameText->SetText(InSkill->DisplayName_KR);
	}
	if (TypeText)
	{
		TypeText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *TerminusUI::SkillTypeLabel(InSkill->SkillType), *TerminusUI::TargetTypeLabel(InSkill->TargetType))));
	}
	if (CostText)
	{
		FString Cost;
		if (InSkill->EnergyCost > 0)      Cost = FString::Printf(TEXT("에너지 %d"), InSkill->EnergyCost);
		if (InSkill->SkillEnergyCost > 0) Cost += FString::Printf(TEXT("%s스킬 에너지 %d"), Cost.IsEmpty() ? TEXT("") : TEXT("  ·  "), InSkill->SkillEnergyCost);
		if (Cost.IsEmpty())               Cost = TEXT("비용 없음");
		CostText->SetText(FText::FromString(Cost));
	}
	if (DescText)
	{
		// DT 설명 중 일부에 줄바꿈이 "\n" 글자 그대로 들어가 있어서 진짜 줄바꿈으로
		DescText->SetText(FText::FromString(InSkill->Description_KR.ToString().Replace(TEXT("\\n"), TEXT("\n"))));
	}
}
