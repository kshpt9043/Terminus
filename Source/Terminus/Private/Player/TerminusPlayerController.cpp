// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerController.h"

#include "Widgets/TavernWidget.h"
#include "Blueprint/UserWidget.h"
#include "Game/TavernGameMode.h"
#include "Player/TerminusPlayerState.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"


void ATerminusPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// 다른 플레이어의 컨트롤러는 리턴
	if (!IsLocalController())
	{
		return;
	}
	
	// 한 틱 뒤에 BeginPlay에서 카메라 액터 찾아서 맵 잡기
	GetWorldTimerManager().SetTimerForNextTick(this, &ATerminusPlayerController::ApplyFixedCamera);
	
	if (!TavernWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PC: TavernWidgetClass 가 비어 있음. BP 디폴트에서 WBP_TavernWidget 을 지정할 것"));
		return;
	}
	
	TavernWidget = CreateWidget<UTavernWidget>(this, TavernWidgetClass);
	if (!TavernWidget)
	{
		return;
	}
	TavernWidget->AddToViewport();
	
	FInputModeUIOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void ATerminusPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TavernWidget)
	{
		TavernWidget->RemoveFromViewport();
		TavernWidget = nullptr;
	}
	
	Super::EndPlay(EndPlayReason);
}

void ATerminusPlayerController::ApplyFixedCamera()
{
	TArray<AActor*> Cameras;
	UGameplayStatics::GetAllActorsOfClass(this, ACameraActor::StaticClass(), Cameras);

	for (AActor* Actor : Cameras)
	{
		const ACameraActor* Cam = Cast<ACameraActor>(Actor);
		if (!Cam || Cam->GetAutoActivatePlayerIndex() != 0)
		{
			continue;
		}
		
		// 기존 컨트롤러는 빙의할때마다 시점을 폰으로 잡아서, 던전에서는 그 관리를 꺼야함
		bAutoManageActiveCameraTarget = false;
		SetViewTarget(Actor);
		return;
	}

	// 카메라가 없는 레벨(주점)에서는 아무것도 안 한다. 엔진 기본 동작 유지
}

void ATerminusPlayerController::Server_StartGame_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("PC: 게임 시작 요청 수락"));
	
	if (ATavernGameMode* GM = GetWorld()->GetAuthGameMode<ATavernGameMode>())
	{
		GM->TryStartGame();
	}
}

void ATerminusPlayerController::Server_SelectCharacter_Implementation(ECharacterClass InClass)
{
	if (InClass >= ECharacterClass::MAX)
	{
		return;
	}
	
	ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	if (!PS)
	{
		return;
	}
	
	// 준비 완료 상태면 캐릭터 변경 무시. 클라 UI 도 막지만 여기가 진짜 관문
	if (PS->IsReady())
	{
		return;
	}
	
	PS->SetCharacterClass(InClass);
}

void ATerminusPlayerController::Server_SetReady_Implementation(bool bInReady)
{
	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->SetReady(bInReady);
	}
}
