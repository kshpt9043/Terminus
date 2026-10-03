#include "Widgets/Skill/SkillCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TerminusDataSettings.h"

namespace
{
	UTextBlock* MakeCardText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	}

	FString SkillTypeLabel(ESkillType Type)
	{
		switch (Type)
		{
		case ESkillType::Attack:  return TEXT("공격");
		case ESkillType::Defense: return TEXT("방어");
		default:                  return TEXT("특수");
		}
	}

	FString TargetTypeLabel(ETargetType Type)
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

void USkillCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (CardButton)
	{
		CardButton->OnClicked.AddDynamic(this, &USkillCardWidget::HandleCardClicked);
	}
}

void USkillCardWidget::BuildDefaultLayout()
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
	Size->SetWidthOverride(DefaultCardSize.X);
	Size->SetHeightOverride(DefaultCardSize.Y);
	WidgetTree->RootWidget = Size;

	CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CardButton"));
	CardButton->SetBackgroundColor(FLinearColor(0.22f, 0.18f, 0.14f));
	Size->SetContent(CardButton);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	CardButton->SetContent(Box);

	auto Add = [Box](UWidget* Widget, float Top, bool bFill = false)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Widget))
		{
			S->SetPadding(FMargin(12.f, Top, 12.f, 0.f));
			S->SetHorizontalAlignment(HAlign_Fill);
			if (bFill) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};

	NameText = MakeCardText(WidgetTree, 22, FLinearColor::White);
	TypeText = MakeCardText(WidgetTree, 14, FLinearColor(0.8f, 0.8f, 0.8f));
	CostText = MakeCardText(WidgetTree, 15, FLinearColor(0.75f, 0.55f, 1.f));
	DescText = MakeCardText(WidgetTree, 15, FLinearColor(1.f, 0.95f, 0.85f));

	Add(NameText, 18.f);
	Add(TypeText, 6.f);
	Add(CostText, 4.f);
	Add(DescText, 20.f, true);
}

void USkillCardWidget::SetSkill(FName InSkillRow)
{
	SkillRow = InSkillRow;

	const FSkillRow* Row = UTerminusDataSettings::FindSkillRow(SkillRow);
	if (!Row)
	{
		if (NameText) NameText->SetText(FText::FromName(SkillRow));
		if (DescText) DescText->SetText(FText::FromString(TEXT("(DT_Skill 에 없는 스킬)")));
		return;
	}

	if (NameText)
	{
		NameText->SetText(Row->DisplayName_KR);
	}

	if (TypeText)
	{
		TypeText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *SkillTypeLabel(Row->SkillType), *TargetTypeLabel(Row->TargetType))));
	}

	if (CostText)
	{
		FString Cost = FString::Printf(TEXT("강화 에너지 %d"), Row->SkillEnergyCost);
		if (Row->EnergyCost > 0)
		{
			Cost += FString::Printf(TEXT("  ·  에너지 %d"), Row->EnergyCost);
		}
		CostText->SetText(FText::FromString(Cost));
	}

	if (DescText)
	{
		// DT 설명 중 일부에 줄바꿈이 "\n" 글자 그대로 들어가 있어서 진짜 줄바꿈으로
		DescText->SetText(FText::FromString(Row->Description_KR.ToString().Replace(TEXT("\\n"), TEXT("\n"))));
	}
}

void USkillCardWidget::HandleCardClicked()
{
	OnCardClicked.ExecuteIfBound(SkillRow);
}
