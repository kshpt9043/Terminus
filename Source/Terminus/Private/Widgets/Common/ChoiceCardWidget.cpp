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
	Inside->AddChildToVerticalBox(TitleText);

	ItemSlot = WidgetTree->ConstructWidget<UItemSlotWidget>(UItemSlotWidget::StaticClass(), TEXT("ItemSlot"));
	TerminusUI::AddRow(Inside, ItemSlot, 8.f);

	DescText = TerminusUI::MakeWrapText(WidgetTree, TEXT("DescText"), TEXT(""), Style.DescSize, TerminusUI::Color::TextBright);
	if (UVerticalBoxSlot* S = TerminusUI::AddRow(Inside, DescText, 8.f, HAlign_Fill))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	FooterText = TerminusUI::MakeWrapText(WidgetTree, TEXT("FooterText"), TEXT(""), 15, TerminusUI::Color::Gold);
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

	FLinearColor Color = Style.NormalColor;
	switch (State)
	{
	case EChoiceCardState::Selected:  Color = FLinearColor(0.55f, 0.5f, 0.35f); break;
	case EChoiceCardState::Highlight: Color = TerminusUI::Color::Gold; break;
	case EChoiceCardState::Danger:    Color = FLinearColor(0.85f, 0.3f, 0.28f); break;
	default: break;
	}
	ChoiceButton->SetBackgroundColor(Color);
}

void UChoiceCardWidget::HandleClicked()
{
	OnChosen.ExecuteIfBound();
}
