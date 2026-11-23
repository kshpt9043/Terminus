#include "Widgets/Common/ItemCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/TerminusUISettings.h"

void UItemCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ActionButton) ActionButton->OnClicked.AddDynamic(this, &UItemCardWidget::HandleAction);
}

void UItemCardWidget::BuildDefaultLayout()
{
	bDefaultLayout = true;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardColumn"));
	WidgetTree->RootWidget = Column;

	// 칸은 SlotClass 를 받은 뒤 첫 SetItem 에서 여기 만듦
	UVerticalBox* Holder = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotHolder"));
	SlotHolder = Holder;
	TerminusUI::AddRow(Column, Holder, 0.f);

	CaptionSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CaptionSizeBox"));
	CaptionText = TerminusUI::MakeWrapText(WidgetTree, TEXT("CaptionText"), TEXT(""), 13, TerminusUI::Color::TextMuted);
	CaptionSizeBox->SetContent(CaptionText);
	TerminusUI::AddRow(Column, CaptionSizeBox, 4.f);

	ActionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ActionButton"));
	TerminusUI::StyleFlatButton(ActionButton);
	ActionText = TerminusUI::MakeText(WidgetTree, TEXT("ActionText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextBody);
	TerminusUI::SetBold(ActionText);
	ActionButton->SetContent(ActionText);
	TerminusUI::AddRow(Column, ActionButton, 4.f);
}

void UItemCardWidget::SetItem(bool bInRelic, FName Row, FVector2D SlotSize, bool bShowName)
{
	bRelic = bInRelic;
	ItemRow = Row;

	if (!ItemSlot && SlotHolder)
	{
		const TSubclassOf<UItemSlotWidget> Class = UTerminusUISettings::Pick(SlotClass, &UTerminusUISettings::ItemSlot);
		ItemSlot = CreateWidget<UItemSlotWidget>(this, Class);
		if (ItemSlot) SlotHolder->AddChild(ItemSlot);
	}

	if (ItemSlot)
	{
		if (!SlotSize.IsZero()) ItemSlot->SetSlotSize(SlotSize);
		ItemSlot->SetShowName(bShowName);
		if (bRelic) ItemSlot->SetRelic(Row);
		else        ItemSlot->SetSkill(Row);
	}
	OnCardUpdated();
}

void UItemCardWidget::SetCaption(const FString& Caption)
{
	if (CaptionText)
	{
		TerminusUI::SetTextIfChanged(CaptionText, Caption);
		CaptionText->SetVisibility(Caption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	OnCardUpdated();
}

void UItemCardWidget::SetAction(const FString& Label, bool bEnabled, const FLinearColor& DefaultTint)
{
	if (ActionText) TerminusUI::SetTextIfChanged(ActionText, Label);
	if (ActionButton)
	{
		ActionButton->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ActionButton->SetIsEnabled(bEnabled);
		if (bDefaultLayout)
		{
			// 재화 바탕색이면 구매 육각 버튼, 아니면 그 색 바탕의 평평한 버튼 (흰색 = 테마 기본)
			if (DefaultTint.Equals(TerminusUI::Color::CurrencyFill, 0.001f)) TerminusUI::StyleHexButton(ActionButton, TerminusUI::EHexButton::Currency);
			else                                                              TerminusUI::StyleFlatButton(ActionButton, DefaultTint);
			if (ActionText) ActionText->SetColorAndOpacity(FSlateColor(bEnabled ? TerminusUI::Color::TextBody : TerminusUI::Color::TextFaint));
		}
	}
	OnCardUpdated();
}

void UItemCardWidget::SetActionLabelColor(const FLinearColor& Color)
{
	if (bDefaultLayout && ActionText) ActionText->SetColorAndOpacity(FSlateColor(Color));
}

void UItemCardWidget::SetCaptionStyle(float Width, int32 FontSize, const FLinearColor& Color)
{
	if (!bDefaultLayout) return;

	if (CaptionSizeBox && Width > 0.f) CaptionSizeBox->SetWidthOverride(Width);
	if (CaptionText)
	{
		FSlateFontInfo Font = CaptionText->GetFont();
		Font.Size = FontSize;
		CaptionText->SetFont(Font);
		CaptionText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UItemCardWidget::HandleAction()
{
	OnAction.ExecuteIfBound();
}
