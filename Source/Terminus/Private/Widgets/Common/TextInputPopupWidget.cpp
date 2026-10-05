#include "Widgets/Common/TextInputPopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "TimerManager.h"
#include "Widgets/Common/EscapeStackSubsystem.h"

namespace
{
	UTextBlock* MakeInputPopupText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
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

void UTextInputPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UTextInputPopupWidget::HandleConfirmClicked);
	if (CancelButton)  CancelButton->OnClicked.AddDynamic(this, &UTextInputPopupWidget::HandleCancelClicked);
	if (RandomButton)
	{
		RandomButton->OnClicked.AddDynamic(this, &UTextInputPopupWidget::HandleRandomClicked);
		RandomButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InputBox)
	{
		InputBox->OnTextChanged.AddDynamic(this, &UTextInputPopupWidget::HandleTextChanged);
		InputBox->OnTextCommitted.AddDynamic(this, &UTextInputPopupWidget::HandleTextCommitted);
	}

	// 뒤 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UTextInputPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면에 붙은 다음 틱에 입력칸으로 포커스 (바로 하면 아직 레이아웃이 없어 안 먹음)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (InputBox) InputBox->SetKeyboardFocus();
		}));
	}
}

void UTextInputPopupWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InputPopupDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(460.f);
	Dim->SetContent(Size);

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InputPopupBox"));
	Box->SetBrushColor(FLinearColor(0.1f, 0.09f, 0.08f, 1.f));
	Box->SetPadding(FMargin(32.f, 24.f));
	Size->SetContent(Box);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Column);

	TitleText = MakeInputPopupText(WidgetTree, TEXT("TitleText"), 22, FLinearColor::White);
	Column->AddChildToVerticalBox(TitleText);

	MessageText = MakeInputPopupText(WidgetTree, TEXT("MessageText"), 15, FLinearColor(0.8f, 0.8f, 0.8f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

	// 입력칸 + 랜덤
	UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(InputRow)) S->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));

	InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("InputBox"));
	if (UHorizontalBoxSlot* S = InputRow->AddChildToHorizontalBox(InputBox))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}

	RandomButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RandomButton"));
	RandomButton->SetContent(MakeInputPopupText(WidgetTree, NAME_None, 14, FLinearColor::Black));
	Cast<UTextBlock>(RandomButton->GetContent())->SetText(FText::FromString(TEXT(" 랜덤 ")));
	if (UHorizontalBoxSlot* S = InputRow->AddChildToHorizontalBox(RandomButton))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}

	// 확인 / 취소
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Buttons))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
	}

	auto MakeButton = [&](const FName& ButtonName, const FString& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UTextBlock* Text = MakeInputPopupText(WidgetTree, NAME_None, 16, FLinearColor::Black);
		Text->SetText(FText::FromString(Label));
		Button->SetContent(Text);
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(Button)) S->SetPadding(FMargin(6.f, 0.f));
		return Button;
	};
	ConfirmButton = MakeButton(TEXT("ConfirmButton"), TEXT("  확인  "));
	CancelButton  = MakeButton(TEXT("CancelButton"), TEXT("  취소  "));
}

void UTextInputPopupWidget::Setup(const FText& InTitle, const FText& InMessage, const FString& InDefaultText, int32 InMaxLength)
{
	DefaultText = InDefaultText;
	MaxLength = FMath::Max(1, InMaxLength);

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
	if (InputBox)
	{
		InputBox->SetText(FText::FromString(DefaultText.Left(MaxLength)));
	}

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateWeakLambda(this, [this]() { Close(false); }));
	}
}

void UTextInputPopupWidget::SetRandomProvider(TFunction<FString()> InProvider)
{
	RandomProvider = MoveTemp(InProvider);
	if (RandomButton)
	{
		RandomButton->SetVisibility(RandomProvider ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

FReply UTextInputPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 바탕 클릭은 먹기만 함 (뒤 화면으로 안 새게)
	return FReply::Handled();
}

void UTextInputPopupWidget::HandleTextChanged(const FText& Text)
{
	// 글자 수 제한
	if (InputBox && Text.ToString().Len() > MaxLength)
	{
		InputBox->SetText(FText::FromString(Text.ToString().Left(MaxLength)));
	}
}

void UTextInputPopupWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Close(true);
	}
}

void UTextInputPopupWidget::HandleRandomClicked()
{
	if (RandomProvider && InputBox)
	{
		InputBox->SetText(FText::FromString(RandomProvider().Left(MaxLength)));
	}
}

void UTextInputPopupWidget::HandleConfirmClicked() { Close(true); }
void UTextInputPopupWidget::HandleCancelClicked()  { Close(false); }

void UTextInputPopupWidget::Close(bool bConfirmed)
{
	if (bClosed) return;
	bClosed = true;

	FString Result = InputBox ? InputBox->GetText().ToString().TrimStartAndEnd().Left(MaxLength) : FString();
	if (Result.IsEmpty())
	{
		Result = RandomProvider ? RandomProvider().Left(MaxLength) : DefaultText;
	}

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	// 먼저 닫고 나서 알림 (받은 쪽이 레벨 이동을 해도 안전하게)
	RemoveFromParent();

	if (bConfirmed)
	{
		OnConfirmedText.ExecuteIfBound(Result);
	}
	else
	{
		OnCancelledNative.ExecuteIfBound();
	}
}
