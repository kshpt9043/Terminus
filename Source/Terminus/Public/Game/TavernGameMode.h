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
	
	// 주점 입장
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	// 채팅창에 입장 / 퇴장 안내. 들어온 사람에게 지금 하드 모드 설정도 넣어 줌
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 하드 모드 (방장만). 파티 전원의 RunState 에 같은 값
	void SetHardMode(bool bInHardMode);
	bool IsHardMode() const { return bHardMode; }
	virtual void Logout(AController* Exiting) override;
	
protected:
	bool AreAllPlayersReady() const;
	
	// 지금 판의 하드 모드 설정
	bool bHardMode = false;

	// 던전 맵 경로
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Flow")
	FString DungeonMapPath = TEXT("/Game/Maps/Lv_Dungeon");
};
