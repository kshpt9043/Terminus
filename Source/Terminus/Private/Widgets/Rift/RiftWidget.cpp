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
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/TerminusUILayers.h"
#include "Widgets/TerminusUISettings.h"

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
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("RiftDim"), FLinearColor(0.05f, 0.02f, 0.09f, 0.93f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT("이공간"), 34, FLinearColor(0.75f, 0.55f, 1.f));
	TerminusUI::AddRow(Column, TitleText, 0.f);

	MessageText = TerminusUI::MakeWrapText(WidgetTree, TEXT("MessageText"), TEXT("동료와의 연결이 끊겨 이공간에 빠졌습니다.\n싸우던 방은 없던 일이 되었습니다. 동료가 돌아오면 지도로 돌아갑니다."), 16, TerminusUI::Color::TextSoft);
	TerminusUI::AddRow(Column, MessageText, 16.f);

	WaitingText = TerminusUI::MakeWrapText(WidgetTree, TEXT("WaitingText"), FString(), 18, TerminusUI::Color::Gold);
	TerminusUI::AddRow(Column, WaitingText, 20.f);

	UTextBlock* Hint = TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("나간 동료는 주점 목록의 '재합류 대기' 방으로 돌아올 수 있습니다."), 13, FLinearColor(0.6f, 0.6f, 0.6f));
	TerminusUI::AddRow(Column, Hint, 8.f);

	LeaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LeaveButton"));
	UTextBlock* LeaveLabel = TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("  던전 나가기  "), 15, FLinearColor::Black);
	LeaveButton->SetContent(LeaveLabel);
	TerminusUI::AddRow(Column, LeaveButton, 32.f);
}

void URiftWidget::SetFromSave(bool bFromSave)
{
	if (!MessageText) return;

	MessageText->SetText(FText::FromString(bFromSave
		? TEXT("세이브를 불러와 이공간에서 동료를 기다립니다.\n세이브 당시 동료가 모두 들어오면 지도로 갑니다.")
		: TEXT("동료와의 연결이 끊겨 이공간에 빠졌습니다.\n싸우던 방은 없던 일이 되었습니다. 동료가 돌아오면 지도로 돌아갑니다.")));
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

	const TSubclassOf<UConfirmPopupWidget> Class = UTerminusUISettings::Pick(PopupClass, &UTerminusUISettings::ConfirmPopup);
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
	Popup->AddToViewport(TerminusUI::Layer::Popup);
}

void URiftWidget::HandleLeaveConfirmed()
{
	if (USessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<USessionSubsystem>() : nullptr)
	{
		Sessions->LeaveToMenu();
	}
}
