// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/DungeonGameMode.h"

#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "Dungeon/DungeonAreaSubsystem.h"
#include "Online/SessionSubsystem.h"
#include "Player/TerminusPlayerState.h"
#include "Game/TerminusRunSubsystem.h"
#include "Game/TerminusSaveSubsystem.h"

AActor* ADungeonGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> Slots;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Slots);

	// X 좌표 순 -> 왼쪽부터 0번 자리. 순서가 뒤집혀 나오면 부등호만 바꾸면 됨
	Slots.Sort([](const AActor& A, const AActor& B)
	{
		return A.GetActorLocation().X < B.GetActorLocation().X;
	});

	// PlayerArray 순서는 리스타트 중에 계속 바뀌어서 못 쓴다(전원이 맨 뒤로 온다)
	// 그래서 순번을 묻지 않고, 아직 안 준 자리를 왼쪽부터 건넨다
	for (AActor* Slot : Slots)
	{
		if (!AssignedStarts.Contains(Slot))
		{
			AssignedStarts.Add(Slot);
			StartOwners.Add(Player, Slot);
			return Slot;
		}
	}

	// 자리보다 인원이 많으면 엔진 기본에 맡긴다
	
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADungeonGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	
	if (!ErrorMessage.IsEmpty()) { return; }   // 엔진이 이미 거절했으면 그대로

	// PIE 는 심리스 트래블이 막혀서 클라가 재접속으로 넘어옴 -> 막으면 테스트 불가
	if (GetWorld()->WorldType == EWorldType::PIE) { return; }

	// 진행 중에 나갔던 사람은 다시 들어올 수 있음
	const FString PlayerId = UniqueId.IsValid() ? UniqueId->ToString() : FString();
	if (FindDeparted(PlayerId, FString()) != INDEX_NONE) { return; }

	// 던전엔 주점에서 같이 넘어온 사람만. 심리스로 온 사람은 여기를 안 탐
	ErrorMessage = TEXT("이미 던전이 진행 중입니다.");
}

void ADungeonGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// 멀티 세이브 이어하기 (메인 메뉴 -> 세션 -> 여기로 바로). 싱글은 주점을 거쳐 평소처럼 옴
	UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this);
	UTerminusRunSave* Loaded = Save ? Save->GetPendingLoad() : nullptr;
	if (!Loaded || !Loaded->Summary.bMultiplayer) return;

	// 지도 / 층 / 슬롯 / 이름 -> MapManager 가 BeginPlay 에서 이 지도를 씀
	if (UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
	{
		Run->BeginLoadedRun(*Loaded);
	}

	// 세이브 플레이어 전원이 '아직 안 온 사람'. 들어오는 대로 세이브 당시 상태로 (직업도 그대로)
	for (const FRunSavePlayer& Player : Loaded->Players)
	{
		FDepartedPlayer& Waiting = DepartedPlayers.AddDefaulted_GetRef();
		Waiting.PlayerId = Player.PlayerId;
		Waiting.PlayerName = Player.PlayerName;
		Waiting.RunState = Player.RunState;
		Waiting.RunState.SelectedRoomId = -1;
	}
	bGatheringFromSave = DepartedPlayers.Num() > 0;

	Save->ClearPendingLoad();
	UE_LOG(LogTemp, Log, TEXT("[Dungeon] 멀티 세이브 %s 이어하기. 플레이어 %d명 기다림"), *Loaded->Summary.SlotName, DepartedPlayers.Num());
}

void ADungeonGameMode::PostLogin(APlayerController* NewPlayer)
{
	// 배틀러는 Super::PostLogin 안에서 생김 -> 그 전에 RunState 를 되살려야 스텟 / 체력이 맞음
	ATerminusPlayerState* TPS = NewPlayer ? NewPlayer->GetPlayerState<ATerminusPlayerState>() : nullptr;
	bool bReturned = false;
	if (TPS && DepartedPlayers.Num() > 0)
	{
		// 방장 자신은 세이브를 만든 사람 -> 아이디가 달라도 자리를 줌 (계정을 바꿨거나 PIE)
		const FUniqueNetIdRepl& NetId = TPS->GetUniqueId();
		const int32 Index = FindDeparted(NetId.IsValid() ? NetId->ToString() : FString(), TPS->GetPlayerName(), NewPlayer->IsLocalController());
		if (Index != INDEX_NONE)
		{
			FRunState Restored = DepartedPlayers[Index].RunState;
			Restored.SelectedRoomId = -1;
			TPS->SetRunState(Restored);
			DepartedPlayers.RemoveAt(Index);
			bReturned = true;
		}
	}

	Super::PostLogin(NewPlayer);

	if (!bReturned) return;

	FChatMessage Notice;
	Notice.Kind = EChatMessageKind::System;
	const TCHAR* Verb = bGatheringFromSave ? TEXT("합류했습니다") : TEXT("돌아왔습니다");
	Notice.Text = DepartedPlayers.Num() == 0
		? FString::Printf(TEXT("%s 님이 %s. 이공간을 빠져나와 지도로 갑니다."), *TPS->GetPlayerName(), Verb)
		: FString::Printf(TEXT("%s 님이 %s. 아직 기다리는 중: %s"), *TPS->GetPlayerName(), Verb, *FString::Join(GetDepartedNames(), TEXT(", ")));
	ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);

	if (DepartedPlayers.Num() == 0)
	{
		bGatheringFromSave = false;
	}

	// 다 돌아왔으면 전원 지도로. 아직 남았으면 돌아온 사람도 이공간에
	RefreshRift();
}

TArray<FString> ADungeonGameMode::GetDepartedNames() const
{
	TArray<FString> Names;
	for (const FDepartedPlayer& Departed : DepartedPlayers)
	{
		Names.Add(Departed.PlayerName);
	}
	return Names;
}

bool ADungeonGameMode::UseLooseRejoinMatching() const
{
	const UWorld* World = GetWorld();
	return World && World->WorldType == EWorldType::PIE;
}

int32 ADungeonGameMode::FindDeparted(const FString& PlayerId, const FString& PlayerName, bool bLoose) const
{
	if (!PlayerId.IsEmpty())
	{
		for (int32 i = 0; i < DepartedPlayers.Num(); ++i)
		{
			if (DepartedPlayers[i].PlayerId == PlayerId) return i;
		}
	}

	if (!bLoose && !UseLooseRejoinMatching()) return INDEX_NONE;

	if (!PlayerName.IsEmpty())
	{
		for (int32 i = 0; i < DepartedPlayers.Num(); ++i)
		{
			if (DepartedPlayers[i].PlayerName == PlayerName) return i;
		}
	}
	return DepartedPlayers.Num() > 0 ? 0 : INDEX_NONE;
}

void ADungeonGameMode::RefreshRift(const AController* Skip)
{
	const bool bOpen = DepartedPlayers.Num() > 0;

	if (USessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<USessionSubsystem>() : nullptr)
	{
		Sessions->SetRejoinListing(bOpen);
	}

	const TArray<FString> Waiting = GetDepartedNames();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(It->Get());
		if (!PC || PC == Skip) continue;

		if (bOpen)
		{
			PC->Client_EnterRift(Waiting, bGatheringFromSave);
		}
		else
		{
			PC->Client_LeaveRift();
		}
	}
}

void ADungeonGameMode::Logout(AController* Exiting)
{
	ATerminusPlayerState* TPS = Exiting ? Exiting->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 방장(리슨 서버 자신)이 나가면 세션이 끝남 -> 기다릴 것 없음. 월드를 닫는 중(PIE 종료 등)이어도 무시
	const APlayerController* ExitingPC = Cast<APlayerController>(Exiting);
	const bool bTearingDown = !GetWorld() || GetWorld()->bIsTearingDown;
	const bool bGuestLeft = TPS && !bTearingDown && GetNetMode() != NM_Standalone && !(ExitingPC && ExitingPC->IsLocalController());

	if (bGuestLeft)
	{
		// 1. 진행 중인 방 정리. 싸우는 중이면 이번 방은 무효 (전원 들어가기 전으로 롤백)
		if (UDungeonAreaSubsystem* Areas = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
		{
			Areas->HandlePlayerLeft(TPS);
		}

		// 2. 돌아오면 되살릴 상태 (롤백 / 진행이 반영된 뒤)
		FDepartedPlayer& Departed = DepartedPlayers.AddDefaulted_GetRef();
		const FUniqueNetIdRepl& NetId = TPS->GetUniqueId();
		Departed.PlayerId = NetId.IsValid() ? NetId->ToString() : FString();
		Departed.PlayerName = TPS->GetPlayerName();
		Departed.RunState = TPS->GetRunState();
		Departed.RunState.SelectedRoomId = -1;

		const ATerminusBattler* Battler = Cast<ATerminusBattler>(TPS->GetPawn());
		const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
		Departed.RunState.SavedHealth = Stats ? FMath::Max(1, Stats->GetCombatState().Health) : Departed.RunState.SavedHealth;

		// 3. 그 사람 자리를 비워 둠 (돌아오면 다시 받게)
		if (const TWeakObjectPtr<AActor>* Start = StartOwners.Find(Exiting))
		{
			AssignedStarts.Remove(Start->Get());
			StartOwners.Remove(Exiting);
		}

		// 4. 남은 사람은 이공간으로, 세션은 주점 목록에 다시 띄움 (나간 사람이 찾아 돌아오게)
		RefreshRift(Exiting);

		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("%s 님이 나갔습니다. 돌아올 때까지 이공간에서 기다립니다."), *TPS->GetPlayerName());
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);

		UE_LOG(LogTemp, Log, TEXT("[Dungeon] %s 나감 (아이디 %s). 돌아올 때까지 대기"), *Departed.PlayerName, *Departed.PlayerId);
	}
	else if (Exiting && Exiting->PlayerState)
	{
		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("%s 님이 나갔습니다."), *Exiting->PlayerState->GetPlayerName());
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
	}

	Super::Logout(Exiting);
}
