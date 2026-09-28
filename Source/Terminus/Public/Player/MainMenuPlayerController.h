#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

/**
 * 메인 화면 전용. 메뉴 위젯을 띄우고 UI 입력으로 바꾸는 것만 한다.
 * 주점/던전용 TerminusPlayerController 와 분리 -> 메뉴엔 서버 RPC 가 필요 없다
 */
UCLASS()
class TERMINUS_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UMainMenuWidget> MainMenuClass;

	UPROPERTY()
	TObjectPtr<UMainMenuWidget> MainMenu;
};