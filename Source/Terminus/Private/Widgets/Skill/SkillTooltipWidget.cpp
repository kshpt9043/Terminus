#include "Widgets/Skill/SkillTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/SkillTypes.h"

namespace
{
	UTextBlock* MakeTooltipText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	}

	FString TooltipTypeLabel(ESkillType Type)
	{
		switch (Type)
		{
		case ESkillType::Attack:  return TEXT("공격");
		case ESkillType::Defense: return TEXT("방어");
		default:                  return TEXT("특수");
		}
	}

	FString TooltipTargetLabel(ETargetType Type)
	{
		switch (Type)
		{
		case ETargetType::SingleEnemy: return TEXT("적 1명");
		case ETargetType::AllEnemies:  return TEXT("적 전체");
		case ETargetType::SingleAlly:  return TEXT("아군 1명");
		case ETargetType::AllAllies:   return TEXT("아군 전체");
		default:                       return TEXT("자신");
		}
	}
}

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

	NameText = MakeTooltipText(WidgetTree, TEXT("NameText"), 17, FLinearColor::White);
	TypeText = MakeTooltipText(WidgetTree, TEXT("TypeText"), 12, FLinearColor(0.75f, 0.75f, 0.75f));
	CostText = MakeTooltipText(WidgetTree, TEXT("CostText"), 12, FLinearColor(0.7f, 0.6f, 1.f));
	DescText = MakeTooltipText(WidgetTree, TEXT("DescText"), 14, FLinearColor(1.f, 0.95f, 0.85f));

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
		TypeText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *TooltipTypeLabel(InSkill->SkillType), *TooltipTargetLabel(InSkill->TargetType))));
	}
	if (CostText)
	{
		FString Cost;
		if (InSkill->EnergyCost > 0)      Cost = FString::Printf(TEXT("에너지 %d"), InSkill->EnergyCost);
		if (InSkill->SkillEnergyCost > 0) Cost += FString::Printf(TEXT("%s강화 에너지 %d"), Cost.IsEmpty() ? TEXT("") : TEXT("  ·  "), InSkill->SkillEnergyCost);
		if (Cost.IsEmpty())               Cost = TEXT("비용 없음");
		CostText->SetText(FText::FromString(Cost));
	}
	if (DescText)
	{
		// DT 설명 중 일부에 줄바꿈이 "\n" 글자 그대로 들어가 있어서 진짜 줄바꿈으로
		DescText->SetText(FText::FromString(InSkill->Description_KR.ToString().Replace(TEXT("\\n"), TEXT("\n"))));
	}
}
