// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TavernGameMode.generated.h"

class ATerminusPlayerState;
class UTerminusRunSave;

/**
 * 
 */
UCLASS()
class TERMINUS_API ATavernGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	// 이어하기 세이브가 있으면 여기서 꺼내 둠 (플레이어 로그인보다 먼저 불림)
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 게임 시작 요청 호스트가 하는거
	// 싱글 이어하기면 세이브의 RunState 로 바로 출발
	void TryStartGame();

	// 세이브를 불러와 연 주점인가 (싱글 이어하기)
	bool IsContinuingRun() const { return LoadedSave != nullptr; }
	
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

	// -------------------------------------------------------------
	// 싱글 이어하기 (세이브 슬롯 하나). 멀티 이어하기는 주점을 거치지 않고 바로 던전 이공간으로 감 (ADungeonGameMode)
	// -------------------------------------------------------------

	// 불러온 세이브. 없으면 새 판
	UPROPERTY()
	TObjectPtr<UTerminusRunSave> LoadedSave;

	FTimerHandle AutoStartTimer;
};
