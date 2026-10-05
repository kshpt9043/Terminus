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
	// 이어하기면 세이브 당시 플레이어가 모두 와야 출발. 각자 세이브의 RunState 로 출발
	void TryStartGame();

	// 세이브를 불러와 연 주점인가
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
	// 이어하기
	// -------------------------------------------------------------

	// 불러온 세이브. 없으면 새 판
	UPROPERTY()
	TObjectPtr<UTerminusRunSave> LoadedSave;

	// 세이브 자리(세이브의 Players 순번) -> 그 자리에 앉은 사람
	TMap<int32, TWeakObjectPtr<ATerminusPlayerState>> SaveSeats;

	// 온라인 아이디 대신 빈 자리에 순서대로 앉힘. PIE 와 싱글은 늘 이렇게 (아이디가 실행마다 바뀜)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Save")
	bool bLooseSaveMatching = false;

	bool UseLooseSaveMatching() const;

	// 이 사람이 앉을 세이브 자리. 아이디가 같은 빈 자리 (느슨하면 이름 -> 아무 빈 자리). 없으면 INDEX_NONE
	int32 FindSaveSeat(const FString& PlayerId, const FString& PlayerName) const;

	void TakeSaveSeat(APlayerController* NewPlayer);

	// 아직 안 온 세이브 플레이어 이름
	TArray<FString> GetMissingSavePlayers() const;

	// 누가 아직 안 왔는지 채팅으로
	void AnnounceSaveSeats() const;

	FTimerHandle AutoStartTimer;
};
