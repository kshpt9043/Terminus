#include "Widgets/Rift/RiftWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Online/SessionSubsystem.h"
#include "Widgets/Common/ConfirmPopupWidget.h"

namespace
{
	UTextBlock* MakeRiftText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void AddRiftCentered(UVerticalBox* Column, UWidget* Widget, float Top)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Widget))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		}
	}
}

void URiftWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (LeaveButton) LeaveButton->OnClicked.AddDynamic(this, &URiftWidget::HandleLeaveClicked);

	// 아래 지도 / 전투 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void URiftWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RiftDim"));
	Dim->SetBrushColor(FLinearColor(0.05f, 0.02f, 0.09f, 0.93f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = MakeRiftText(WidgetTree, TEXT("TitleText"), TEXT("이공간"), 34, FLinearColor(0.75f, 0.55f, 1.f));
	AddRiftCentered(Column, TitleText, 0.f);

	MessageText = MakeRiftText(WidgetTree, TEXT("MessageText"),
		TEXT("동료와의 연결이 끊겨 이공간에 빠졌습니다.\n싸우던 방은 없던 일이 되었습니다. 동료가 돌아오면 지도로 돌아갑니다."),
		16, FLinearColor(0.85f, 0.85f, 0.85f));
	AddRiftCentered(Column, MessageText, 16.f);

	WaitingText = MakeRiftText(WidgetTree, TEXT("WaitingText"), FString(), 18, FLinearColor(1.f, 0.85f, 0.35f));
	AddRiftCentered(Column, WaitingText, 20.f);

	UTextBlock* Hint = MakeRiftText(WidgetTree, NAME_None,
		TEXT("나간 동료는 주점 목록의 '재합류 대기' 방으로 돌아올 수 있습니다."), 13, FLinearColor(0.6f, 0.6f, 0.6f));
	AddRiftCentered(Column, Hint, 8.f);

	LeaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LeaveButton"));
	UTextBlock* LeaveLabel = MakeRiftText(WidgetTree, NAME_None, TEXT("  던전 나가기  "), 15, FLinearColor::Black);
	LeaveButton->SetContent(LeaveLabel);
	AddRiftCentered(Column, LeaveButton, 32.f);
}

void URiftWidget::SetWaiting(const TArray<FString>& WaitingFor)
{
	if (WaitingText)
	{
		WaitingText->SetText(FText::FromString(FString::Printf(TEXT("기다리는 중: %s"), *FString::Join(WaitingFor, TEXT(", ")))));
	}
}

void URiftWidget::HandleLeaveClicked()
{
	const APlayerController* PC = GetOwningPlayer();
	const bool bIsHost = PC && PC->HasAuthority();

	const TSubclassOf<UConfirmPopupWidget> Class = PopupClass ? PopupClass : TSubclassOf<UConfirmPopupWidget>(UConfirmPopupWidget::StaticClass());
	UConfirmPopupWidget* Popup = CreateWidget<UConfirmPopupWidget>(GetOwningPlayer(), Class);
	if (!Popup)
	{
		HandleLeaveConfirmed();
		return;
	}

	Popup->Setup(FText::FromString(TEXT("던전 나가기")),
		FText::FromString(bIsHost
			? TEXT("방장이 나가면 던전이 끝나고 모두 메인 화면으로 돌아갑니다. 나갈까요?")
			: TEXT("던전을 나갈까요? 나가면 다른 동료들도 이공간에서 기다리게 됩니다.")),
		FText::FromString(TEXT("나가기")), FText::FromString(TEXT("취소")));
	Popup->OnConfirmedNative.BindUObject(this, &URiftWidget::HandleLeaveConfirmed);
	Popup->AddToViewport(50);
}

void URiftWidget::HandleLeaveConfirmed()
{
	if (USessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<USessionSubsystem>() : nullptr)
	{
		Sessions->LeaveToMenu();
	}
}
