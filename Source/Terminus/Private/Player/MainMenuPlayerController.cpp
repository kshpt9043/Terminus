#include "Player/MainMenuPlayerController.h"

#include "Widgets/MainMenuWidget.h"
#include "Blueprint/UserWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) { return; }

	if (!MainMenuClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MainMenuPC: MainMenuClass 가 비어 있음. BP 디폴트에서 WBP_MainMenu 를 지정할 것"));
		return;
	}

	MainMenu = CreateWidget<UMainMenuWidget>(this, MainMenuClass);
	if (!MainMenu) { return; }
	MainMenu->AddToViewport();

	FInputModeUIOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = true;
}