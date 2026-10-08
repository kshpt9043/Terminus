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
#include "Game/LoadingScreenSubsystem.h"

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

	if (USessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USessionSubsystem>())
	{
		Sessions->StartRun();
	}

	UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr;

	if (LoadedSave)
	{
		// 세이브의 지도 / 층 / 슬롯 그대로. 세이브 당시 RunState 로 (시작 고르기는 이미 끝난 상태)
		if (Run)
		{
			Run->BeginLoadedRun(*LoadedSave);
		}

		ATerminusPlayerState* TPS = GameState->PlayerArray.Num() > 0 ? Cast<ATerminusPlayerState>(GameState->PlayerArray[0]) : nullptr;
		if (TPS && LoadedSave->Players.Num() > 0)
		{
			FRunState Saved = LoadedSave->Players[0].RunState;
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
	
	// 방장 화면 (손님은 PC 의 PreClientTravel 에서)
	if (ULoadingScreenSubsystem* Loading = ULoadingScreenSubsystem::Get(this))
	{
		Loading->Show(FText::FromString(TEXT("던전으로 이동하는 중...")));
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

	// 싱글 이어하기: 세이브 당시 직업 / 하드 모드로 고정하고 바로 출발 (로그인 도중에 레벨을 옮기지 않게 살짝 뒤에)
	if (LoadedSave && LoadedSave->Players.Num() > 0)
	{
		if (ATerminusPlayerState* TPS = NewPlayer ? NewPlayer->GetPlayerState<ATerminusPlayerState>() : nullptr)
		{
			TPS->LockClassFromSave(LoadedSave->Players[0].RunState.CharacterClass);
			TPS->SetHardMode(LoadedSave->Summary.bHardMode);
		}
		GetWorldTimerManager().SetTimer(AutoStartTimer, this, &ATavernGameMode::TryStartGame, 0.2f, false);
	}
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

	Super::Logout(Exiting);
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
	Notice.Text = bInHardMode ? TEXT("하드 모드가 켜졌습니다. 죽으면 던전에 들고 간 창고 유물이 창고에서 사라집니다.")
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
