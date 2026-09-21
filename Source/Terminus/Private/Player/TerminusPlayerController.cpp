// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerController.h"

#include "Widgets/TavernWidget.h"
#include "Blueprint/UserWidget.h"
#include "Game/TavernGameMode.h"
#include "Player/TerminusPlayerState.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"


void ATerminusPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// 다른 플레이어의 컨트롤러는 리턴
	if (!IsLocalController())
	{
		return;
	}
	
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

void ATerminusPlayerController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
	ACameraActor* Fixed = FindFixedCamera();
	
	// 안되면 이 줄만 보면 됨. 카메라를 찾았냐 못찾았냐가 전부임
	UE_LOG(LogTemp, Warning, TEXT("[FixedCam] World=%s Local=%d Found=%s"),
		*GetWorld()->GetName(),
		IsLocalController() ? 1 : 0,
		*GetNameSafe(Fixed));
	
	// 엔진은 빙의할 때마다 시점을 폰으로 잡으려함 -> 2.5D는 폰 말고 카메라를 봐야함
	// 빙의마다 불려서 트래블로 레벨이 바뀌어도 다시 걸림. BeginPlay 처럼 한 번만 도는게 아님
	if (Fixed)
	{
		SetViewTarget(Fixed);
		return;
	}
	
	// 카메라 없는 레벨(주점)은 엔진 기본 동작 그대로
	Super::AutoManageActiveCameraTarget(SuggestedTarget);
}

ACameraActor* ATerminusPlayerController::FindFixedCamera() const
{
	TArray<AActor*> Cameras;
	UGameplayStatics::GetAllActorsOfClass(this, ACameraActor::StaticClass(), Cameras);
	
	for (AActor* Actor : Cameras)
	{
		ACameraActor* Cam = Cast<ACameraActor>(Actor);
		
		// 레벨에서 Auto Activate 켜둔 카메라 = 그 레벨의 고정 카메라
		// 인덱스 0만 보면 호스트만 걸려서 INDEX_NONE 만 걸러냄
		if (Cam && Cam->GetAutoActivatePlayerIndex() != INDEX_NONE)
		{
			return Cam;
		}
	}
	
	return nullptr;
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
