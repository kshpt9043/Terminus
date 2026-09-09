// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TavernGameMode.generated.h"

/**
 * 
 */
UCLASS()
class TERMINUS_API ATavernGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	// 게임 시작 요청 호스트가 하는거
	void TryStartGame();
	
protected:
	bool AreAllPlayersReady() const;
	
	// 던전 맵 경로
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Flow")
	FString DungeonMapPath = TEXT("/Game/Maps/Lv_Dungeon");
};
