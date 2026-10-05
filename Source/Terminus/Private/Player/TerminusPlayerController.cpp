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
#include "Widgets/Skill/StartSkillPickWidget.h"
#include "Widgets/Map/MapScreenWidget.h"
#include "Widgets/Common/ConfirmPopupWidget.h"
#include "Widgets/Chat/ChatWidget.h"
#include "Widgets/Relic/StartRelicPickWidget.h"
#include "Widgets/Reward/MonsterRewardWidget.h"
#include "Widgets/Rift/RiftWidget.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Data/TerminusDataSettings.h"
#include "TimerManager.h"
#include "EngineUtils.h"


void ATerminusPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// 다른 플레이어의 컨트롤러는 리턴
	if (!IsLocalController())
	{
		return;
	}

	// 커서: 화면 빈 곳을 눌러도 사라지지 않게 (레벨 BP 가 입력 모드를 안 정해 줘도)
	ApplyUIInputMode();

	// 던전이면 지도 보기 전에 강화 스킬 고르기. 주점에선 MapManager 가 없어 몇 초 확인하다 그만둠
	GetWorldTimerManager().SetTimer(StartSkillCheckTimer, this, &ATerminusPlayerController::CheckStartSkillPick, 0.25f, true, 0.f);

	// 주점 / 던전 어디서나 채팅 (멀티일 때만)
	CreateChatWidget();
	
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

void ATerminusPlayerController::ApplyUIInputMode(UWidget* FocusWidget)
{
	if (!IsLocalController()) return;

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (FocusWidget)
	{
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void ATerminusPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartSkillCheckTimer);

	if (StartSkillPick)
	{
		StartSkillPick->RemoveFromParent();
		StartSkillPick = nullptr;
	}

	if (MapScreen)
	{
		MapScreen->RemoveFromParent();
		MapScreen = nullptr;
	}

	if (ChatWidget)
	{
		ChatWidget->RemoveFromParent();
		ChatWidget = nullptr;
	}

	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->OnRunStateChanged.RemoveAll(this);
	}

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

void ATerminusPlayerController::Server_SetHardMode_Implementation(bool bInHardMode)
{
	// 방장 = 리슨 서버 자신의 PC (서버에서 로컬). 손님이 보낸 요청은 무시
	if (!IsLocalController()) return;

	if (ATavernGameMode* GM = GetWorld()->GetAuthGameMode<ATavernGameMode>())
	{
		GM->SetHardMode(bInHardMode);
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
	ShowPopup(FText::FromString(TEXT("방을 고를 수 없습니다")), FText::FromString(ReasonMessage), FText::FromString(TEXT("확인")), FText::GetEmpty());
}

void ATerminusPlayerController::CreateChatWidget()
{
	if (ChatWidget || !IsLocalController()) return;

	// 혼자 하는 오프라인(던전 입장 싱글)은 채팅할 사람이 없음
	if (GetNetMode() == NM_Standalone) return;

	const TSubclassOf<UChatWidget> Class = ChatWidgetClass ? ChatWidgetClass : TSubclassOf<UChatWidget>(UChatWidget::StaticClass());
	ChatWidget = CreateWidget<UChatWidget>(this, Class);
	if (ChatWidget)
	{
		ChatWidget->AddToViewport(25);   // 지도 / 전투 HUD / 스킬 고르기 위, 팝업(50) 아래
	}
}

void ATerminusPlayerController::Server_SendChat_Implementation(const FString& Message)
{
	// 클라가 보낸 값은 서버가 다시 다듬음
	FString Text = Message.TrimStartAndEnd().Left(200);
	Text.ReplaceCharInline(TEXT('\n'), TEXT(' '));
	Text.ReplaceCharInline(TEXT('\r'), TEXT(' '));
	if (Text.IsEmpty()) return;

	// 너무 빨리 연달아 보내면 버림 (도배 막기)
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (LastChatTime >= 0.0 && Now - LastChatTime < 0.3) return;
	LastChatTime = Now;

	FChatMessage Chat;
	Chat.Sender = PlayerState ? PlayerState->GetPlayerName() : TEXT("?");
	Chat.Text = Text;
	Chat.Kind = EChatMessageKind::Player;
	BroadcastChat(GetWorld(), Chat);
}

void ATerminusPlayerController::BroadcastChat(UWorld* World, const FChatMessage& Message)
{
	if (!World) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(It->Get()))
		{
			PC->Client_ReceiveChat(Message);
		}
	}
}

void ATerminusPlayerController::Client_ReceiveChat_Implementation(const FChatMessage& Message)
{
	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		Chat->AddMessage(Message);
	}
}

UConfirmPopupWidget* ATerminusPlayerController::ShowPopup(const FText& Title, const FText& Message, const FText& ConfirmLabel, const FText& CancelLabel)
{
	if (!IsLocalController())
	{
		return nullptr;
	}

	const TSubclassOf<UConfirmPopupWidget> Class = PopupClass ? PopupClass : TSubclassOf<UConfirmPopupWidget>(UConfirmPopupWidget::StaticClass());
	UConfirmPopupWidget* Popup = CreateWidget<UConfirmPopupWidget>(this, Class);
	if (!Popup)
	{
		return nullptr;
	}

	Popup->Setup(Title, Message, ConfirmLabel, CancelLabel);
	Popup->AddToViewport(50);   // 어떤 화면보다도 위
	Popup->SetUserFocus(this);  // Enter / Esc
	return Popup;
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

void ATerminusPlayerController::Client_EnterRift_Implementation(const TArray<FString>& WaitingFor)
{
	if (!Rift)
	{
		const TSubclassOf<URiftWidget> Class = RiftClass ? RiftClass : TSubclassOf<URiftWidget>(URiftWidget::StaticClass());
		Rift = CreateWidget<URiftWidget>(this, Class);
		if (Rift)
		{
			Rift->AddToViewport(24);   // 지도 / 전투 HUD / 고르기 / 보상 위, 채팅(25) 아래 -> 기다리면서 채팅 가능
		}
	}

	if (Rift)
	{
		Rift->SetWaiting(WaitingFor);
	}
}

void ATerminusPlayerController::Client_LeaveRift_Implementation()
{
	if (Rift)
	{
		Rift->RemoveFromParent();
		Rift = nullptr;
	}
}

void ATerminusPlayerController::Client_CloseMonsterReward_Implementation()
{
	if (MonsterReward)
	{
		MonsterReward->RemoveFromParent();
		MonsterReward = nullptr;
	}
}

void ATerminusPlayerController::Client_ShowMonsterReward_Implementation(int32 Currency, const TArray<FName>& SkillOffers)
{
	if (MonsterReward)
	{
		MonsterReward->RemoveFromParent();
		MonsterReward = nullptr;
	}

	const TSubclassOf<UMonsterRewardWidget> Class = MonsterRewardClass ? MonsterRewardClass : TSubclassOf<UMonsterRewardWidget>(UMonsterRewardWidget::StaticClass());
	MonsterReward = CreateWidget<UMonsterRewardWidget>(this, Class);
	if (MonsterReward)
	{
		MonsterReward->Setup(Currency, SkillOffers);
		MonsterReward->AddToViewport(22);   // 전투 HUD(10) / 스킬 고르기(20) 위, 채팅(25) 아래
	}
}

void ATerminusPlayerController::Server_FinishMonsterReward_Implementation(FName ChosenSkill, int32 ReplaceSlot)
{
	ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	ADungeonArea* Area = PS ? PS->GetCurrentArea() : nullptr;
	if (UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr)
	{
		Combat->HandleRewardFinished(PS, ChosenSkill, ReplaceSlot);
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

void ATerminusPlayerController::CheckStartSkillPick()
{
	ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	const bool bInDungeon = UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()) != nullptr;

	if (!PS || !bInDungeon)
	{
		// 주점 / 메뉴처럼 지도가 없는 레벨이면 10초쯤 보다가 그만둠
		if (++StartSkillCheckTries > 40)
		{
			GetWorldTimerManager().ClearTimer(StartSkillCheckTimer);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(StartSkillCheckTimer);

	// 고르기가 서버에서 확정될 때마다(복제로 RunState 가 바뀌면) 다음 단계로
	PS->OnRunStateChanged.AddUniqueDynamic(this, &ATerminusPlayerController::HandleLocalRunStateChanged);
	ContinueStartFlow(PS->GetRunState());
}

void ATerminusPlayerController::HandleLocalRunStateChanged(const FRunState& NewRunState)
{
	ContinueStartFlow(NewRunState);
}

void ATerminusPlayerController::ContinueStartFlow(const FRunState& Run)
{
	// 런 중간(방을 하나라도 지남)이거나 둘 다 골랐으면 지도
	if (Run.CurrentMapLevel > 0 || (Run.bStartSkillChosen && Run.bStartRelicsChosen))
	{
		OpenMapScreen();
		return;
	}

	if (!Run.bStartSkillChosen)
	{
		OpenStartSkillPick();
		return;
	}

	OpenStartRelicPick();
}

void ATerminusPlayerController::OpenStartRelicPick()
{
	if (bStartRelicPickOpened) return;
	bStartRelicPickOpened = true;

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();

	// 보관한 유물이 하나도 없으면 고를 게 없음 -> 바로 '안 고름'
	TArray<FName> Stored;
	if (Profile)
	{
		for (const FName& Row : Profile->GetStoredRelics())
		{
			if (UTerminusProfileSubsystem::IsStorableRelic(Row)) Stored.Add(Row);
		}
	}
	if (Stored.Num() == 0 || !PS)
	{
		Server_ChooseStartRelics({});
		return;
	}

	const TSubclassOf<UStartRelicPickWidget> Class = StartRelicPickClass ? StartRelicPickClass : TSubclassOf<UStartRelicPickWidget>(UStartRelicPickWidget::StaticClass());
	StartRelicPick = CreateWidget<UStartRelicPickWidget>(this, Class);
	if (StartRelicPick)
	{
		StartRelicPick->SetCandidates(Stored, PS->GetCharacterClass(), PS->IsHardMode());
		StartRelicPick->AddToViewport(20);   // 지도 / 전투 HUD 위
	}
}

void ATerminusPlayerController::Server_ChooseStartRelics_Implementation(const TArray<FName>& RelicRows)
{
	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->ChooseStartRelics(RelicRows);
	}
}

void ATerminusPlayerController::DebugShowStartRelicPick()
{
	if (StartRelicPick)
	{
		StartRelicPick->RemoveFromParent();
		StartRelicPick = nullptr;
	}
	bStartRelicPickOpened = false;

	// 같은 PC 의 Reliable RPC 라 순서 보장 -> 초기화 뒤 RunState 가 바뀌면 흐름이 다시 유물 고르기를 띄움
	Server_DebugResetStartRelics();
}

void ATerminusPlayerController::Server_DebugResetStartRelics_Implementation()
{
	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->ResetStartRelics();
	}
}

void ATerminusPlayerController::OpenMapScreen()
{
	if (MapScreen)
	{
		return;
	}

	if (!MapScreenClass)
	{
		UE_LOG(LogTemp, Error, TEXT("PC: MapScreenClass 가 비어 있어 지도를 못 띄움. BP_DungeonPC 에서 WBP_MapScreen 을 지정할 것"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("BP_DungeonPC 의 Map Screen Class 가 비어 있음 (WBP_MapScreen 지정)"));
		}
		return;
	}

	MapScreen = CreateWidget<UMapScreenWidget>(this, MapScreenClass);
	if (MapScreen)
	{
		MapScreen->AddToViewport(0);   // 맨 아래. 전투 HUD(10) / 고르기 화면(20) 이 위
	}
}

void ATerminusPlayerController::OpenStartSkillPick()
{
	if (StartSkillPick)
	{
		return;
	}

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	TArray<FName> Pool = (Profile && PS) ? Profile->GetUsableOwnedSkills(PS->GetCharacterClass()) : TArray<FName>();

	// 3개 이상이면 그 중 3장, 1~2개면 전부
	// 완전 첫판(보유 스킬 없음)이면 빈 채로 띄움 -> "보유한 스킬이 없습니다" 안내, 아무 키나 누르면 지도로
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		Pool.Swap(i, FMath::RandRange(0, i));
	}
	Pool.SetNum(FMath::Min(Pool.Num(), 3));

	const TSubclassOf<UStartSkillPickWidget> Class = StartSkillPickClass ? StartSkillPickClass : TSubclassOf<UStartSkillPickWidget>(UStartSkillPickWidget::StaticClass());
	StartSkillPick = CreateWidget<UStartSkillPickWidget>(this, Class);
	if (StartSkillPick)
	{
		StartSkillPick->SetChoices(Pool);
		StartSkillPick->AddToViewport(20);   // 지도 / 전투 HUD 보다 위

		// 아무 키 입력이 이 화면으로 오게 포커스. 마우스 커서는 그대로 (지도는 클릭으로 씀)
		ApplyUIInputMode(StartSkillPick);
	}
}

void ATerminusPlayerController::Server_ChooseStartSkill_Implementation(FName SkillRow)
{
	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->ChooseStartSkill(SkillRow);
	}
}

void ATerminusPlayerController::DebugOwnSkill(FName SkillRow)
{
	UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	if (!Profile) return;

	if (Profile->AddOwnedSkill(SkillRow))
	{
		UE_LOG(LogTemp, Log, TEXT("[Debug] 보유 스킬 추가: %s (보유 %d개)"), *SkillRow.ToString(), Profile->GetOwnedSkills().Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] '%s' 는 이미 보유 중이거나 강화 스킬이 아님"), *SkillRow.ToString());
	}
}

void ATerminusPlayerController::DebugOwnAllSkills()
{
	UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous();
	if (!Profile || !Table) return;

	for (const FName& Row : Table->GetRowNames())
	{
		Profile->AddOwnedSkill(Row);   // 강화 스킬이 아닌 행은 알아서 거름
	}
	UE_LOG(LogTemp, Log, TEXT("[Debug] 보유 스킬 %d개"), Profile->GetOwnedSkills().Num());
}

void ATerminusPlayerController::DebugClearOwnedSkills()
{
	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->ClearOwnedSkills();
		UE_LOG(LogTemp, Log, TEXT("[Debug] 보유 스킬 전부 삭제"));
	}
}

void ATerminusPlayerController::DebugShowStartSkillPick()
{
	// 같은 PC 의 Reliable RPC 라 순서가 지켜짐 -> 고른 결과는 초기화 뒤에 도착
	if (StartSkillPick)
	{
		StartSkillPick->RemoveFromParent();
		StartSkillPick = nullptr;
	}
	Server_DebugResetStartSkill();
	OpenStartSkillPick();
}

void ATerminusPlayerController::DebugGainRelic(FName RelicRow)
{
	Server_DebugGainRelic(RelicRow);
}

void ATerminusPlayerController::Server_DebugGainRelic_Implementation(FName RelicRow)
{
	ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	if (!PS) return;

	if (RelicRow != TEXT("All"))
	{
		PS->GainRelic(RelicRow);
		return;
	}

	// 가질 수 있는 것 전부 (Basic 은 직업 시작 유물이라 뺌. GainRelic 이 다른 직업 / 몬스터 것을 거름)
	if (const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous())
	{
		for (const FName& Row : Table->GetRowNames())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			if (Relic && Relic->RelicTier != ERelicTier::Basic && Relic->OwnerClass != ESkillOwner::Monster)
			{
				PS->GainRelic(Row);
			}
		}
	}
}

void ATerminusPlayerController::DebugRemoveRelic(FName RelicRow)
{
	Server_DebugRemoveRelic(RelicRow);
}

void ATerminusPlayerController::Server_DebugRemoveRelic_Implementation(FName RelicRow)
{
	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->RemoveRelic(RelicRow);
	}
}

void ATerminusPlayerController::DebugListRelics()
{
	const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>();
	if (!PS) return;

	UE_LOG(LogTemp, Log, TEXT("[Debug] 보유 유물 %d개, 던전 재화 %d"), PS->GetRelics().Num(), PS->GetRunState().Currency);
	for (const FName& Row : PS->GetRelics())
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		UE_LOG(LogTemp, Log, TEXT("  %s  %s : %s"), *Row.ToString(),
			Relic ? *Relic->RelicName.ToString() : TEXT("?"), Relic ? *Relic->RelicDesc.ToString() : TEXT(""));
	}
}

void ATerminusPlayerController::Server_DebugResetStartSkill_Implementation()
{
	if (ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		PS->ResetStartSkill();
	}
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
	
	// 준비 완료 상태거나 이어하기로 직업이 고정됐으면 캐릭터 변경 무시. 클라 UI 도 막지만 여기가 진짜 관문
	if (PS->IsReady() || PS->IsClassLocked())
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
