// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerController.h"

#include "Widgets/TavernWidget.h"
#include "Blueprint/UserWidget.h"
#include "Game/TavernGameMode.h"
#include "Player/TerminusPlayerState.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Map/MapManager.h"
#include "Dungeon/DungeonArea.h"
#include "Dungeon/DungeonCombatComponent.h"
#include "Widgets/Combat/CombatHUDWidget.h"
#include "EngineUtils.h"


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
		TavernWidget->RemoveFromParent();
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
	
	// 구역에 들어가 있는 중이면 그 구역 카메라 유지 (재빙의로 지도 카메라에 뺏기지 않게)
	if (ADungeonArea* Area = ViewedArea.Get())
	{
		SetViewTarget(Area);
		return;
	}

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

void ATerminusPlayerController::Server_RequestSelectRoom_Implementation(int32 RoomId)
{
	AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	if (!MapMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("PC: 방 선택 요청을 받았는데 레벨에 MapManager 가 없음"));
		return;
	}
	
	MapMgr->HandleSelectRoomRequest(this, RoomId);
}

void ATerminusPlayerController::Client_OnRoomSelectFailed_Implementation(const FString& ReasonMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("[MapManager] 선택 실패: %s"), *ReasonMessage);
	// TODO: 화면에 인원 초과/실패 팝업 UI 생성 및 메시지 출력
}

void ATerminusPlayerController::ViewDungeonArea(ADungeonArea* Area)
{
	if (!IsLocalController())
	{
		return;
	}

	ViewedArea = Area;

	// 구역은 카메라 컴포넌트를 들고 있어서 액터 자체를 시점 대상으로 주면 그 카메라로 봄
	if (Area)
	{
		SetViewTarget(Area);
	}
	else if (ACameraActor* Fixed = FindFixedCamera())
	{
		SetViewTarget(Fixed);
	}

	// 전투 HUD: 구역에 들어가면 그 구역을 넘김. 전투 방일 때만 HUD 가 스스로 보임
	if (Area && !CombatHUD)
	{
		TSubclassOf<UCombatHUDWidget> HUDClass = CombatHUDClass ? CombatHUDClass : TSubclassOf<UCombatHUDWidget>(UCombatHUDWidget::StaticClass());
		CombatHUD = CreateWidget<UCombatHUDWidget>(this, HUDClass);
		if (CombatHUD)
		{
			CombatHUD->AddToViewport(10);   // 지도 위젯보다 위
		}
	}
	if (CombatHUD)
	{
		CombatHUD->SetArea(Area);
	}

	OnViewAreaChanged.Broadcast(Area);
}

void ATerminusPlayerController::Server_UseSkill_Implementation(int32 SkillIndex, int32 TargetIndex)
{
	ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	ADungeonArea* Area = PS ? PS->GetCurrentArea() : nullptr;
	if (UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr)
	{
		Combat->HandleUseSkill(PS, SkillIndex, TargetIndex);
	}
}

void ATerminusPlayerController::Server_EndTurn_Implementation()
{
	ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	ADungeonArea* Area = PS ? PS->GetCurrentArea() : nullptr;
	if (UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr)
	{
		Combat->HandleEndTurn(PS);
	}
}

void ATerminusPlayerController::DebugWinCombat()
{
	Server_DebugWinCombat();
}

void ATerminusPlayerController::Server_DebugWinCombat_Implementation()
{
	const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	ADungeonArea* Area = PS ? PS->GetCurrentArea() : nullptr;
	UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr;
	if (!Combat || !Combat->IsInCombat())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] 전투 중이 아님"));
		return;
	}

	Combat->DebugKillAllMonsters();
}

void ATerminusPlayerController::DebugClearArea(bool bAll)
{
	Server_DebugClearArea(bAll);
}

void ATerminusPlayerController::Server_DebugClearArea_Implementation(bool bAll)
{
	if (bAll)
	{
		// 마지막 구역이 클리어되는 순간 전부 해제되므로 목록을 먼저 떠 둠
		TArray<ADungeonArea*> InUse;
		for (TActorIterator<ADungeonArea> It(GetWorld()); It; ++It)
		{
			if (It->IsInUse()) InUse.Add(*It);
		}

		for (ADungeonArea* Area : InUse)
		{
			Area->MarkCleared();
		}
		return;
	}

	const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	ADungeonArea* Area = PS ? PS->GetCurrentArea() : nullptr;

	if (!Area)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] 구역에 들어가 있지 않음"));
		return;
	}

	Area->MarkCleared();
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
