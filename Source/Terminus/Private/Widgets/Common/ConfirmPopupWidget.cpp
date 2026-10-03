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

namespace
{
	UTextBlock* MakePopupText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	}
}

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
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PopupDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetMinDesiredWidth(420.f);
	Size->SetMaxDesiredWidth(560.f);
	Dim->SetContent(Size);

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PopupBox"));
	Box->SetBrushColor(FLinearColor(0.1f, 0.09f, 0.08f, 1.f));
	Box->SetPadding(FMargin(32.f, 24.f));
	Size->SetContent(Box);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Column);

	TitleText = MakePopupText(WidgetTree, TEXT("TitleText"), 22, FLinearColor::White);
	MessageText = MakePopupText(WidgetTree, TEXT("MessageText"), 16, FLinearColor(0.85f, 0.85f, 0.85f));
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

	auto MakeButton = [&](const FName& ButtonName, const FName& LabelName, TObjectPtr<UButton>& OutButton, TObjectPtr<UTextBlock>& OutLabel)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		OutLabel = MakePopupText(WidgetTree, LabelName, 16, FLinearColor::Black);
		OutButton->SetContent(OutLabel);
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(OutButton))
		{
			S->SetPadding(FMargin(6.f, 0.f));
		}
	};
	MakeButton(TEXT("ConfirmButton"), TEXT("ConfirmLabel"), ConfirmButton, ConfirmLabel);
	MakeButton(TEXT("CancelButton"), TEXT("CancelLabel"), CancelButton, CancelLabel);
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
