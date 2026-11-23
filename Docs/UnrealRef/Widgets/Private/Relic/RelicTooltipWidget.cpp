#include "Widgets/Relic/RelicTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/RelicTypes.h"
#include "Engine/Texture2D.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/Common/TerminusUIText.h"
#include "Widgets/TerminusUIColors.h"

void URelicTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
}

void URelicTooltipWidget::BuildDefaultLayout()
{
	// 스킬 툴팁과 같은 모양 (스타일 가이드 06: 폭 320, 판 #1B1E25 + 1px #3A3F4A, 위쪽 3px 등급 띠)
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
	TierText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TierText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextAux, ETextJustify::Left);
	DescText = TerminusUI::MakeWrapText(WidgetTree, TEXT("DescText"), TEXT(""), 15, TerminusUI::Color::TextBody, ETextJustify::Left);

	Column->AddChildToVerticalBox(NameText);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TierText)) S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TerminusUI::MakeGoldDivider(WidgetTree)))
	{
		S->SetPadding(FMargin(0.f, 10.f));
	}
	Column->AddChildToVerticalBox(DescText);
}

void URelicTooltipWidget::SetRelic(const FRelicRow* InRelic)
{
	if (!InRelic) return;

	if (NameText)
	{
		// 이름은 금색, 등급은 띠 / 마름모 색으로
		NameText->SetText(InRelic->RelicName);
		NameText->SetColorAndOpacity(FSlateColor(TerminusUI::Color::Gold));
	}
	if (TierText)
	{
		TierText->SetText(FText::FromString(TEXT("◆ ") + TerminusUI::RelicTierLabel(InRelic->RelicTier) + TEXT(" 유물")));
		TierText->SetColorAndOpacity(FSlateColor(TerminusUI::RelicTierColor(InRelic->RelicTier)));
	}
	if (TierBand)
	{
		TierBand->SetBrushColor(TerminusUI::RelicTierColor(InRelic->RelicTier));
	}
	if (DescText)
	{
		// 스킬 설명처럼 "\n" 글자 그대로 들어간 줄바꿈을 진짜 줄바꿈으로
		DescText->SetText(FText::FromString(InRelic->RelicDesc.ToString().Replace(TEXT("\\n"), TEXT("\n"))));
	}
	if (IconImage)
	{
		UTexture2D* Texture = InRelic->Icon.LoadSynchronous();
		if (Texture) IconImage->SetBrushFromTexture(Texture);
		IconImage->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
