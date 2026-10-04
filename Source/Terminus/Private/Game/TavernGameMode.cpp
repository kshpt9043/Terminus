// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/TavernGameMode.h"

#include "Player/TerminusPlayerState.h"
#include "Game/TerminusRunSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Online/SessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Player/TerminusPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusFlow, Log, All);

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
	
	// 새 런 시작. 인원은 여기서 확정 -> 지도 생성이 이 값을 씀
	if (UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
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
