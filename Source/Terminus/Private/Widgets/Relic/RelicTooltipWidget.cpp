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
	// 스킬 툴팁과 같은 모양
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
	TierText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TierText"), TEXT(""), 12, TerminusUI::Color::TextMuted, ETextJustify::Left);
	DescText = TerminusUI::MakeWrapText(WidgetTree, TEXT("DescText"), TEXT(""), 14, FLinearColor(1.f, 0.95f, 0.85f), ETextJustify::Left);

	Column->AddChildToVerticalBox(NameText);
	Column->AddChildToVerticalBox(TierText);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(DescText))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
}

void URelicTooltipWidget::SetRelic(const FRelicRow* InRelic)
{
	if (!InRelic) return;

	if (NameText)
	{
		NameText->SetText(InRelic->RelicName);
		NameText->SetColorAndOpacity(FSlateColor(TerminusUI::RelicTierColor(InRelic->RelicTier)));
	}
	if (TierText)
	{
		TierText->SetText(FText::FromString(TerminusUI::RelicTierLabel(InRelic->RelicTier) + TEXT(" 유물")));
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
