// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/RunTypes.h"
#include "DungeonGameMode.generated.h"

class ATerminusPlayerState;

// 진행 중에 나간 플레이어. 돌아오면 이 상태로 되살림
struct FDepartedPlayer
{
	FString PlayerId;     // 온라인 아이디 (스팀 ID)
	FString PlayerName;
	FRunState RunState;   // 나갈 때 상태. SavedHealth = 그때 체력
};

/**
 * 
 */
UCLASS()
class TERMINUS_API ADungeonGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	/* 기본 겜모 함수 오버라이드 - PlayerStart 지점 고르는 함수인데, 배틀러 위치
	 * 를 로비 UI에 있던 순서대로 하기 위해서 함 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
	// 멀티 세이브 이어하기면 세이브의 지도 / 층을 쓰고, 세이브 플레이어 전원을 '아직 안 온 사람'으로 둠 (= 이공간)
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 던전엔 주점에서 같이 넘어온 사람만. 진행 중에 나갔던 사람 / 세이브 플레이어는 들어올 수 있음
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	// 나갔던 사람이 돌아오면 나갈 때 상태로 되살림 (배틀러가 생기기 전에)
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 멀티에서 진행 중에 누가 나가면 남은 사람은 전원 이공간으로 (싸우던 방은 무효 = 들어가기 전으로 롤백)
	// 세션을 주점 목록에 다시 띄우고, 나간 사람이 그걸 찾아 돌아오면 전원 지도로
	virtual void Logout(AController* Exiting) override;

	// 나갔다가 아직 안 돌아온 사람이 있는가 (= 이공간). 있으면 방을 고를 수 없음
	bool HasDepartedPlayers() const { return DepartedPlayers.Num() > 0; }
	TArray<FString> GetDepartedNames() const;
	const TArray<FDepartedPlayer>& GetDepartedPlayers() const { return DepartedPlayers; }

protected:
	// 이미 배정한 자리. 같은 자리를 두 번 주지 않기 위함
	UPROPERTY()
	TArray<TObjectPtr<AActor>> AssignedStarts;

	// 누가 어느 자리를 받았나. 나가면 자리를 돌려받아 돌아온 사람에게 줌
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> StartOwners;

	TArray<FDepartedPlayer> DepartedPlayers;

	// 멀티 세이브를 불러와 모이는 중 (전원 모이면 false). 이공간 / 채팅 문구만 다름
	bool bGatheringFromSave = false;

	// 돌아온 사람의 기록. 아이디가 같은 것 (PIE 는 아이디가 매번 바뀌어서 이름 -> 아무거나). 없으면 INDEX_NONE
	// bLoose = 아이디가 안 맞아도 이름 -> 아무 자리 (방장 자신 등)
	int32 FindDeparted(const FString& PlayerId, const FString& PlayerName, bool bLoose = false) const;
	bool UseLooseRejoinMatching() const;

	// 이공간 상태를 맞춤: 기다리는 사람이 있으면 세션을 목록에 띄우고 전원 이공간 화면, 없으면 닫고 전원 지도로
	// Skip = 지금 나가는 중인 컨트롤러 (그쪽엔 안 보냄)
	void RefreshRift(const AController* Skip = nullptr);
};
