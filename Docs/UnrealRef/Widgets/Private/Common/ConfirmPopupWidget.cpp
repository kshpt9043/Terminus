#include "Widgets/Common/ConfirmPopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

void UConfirmPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UConfirmPopupWidget::HandleConfirmClicked);
	if (CancelButton)  CancelButton->OnClicked.AddDynamic(this, &UConfirmPopupWidget::HandleCancelClicked);

	// 뒤 화면을 못 누르게 전체가 클릭을 받고, Enter / Esc 를 받으려고 포커스도 받음
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UConfirmPopupWidget::BuildDefaultLayout()
{
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("PopupDim"), TerminusUI::Color::Dim);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetMinDesiredWidth(420.f);
	Size->SetMaxDesiredWidth(560.f);
	Dim->SetContent(Size);

	UBorder* Box = TerminusUI::MakePanel(WidgetTree, TEXT("PopupBox"), TerminusUI::Color::Board, FMargin(32.f, 24.f));
	Size->SetContent(Box);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Column);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT(""), TerminusUI::FontSize::Section, TerminusUI::Color::Gold);
	TerminusUI::SetBold(TitleText);
	MessageText = TerminusUI::MakeWrapText(WidgetTree, TEXT("MessageText"), TEXT(""), 16, TerminusUI::Color::TextSoft);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Fill);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Buttons))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
	}

	// 확인 = 주 행동 육각 버튼, 취소 = 평평한 버튼
	auto MakeButton = [&](const FName& ButtonName, const FName& LabelName, TObjectPtr<UButton>& OutButton, TObjectPtr<UTextBlock>& OutLabel, bool bPrimary)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		if (bPrimary) TerminusUI::StyleHexButton(OutButton, TerminusUI::EHexButton::Primary);
		else          TerminusUI::StyleFlatButton(OutButton);
		OutLabel = TerminusUI::MakeWrapText(WidgetTree, LabelName, TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::TextBody);
		TerminusUI::SetBold(OutLabel);
		OutButton->SetContent(OutLabel);
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(OutButton))
		{
			S->SetPadding(FMargin(6.f, 0.f));
		}
	};
	MakeButton(TEXT("ConfirmButton"), TEXT("ConfirmLabel"), ConfirmButton, ConfirmLabel, true);
	MakeButton(TEXT("CancelButton"), TEXT("CancelLabel"), CancelButton, CancelLabel, false);
}

void UConfirmPopupWidget::Setup(const FText& InTitle, const FText& InMessage, const FText& InConfirmLabel, const FText& InCancelLabel)
{
	bHasCancel = !InCancelLabel.IsEmpty();

	if (TitleText)
	{
		TitleText->SetText(InTitle);
		TitleText->SetVisibility(InTitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (MessageText)
	{
		MessageText->SetAutoWrapText(true);   // WBP 에서 꺼 두었어도 긴 안내가 창 밖으로 안 나가게
		MessageText->SetText(InMessage);
		MessageText->SetVisibility(InMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ConfirmLabel)
	{
		ConfirmLabel->SetText(InConfirmLabel.IsEmpty() ? FText::FromString(TEXT("확인")) : InConfirmLabel);
	}
	if (CancelLabel)
	{
		CancelLabel->SetText(InCancelLabel);
	}
	if (CancelButton)
	{
		CancelButton->SetVisibility(bHasCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// ESC = 취소 (알림처럼 취소 버튼이 없으면 확인)
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateWeakLambda(this, [this]() { Close(!bHasCancel); }));
	}
}

FReply UConfirmPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		Close(true);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UConfirmPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 바탕 클릭은 먹기만 함 (뒤 화면으로 안 새게)
	return FReply::Handled();
}

void UConfirmPopupWidget::HandleConfirmClicked() { Close(true); }
void UConfirmPopupWidget::HandleCancelClicked()  { Close(false); }

void UConfirmPopupWidget::Close(bool bConfirmed)
{
	if (bClosed) return;
	bClosed = true;

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	// 먼저 닫고 나서 알림 (알림을 받은 쪽이 레벨 이동 같은 걸 해도 안전하게)
	RemoveFromParent();

	if (bConfirmed)
	{
		OnConfirmedNative.ExecuteIfBound();
		OnConfirmed.Broadcast();
	}
	else
	{
		OnCancelledNative.ExecuteIfBound();
		OnCancelled.Broadcast();
	}
}
