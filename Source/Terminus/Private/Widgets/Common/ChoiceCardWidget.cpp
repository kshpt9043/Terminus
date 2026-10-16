#include "Widgets/Common/ChoiceCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

void UChoiceCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ChoiceButton) ChoiceButton->OnClicked.AddDynamic(this, &UChoiceCardWidget::HandleClicked);
	if (ItemSlot) ItemSlot->SetVisibility(ESlateVisibility::Collapsed);
}

void UChoiceCardWidget::BuildDefaultLayout()
{
	bDefaultLayout = true;

	ChoiceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ChoiceButton"));
	WidgetTree->RootWidget = ChoiceButton;

	CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSizeBox"));
	ChoiceButton->SetContent(CardSizeBox);

	UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
	CardSizeBox->SetContent(Inside);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT(""), Style.TitleSize, Style.TitleColor);
	TerminusUI::SetBold(TitleText);
	Inside->AddChildToVerticalBox(TitleText);

	ItemSlot = WidgetTree->ConstructWidget<UItemSlotWidget>(UItemSlotWidget::StaticClass(), TEXT("ItemSlot"));
	TerminusUI::AddRow(Inside, ItemSlot, 8.f);

	DescText = TerminusUI::MakeWrapText(WidgetTree, TEXT("DescText"), TEXT(""), Style.DescSize, TerminusUI::Color::TextBright);
	if (UVerticalBoxSlot* S = TerminusUI::AddRow(Inside, DescText, 8.f, HAlign_Fill))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	FooterText = TerminusUI::MakeWrapText(WidgetTree, TEXT("FooterText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextFaint);
	TerminusUI::AddRow(Inside, FooterText, 6.f, HAlign_Fill);

	ApplyDefaultStyle(Style);
}

void UChoiceCardWidget::ApplyDefaultStyle(const FChoiceCardStyle& InStyle)
{
	Style = InStyle;
	if (!bDefaultLayout) return;

	if (CardSizeBox)
	{
		CardSizeBox->ClearWidthOverride();
		CardSizeBox->ClearHeightOverride();
		CardSizeBox->ClearMinDesiredHeight();
		if (Style.Size.X > 0.f) CardSizeBox->SetWidthOverride(Style.Size.X);
		if (Style.Size.Y > 0.f)
		{
			if (Style.bFixedHeight) CardSizeBox->SetHeightOverride(Style.Size.Y);
			else                    CardSizeBox->SetMinDesiredHeight(Style.Size.Y);
		}
		if (USizeBoxSlot* Inner = CardSizeBox->GetContent() ? Cast<USizeBoxSlot>(CardSizeBox->GetContent()->Slot) : nullptr)
		{
			Inner->SetPadding(Style.Padding);
		}
	}

	auto ApplyText = [this](UTextBlock* Text, int32 Size)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetJustification(Style.Justify);
	};
	ApplyText(TitleText, Style.TitleSize);
	ApplyText(DescText, Style.DescSize);
	if (FooterText) FooterText->SetJustification(Style.Justify);
	if (TitleText) TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));

	// 칸은 가운데 정렬 카드면 가운데, 왼쪽 정렬 줄이면 왼쪽
	if (UVerticalBoxSlot* S = ItemSlot ? Cast<UVerticalBoxSlot>(ItemSlot->Slot) : nullptr)
	{
		S->SetHorizontalAlignment(Style.Justify == ETextJustify::Center ? HAlign_Center : HAlign_Left);
	}

	ApplyStateColor();
}

void UChoiceCardWidget::SetLine(UTextBlock* Text, const FString& Value)
{
	if (!Text) return;
	TerminusUI::SetTextIfChanged(Text, Value);
	Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UChoiceCardWidget::SetTexts(const FString& Title, const FString& Description, const FString& Footer)
{
	SetLine(TitleText, Title);
	SetLine(DescText, Description);
	SetLine(FooterText, Footer);
	OnCardUpdated();
}

void UChoiceCardWidget::SetItem(bool bRelic, FName Row)
{
	if (!ItemSlot) return;

	if (Row.IsNone())
	{
		ItemSlot->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		ItemSlot->SetShowName(false);
		if (bRelic) ItemSlot->SetRelic(Row);
		else        ItemSlot->SetSkill(Row);
		ItemSlot->SetVisibility(ESlateVisibility::HitTestInvisible);   // 칸은 보기만 (누르는 건 카드)
	}
	OnCardUpdated();
}

void UChoiceCardWidget::SetChoiceId(FName InId)
{
	ChoiceId = InId;
	OnCardUpdated();
}

void UChoiceCardWidget::SetState(EChoiceCardState InState)
{
	State = InState;
	ApplyStateColor();
	OnCardUpdated();
}

void UChoiceCardWidget::SetChoiceEnabled(bool bEnabled)
{
	if (ChoiceButton) ChoiceButton->SetIsEnabled(bEnabled);
}

void UChoiceCardWidget::ApplyStateColor()
{
	if (!bDefaultLayout || !ChoiceButton) return;

	using namespace TerminusUI;

	// 스타일 가이드 06: 카드 #262A32 + 1px #3A3F4A. 상태는 테두리 / 글자색으로
	FLinearColor Fill = Style.NormalColor;
	FLinearColor Outline = Color::Line;
	FLinearColor TitleColor = Style.TitleColor;
	FLinearColor FooterColor = Color::TextFaint;
	float Width = 1.f;
	switch (State)
	{
	case EChoiceCardState::Selected:  Outline = Color::GoldBase; TitleColor = Color::GoldHover; FooterColor = Color::Ready; Width = 2.f; break;
	case EChoiceCardState::Highlight: Outline = Color::GoldHover; Fill = Color::Disabled; TitleColor = Color::GoldHover; Width = 2.f; break;
	case EChoiceCardState::Danger:    Outline = Color::Alert; Width = 2.f; break;
	default: break;
	}

	// 여백은 CardSizeBox 쪽에서 -> 버튼 여백은 엔진 기본 그대로
	const FButtonStyle& EngineDefault = GetDefault<UButton>()->GetStyle();
	FButtonStyle ButtonStyle = MakeFlatButtonStyle(Fill);
	ButtonStyle.Normal = MakeBoxBrush(Fill, Outline, 6.f, Width);
	ButtonStyle.Hovered = MakeBoxBrush(Color::Disabled, State == EChoiceCardState::Normal ? Color::GoldPressed : Outline, 6.f, Width);
	ButtonStyle.Pressed = MakeBoxBrush(Color::Section, Color::GoldPressed, 6.f, Width);
	ButtonStyle.NormalPadding = EngineDefault.NormalPadding;
	ButtonStyle.PressedPadding = EngineDefault.PressedPadding;
	ChoiceButton->SetStyle(ButtonStyle);
	ChoiceButton->SetBackgroundColor(FLinearColor::White);

	if (TitleText) TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	if (FooterText) FooterText->SetColorAndOpacity(FSlateColor(FooterColor));
}

void UChoiceCardWidget::HandleClicked()
{
	OnChosen.ExecuteIfBound();
}
