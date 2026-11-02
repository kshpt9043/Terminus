#include "Widgets/Skill/SkillTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Combat/SkillExecutor.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
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
	if (!StatusPanel)
	{
		BuildStatusPanel();
	}
}

void USkillTooltipWidget::BuildStatusPanel()
{
	UWidget* OldRoot = WidgetTree->RootWidget;
	if (!OldRoot) return;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = Row;
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OldRoot)) S->SetVerticalAlignment(VAlign_Top);

	StatusPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusPanel"));
	StatusPanel->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::Color::Board, TerminusUI::Color::Line));
	StatusPanel->SetPadding(FMargin(14.f, 12.f));
	StatusPanel->SetVisibility(ESlateVisibility::Collapsed);
	StatusText = TerminusUI::MakeText(WidgetTree, TEXT("StatusText"), TEXT(""), 14, TerminusUI::Color::TextBody, ETextJustify::Left);
	StatusText->SetAutoWrapText(false);
	StatusText->SetWrapTextAt(240.f);
	StatusPanel->SetContent(StatusText);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(StatusPanel))
	{
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	}
}

void USkillTooltipWidget::BuildDefaultLayout()
{
	// 스타일 가이드 06: 폭 320, 판 #1B1E25 + 1px #3A3F4A, 위쪽 3px 종류 색 띠
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(DefaultWidth);
	WidgetTree->RootWidget = Size;

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::Color::Board, TerminusUI::Color::Line));
	Box->SetPadding(FMargin(1.f));
	Size->SetContent(Box);

	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Outer);

	USizeBox* BandSize = WidgetTree->ConstructWidget<USizeBox>();
	BandSize->SetHeightOverride(3.f);
	TierBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TierBand"));
	TierBand->SetBrushColor(TerminusUI::Color::Line);
	BandSize->SetContent(TierBand);
	Outer->AddChildToVerticalBox(BandSize);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(Column)) S->SetPadding(FMargin(15.f, 13.f, 15.f, 15.f));

	NameText = TerminusUI::MakeWrapText(WidgetTree, TEXT("NameText"), TEXT(""), TerminusUI::FontSize::Subtitle, TerminusUI::Color::Gold, ETextJustify::Left);
	TerminusUI::SetBold(NameText);
	TypeText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TypeText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextAux, ETextJustify::Left);
	CostText = TerminusUI::MakeWrapText(WidgetTree, TEXT("CostText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextSub, ETextJustify::Left);
	DescText = TerminusUI::MakeWrapText(WidgetTree, TEXT("DescText"), TEXT(""), 15, TerminusUI::Color::TextBody, ETextJustify::Left);

	Column->AddChildToVerticalBox(NameText);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TypeText)) S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	Column->AddChildToVerticalBox(CostText);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TerminusUI::MakeGoldDivider(WidgetTree)))
	{
		S->SetPadding(FMargin(0.f, 10.f));
	}
	Column->AddChildToVerticalBox(DescText);
}

void USkillTooltipWidget::SetSkill(const FSkillRow* InSkill)
{
	if (!InSkill) return;

	if (NameText)
	{
		NameText->SetText(InSkill->DisplayName_KR);
	}
	if (TierBand)
	{
		TierBand->SetBrushColor(TerminusUI::SkillTypeColor(InSkill->SkillType));
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

	// 상태이상 창: "허약(얼음)\n수치만큼 공격 스텟이 하락한다." 를 상태마다
	if (StatusPanel)
	{
		FString Lines;
		for (const EStatusEffect Type : FSkillExecutor::GetAppliedStatuses(*InSkill))
		{
			const FString Desc = GetStatusDescription(Type);
			if (Desc.IsEmpty()) continue;
			if (!Lines.IsEmpty()) Lines += TEXT("\n\n");
			Lines += FString::Printf(TEXT("[%s] %s\n%s"), IsStatusDebuff(Type) ? TEXT("해로움") : TEXT("이로움"), *GetStatusDisplayName(Type), *Desc);
		}
		if (StatusText) StatusText->SetText(FText::FromString(Lines));
		StatusPanel->SetVisibility(Lines.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
