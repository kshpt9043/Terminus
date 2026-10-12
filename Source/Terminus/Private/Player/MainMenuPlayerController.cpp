#include "Player/MainMenuPlayerController.h"

#include "Widgets/MainMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/TerminusUISettings.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) { return; }

	// BP 에 지정한 값 -> 프로젝트 설정(Terminus UI) 순서. 메인 메뉴는 C++ 기본 모양이 없어 둘 다 비면 못 띄움
	const TSubclassOf<UMainMenuWidget> MenuClass = MainMenuClass ? MainMenuClass : TSubclassOf<UMainMenuWidget>(UTerminusUISettings::Get()->MainMenu.LoadSynchronous());
	if (!MenuClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MainMenuPC: 메인 메뉴 클래스가 비어 있음. 프로젝트 설정 > Terminus UI > Main Menu 에 WBP_MainMenu 를 지정할 것"));
		return;
	}

	MainMenu = CreateWidget<UMainMenuWidget>(this, MenuClass);
	if (!MainMenu) { return; }
	MainMenu->AddToViewport();

	FInputModeUIOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = true;
}