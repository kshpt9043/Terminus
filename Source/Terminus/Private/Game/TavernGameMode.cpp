// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/TavernGameMode.h"

#include "Player/TerminusPlayerState.h"
#include "Game/TerminusRunSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusFlow, Log, All);

void ATavernGameMode::TryStartGame()
{
	if (!AreAllPlayersReady())
	{
		UE_LOG(LogTerminusFlow, Warning, TEXT("Tavern: 시작 거절. 전원 준비 아님"));
		return;
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

bool ATavernGameMode::AreAllPlayersReady() const
{
	if (!GameState || GameState->PlayerArray.Num() == 0)
	{
		return false;
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
