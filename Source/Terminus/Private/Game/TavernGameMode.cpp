// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/TavernGameMode.h"

#include "Player/TerminusPlayerState.h"
#include "Game/TerminusRunSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Online/SessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Player/TerminusPlayerController.h"
#include "Game/TerminusSaveSubsystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusFlow, Log, All);

void ATavernGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (const UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		LoadedSave = Save->GetPendingLoad();
	}

	if (LoadedSave)
	{
		bHardMode = LoadedSave->Summary.bHardMode;
		UE_LOG(LogTerminusFlow, Log, TEXT("Tavern: 이어하기 %s (플레이어 %d명)"), *LoadedSave->Summary.SlotName, LoadedSave->Players.Num());
	}
}

void ATavernGameMode::TryStartGame()
{
	if (!AreAllPlayersReady())
	{
		UE_LOG(LogTerminusFlow, Warning, TEXT("Tavern: 시작 거절. 전원 준비 아님"));
		return;
	}

	// 이어하기: 세이브 당시 플레이어가 다 와야 출발
	if (LoadedSave)
	{
		const TArray<FString> Missing = GetMissingSavePlayers();
		if (Missing.Num() > 0)
		{
			FChatMessage Notice;
			Notice.Kind = EChatMessageKind::System;
			Notice.Text = FString::Printf(TEXT("세이브 당시 플레이어가 모두 모여야 출발할 수 있습니다. 기다리는 중: %s"), *FString::Join(Missing, TEXT(", ")));
			ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
			UE_LOG(LogTerminusFlow, Warning, TEXT("Tavern: 시작 거절. 세이브 플레이어 %d명 안 옴"), Missing.Num());
			return;
		}
	}

	if (USessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USessionSubsystem>())
	{
		Sessions->StartRun();
	}

	UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr;

	if (LoadedSave)
	{
		// 세이브의 지도 / 층 / 슬롯 그대로. 각자 세이브 당시 RunState 로 (시작 고르기는 이미 끝난 상태)
		if (Run)
		{
			Run->BeginLoadedRun(*LoadedSave);
		}

		for (const TPair<int32, TWeakObjectPtr<ATerminusPlayerState>>& Seat : SaveSeats)
		{
			ATerminusPlayerState* TPS = Seat.Value.Get();
			if (!TPS || !LoadedSave->Players.IsValidIndex(Seat.Key)) continue;

			FRunState Saved = LoadedSave->Players[Seat.Key].RunState;
			Saved.SelectedRoomId = -1;
			TPS->SetRunState(Saved);
		}

		if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
		{
			Save->ClearPendingLoad();
		}
	}
	else
	{
		// 새 런 시작. 인원은 여기서 확정 -> 지도 생성이 이 값을 씀
		if (Run)
		{
			Run->BeginNewRun(GameState->PlayerArray.Num());
		}

		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
			{
				TPS->BeginRun();
			}
		}
	}
	
	UE_LOG(LogTerminusFlow, Log, TEXT("Tavern: 던전으로 이동 %s"), *DungeonMapPath);
	GetWorld()->ServerTravel(DungeonMapPath);
}

void ATavernGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	
	if (!ErrorMessage.IsEmpty()) { return; }   // 엔진이 이미 거절했으면 그대로

	if (const USessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USessionSubsystem>())
	{
		// GetNumPlayers 는 호스트 포함 지금 주점에 있는 인원
		Sessions->CheckJoinRequest(Options, GetNumPlayers(), ErrorMessage);
	}

	// 이어하기: 세이브에 있는 사람만
	if (ErrorMessage.IsEmpty() && LoadedSave)
	{
		const FString PlayerId = UniqueId.IsValid() ? UniqueId->ToString() : FString();
		if (FindSaveSeat(PlayerId, FString()) == INDEX_NONE)
		{
			ErrorMessage = UseLooseSaveMatching()
				? TEXT("세이브 인원이 모두 찼습니다.")
				: TEXT("이 주점은 세이브를 이어하는 중입니다. 세이브에 없는 플레이어는 들어갈 수 없습니다.");
		}
	}
}

void ATavernGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ATerminusPlayerState* TPS = NewPlayer ? NewPlayer->GetPlayerState<ATerminusPlayerState>() : nullptr)
	{
		TPS->SetHardMode(bHardMode);
	}

	if (NewPlayer && NewPlayer->PlayerState)
	{
		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("%s 님이 주점에 들어왔습니다."), *NewPlayer->PlayerState->GetPlayerName());
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
	}

	if (LoadedSave)
	{
		TakeSaveSeat(NewPlayer);

		// 싱글 이어하기는 고를 게 없으니 바로 출발 (로그인 도중에 레벨을 옮기지 않게 살짝 뒤에)
		if (GetNetMode() == NM_Standalone)
		{
			GetWorldTimerManager().SetTimer(AutoStartTimer, this, &ATavernGameMode::TryStartGame, 0.2f, false);
		}
	}
}

// =====================================================================
// 이어하기 자리
// =====================================================================

bool ATavernGameMode::UseLooseSaveMatching() const
{
	const UWorld* World = GetWorld();
	return bLooseSaveMatching || GetNetMode() == NM_Standalone || (World && World->WorldType == EWorldType::PIE);
}

int32 ATavernGameMode::FindSaveSeat(const FString& PlayerId, const FString& PlayerName) const
{
	if (!LoadedSave) return INDEX_NONE;

	auto IsFree = [this](int32 Seat)
	{
		const TWeakObjectPtr<ATerminusPlayerState>* Taken = SaveSeats.Find(Seat);
		return !Taken || !Taken->IsValid();
	};

	const TArray<FRunSavePlayer>& Players = LoadedSave->Players;

	// 1. 같은 온라인 아이디
	if (!PlayerId.IsEmpty())
	{
		for (int32 i = 0; i < Players.Num(); ++i)
		{
			if (IsFree(i) && Players[i].PlayerId == PlayerId) return i;
		}
	}

	if (!UseLooseSaveMatching()) return INDEX_NONE;

	// 2. 같은 이름 -> 3. 아무 빈 자리
	if (!PlayerName.IsEmpty())
	{
		for (int32 i = 0; i < Players.Num(); ++i)
		{
			if (IsFree(i) && Players[i].PlayerName == PlayerName) return i;
		}
	}
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		if (IsFree(i)) return i;
	}
	return INDEX_NONE;
}

void ATavernGameMode::TakeSaveSeat(APlayerController* NewPlayer)
{
	ATerminusPlayerState* TPS = NewPlayer ? NewPlayer->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!TPS || !LoadedSave) return;

	const FUniqueNetIdRepl& NetId = TPS->GetUniqueId();
	const int32 Seat = FindSaveSeat(NetId.IsValid() ? NetId->ToString() : FString(), TPS->GetPlayerName());
	if (Seat == INDEX_NONE)
	{
		UE_LOG(LogTerminusFlow, Warning, TEXT("Tavern: %s 님이 앉을 세이브 자리가 없음"), *TPS->GetPlayerName());
		return;
	}

	SaveSeats.Add(Seat, TPS);

	const FRunSavePlayer& Saved = LoadedSave->Players[Seat];
	TPS->LockClassFromSave(Saved.RunState.CharacterClass);
	TPS->SetHardMode(LoadedSave->Summary.bHardMode);

	UE_LOG(LogTerminusFlow, Log, TEXT("Tavern: %s -> 세이브 자리 %d (%s, 저장 당시 이름 %s)"),
		*TPS->GetPlayerName(), Seat, *UEnum::GetValueAsString(Saved.RunState.CharacterClass), *Saved.PlayerName);

	AnnounceSaveSeats();
}

TArray<FString> ATavernGameMode::GetMissingSavePlayers() const
{
	TArray<FString> Missing;
	if (!LoadedSave) return Missing;

	for (int32 i = 0; i < LoadedSave->Players.Num(); ++i)
	{
		const TWeakObjectPtr<ATerminusPlayerState>* Taken = SaveSeats.Find(i);
		if (!Taken || !Taken->IsValid())
		{
			Missing.Add(LoadedSave->Players[i].PlayerName);
		}
	}
	return Missing;
}

void ATavernGameMode::AnnounceSaveSeats() const
{
	if (!LoadedSave || GetNetMode() == NM_Standalone) return;

	const TArray<FString> Missing = GetMissingSavePlayers();

	FChatMessage Notice;
	Notice.Kind = EChatMessageKind::System;
	Notice.Text = Missing.Num() == 0
		? FString(TEXT("세이브 당시 플레이어가 모두 모였습니다. 직업은 세이브 당시로 고정됩니다."))
		: FString::Printf(TEXT("이어하기: %d / %d명. 기다리는 중: %s"),
			LoadedSave->Players.Num() - Missing.Num(), LoadedSave->Players.Num(), *FString::Join(Missing, TEXT(", ")));
	ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
}

void ATavernGameMode::Logout(AController* Exiting)
{
	if (Exiting && Exiting->PlayerState)
	{
		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("%s 님이 나갔습니다."), *Exiting->PlayerState->GetPlayerName());
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
	}

	// 이어하기 자리 비우기
	if (LoadedSave && Exiting)
	{
		const APlayerState* Leaving = Exiting->PlayerState;
		for (auto It = SaveSeats.CreateIterator(); It; ++It)
		{
			if (!It.Value().IsValid() || It.Value().Get() == Leaving)
			{
				It.RemoveCurrent();
			}
		}
	}

	Super::Logout(Exiting);

	if (LoadedSave)
	{
		AnnounceSaveSeats();
	}
}

void ATavernGameMode::SetHardMode(bool bInHardMode)
{
	// 이어하기는 세이브 당시 설정 그대로
	if (LoadedSave) return;

	bHardMode = bInHardMode;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
		{
			TPS->SetHardMode(bInHardMode);
		}
	}

	FChatMessage Notice;
	Notice.Kind = EChatMessageKind::System;
	Notice.Text = bInHardMode ? TEXT("하드 모드가 켜졌습니다. 던전에 들고 간 창고 유물은 창고에서 사라집니다.")
	                          : TEXT("하드 모드가 꺼졌습니다.");
	ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
}

bool ATavernGameMode::AreAllPlayersReady() const
{
	if (!GameState || GameState->PlayerArray.Num() == 0)
	{
		return false;
	}
	
	// 싱글은 검사 안함
	if (GetNetMode() == NM_Standalone)
	{
		return true;
	}
	
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS);
		if (!TPS || !TPS->IsReady())
		{
			return false;
		}
	}
	
	return true;
}
