#include "Widgets/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Online/SessionSubsystem.h"
#include "Widgets/TerminusUIColors.h"

namespace
{
	const FLinearColor TextGold     = TerminusUI::Hex(TEXT("E6C47A"));
	const FLinearColor TextDim      = TerminusUI::Hex(TEXT("8D8778"));   // 게임 종료 기본색
	const FLinearColor TextDisabled = TerminusUI::Hex(TEXT("5D5A52"));
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// NativeConstruct 는 여러 번 불릴 수 있어서 버튼 바인딩은 여기서 한 번만 (주점과 같은 규칙)
	Btn_Dungeon->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleDungeonClicked);
	Btn_Tavern->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleTavernClicked);
	Btn_Quit->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);
	Btn_Quit->OnHovered.AddDynamic(this, &UMainMenuWidget::HandleQuitHovered);
	Btn_Quit->OnUnhovered.AddDynamic(this, &UMainMenuWidget::HandleQuitUnhovered);
	Btn_PopupOK->OnClicked.AddDynamic(this, &UMainMenuWidget::HandlePopupOKClicked);

	// 아직 기능이 없는 건물은 자리만 두고 잠근다
	for (UButton* Unready : { Btn_Tower.Get(), Btn_Training.Get() })
	{
		if (!Unready) { continue; }
		Unready->SetIsEnabled(false);
		SetButtonTextColor(Unready, TextDisabled);
	}
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	DisconnectPopup->SetVisibility(ESlateVisibility::Collapsed);
	SetButtonTextColor(Btn_Quit, TextDim);

	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	// 서브시스템은 위젯보다 오래 산다 -> Construct 마다 붙이고 Destruct 에서 뗀다
	Sessions->OnHostComplete.AddUniqueDynamic(this, &UMainMenuWidget::HandleHostComplete);

	// 끊겨서 돌아온 거면 이유를 보여준다. 꺼내면 비워지므로 한 번만 뜬다
	const FText Reason = Sessions->ConsumeDisconnectReason();
	if (!Reason.IsEmpty())
	{
		ShowPopup(Reason, FText::FromString(TEXT("메인 화면으로 돌아왔습니다.")));
	}
}

void UMainMenuWidget::NativeDestruct()
{
	if (USessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostComplete.RemoveDynamic(this, &UMainMenuWidget::HandleHostComplete);
	}

	Super::NativeDestruct();
}

void UMainMenuWidget::HandleDungeonClicked()
{
	// 싱글은 세션 없이 주점 레벨을 연다 -> NM_Standalone.
	// 주점을 거쳐야 ServerTravel(CopyProperties)로 RunState 가 던전까지 간다
	UGameplayStatics::OpenLevel(this, FName(*TavernMapPath));
}

void UMainMenuWidget::HandleTavernClicked()
{
	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	// 세션 생성은 비동기라 기다리는 동안 연타를 막는다
	SetMenuEnabled(false);
	Sessions->HostSession(MaxPartySize, TavernMapPath);
}

void UMainMenuWidget::HandleHostComplete(bool bWasSuccessful)
{
	// 성공이면 서브시스템이 주점으로 ServerTravel 한다. 이 위젯은 곧 사라짐
	if (bWasSuccessful) { return; }

	SetMenuEnabled(true);
	ShowPopup(FText::FromString(TEXT("주점을 열지 못했습니다.")),
		FText::FromString(TEXT("Steam이 켜져 있는지 확인한 뒤 다시 시도하세요.")));
}

void UMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::HandlePopupOKClicked()
{
	DisconnectPopup->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainMenuWidget::HandleQuitHovered()   { SetButtonTextColor(Btn_Quit, TextGold); }
void UMainMenuWidget::HandleQuitUnhovered() { SetButtonTextColor(Btn_Quit, TextDim); }

void UMainMenuWidget::ShowPopup(const FText& Title, const FText& Body)
{
	if (PopupTitle) { PopupTitle->SetText(Title); }
	PopupBody->SetText(Body);

	// 팝업 뒤를 덮는 검은 이미지가 클릭을 막아준다
	DisconnectPopup->SetVisibility(ESlateVisibility::Visible);
}

void UMainMenuWidget::SetMenuEnabled(bool bEnabled)
{
	Btn_Dungeon->SetIsEnabled(bEnabled);
	Btn_Tavern->SetIsEnabled(bEnabled);
}

void UMainMenuWidget::SetButtonTextColor(UButton* Button, const FLinearColor& Color)
{
	// 버튼 바로 아래 TextBlock 하나만 있는 구조라 GetContent 로 바로 꺼낸다
	if (UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr)
	{
		Label->SetColorAndOpacity(FSlateColor(Color));
	}
}

USessionSubsystem* UMainMenuWidget::GetSessions() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USessionSubsystem>() : nullptr;
}