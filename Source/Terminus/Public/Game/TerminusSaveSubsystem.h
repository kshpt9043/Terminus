#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CharacterTypes.h"
#include "Data/RunTypes.h"
#include "Map/MapManager.h"
#include "TerminusSaveSubsystem.generated.h"

// 세이브 속 플레이어 한 명
USTRUCT(BlueprintType)
struct FRunSavePlayer
{
	GENERATED_BODY()

	// 온라인 아이디 (스팀 ID). 멀티 이어하기 때 같은 사람인지 확인
	UPROPERTY(BlueprintReadOnly)
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;

	// 저장 당시 RunState (직업 / 스텟 / 유물 / 강화 스킬 / 재화 / 지도 위치). SavedHealth = 그때 체력
	UPROPERTY(BlueprintReadOnly)
	FRunState RunState;
};

// 세이브 목록에 보여줄 요약. 목록을 띄울 때 세이브 파일을 다 열지 않게 인덱스 파일에 따로 모아 둠
USTRUCT(BlueprintType)
struct FRunSaveSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SlotName;

	// 방(던전) 이름. 처음 주점을 열 때(싱글은 시작할 때) 정한 것
	UPROPERTY(BlueprintReadOnly)
	FString RoomName;

	UPROPERTY(BlueprintReadOnly)
	FDateTime SavedAt;

	UPROPERTY(BlueprintReadOnly)
	bool bMultiplayer = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHardMode = false;

	UPROPERTY(BlueprintReadOnly)
	int32 Floor = 1;

	// 지나온 방 수 (가장 앞선 사람 기준)
	UPROPERTY(BlueprintReadOnly)
	int32 RoomsCleared = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> PlayerNames;

	UPROPERTY(BlueprintReadOnly)
	TArray<ECharacterClass> PlayerClasses;
};

// 런 하나의 세이브 파일. 런마다 슬롯 하나 ("Run_20261005_143210"), 방을 끝낼 때마다 덮어씀
UCLASS()
class TERMINUS_API UTerminusRunSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FRunSaveSummary Summary;

	// 이번 층 지도
	UPROPERTY()
	TArray<FRoomNode> Rooms;

	UPROPERTY()
	int32 Floor = 1;

	UPROPERTY()
	TArray<FRunSavePlayer> Players;
};

// 세이브 목록 (요약만). 슬롯 "RunSaveIndex"
UCLASS()
class TERMINUS_API UTerminusRunSaveIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FRunSaveSummary> Entries;
};

/**
 * 런 세이브 (이어하기)
 *  - 자동 저장: 방(전투)이 끝나고 전원이 지도로 돌아왔을 때 서버가 저장 (UDungeonAreaSubsystem::FinishAllRooms)
 *    싱글 / 멀티 모두 서버(방장) 컴퓨터에만 저장됨
 *  - 싱글: 세이브 하나 (SingleRunSlot). 진행 중인 게 있으면 메인 메뉴 '던전 입장' 이 그걸 이어서 함 (새 게임 불가)
 *  - 멀티: 런마다 슬롯 하나. 메인 메뉴 '이어하기' 목록에서 고름
 *  - 이어하기: 세이브를 읽어 두고 엶. 게임모드가 GetPendingLoad 로 꺼내 씀
 *    싱글 = 세션 없이 주점 -> 바로 출발. 멀티 = 세이브 인원으로 주점을 열고, 세이브 당시 플레이어가 다 모여야 출발
 *
 * 영구 데이터(골드 / 창고 / 보유 스킬)는 여기가 아니라 UTerminusProfileSubsystem
 */
UCLASS()
class TERMINUS_API UTerminusSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UTerminusSaveSubsystem* Get(const UObject* WorldContext);

	// 세이브 목록 (최근 저장 먼저). 파일이 없어진 항목은 뺌. bMultiplayerOnly = 멀티 세이브만 (이어하기 목록)
	TArray<FRunSaveSummary> GetRunSaves(bool bMultiplayerOnly = false);

	// 이어하기 목록에 띄울 멀티 세이브가 있는가
	bool HasRunSaves() { return GetRunSaves(true).Num() > 0; }

	// 싱글은 세이브가 하나뿐 (슬롯 SingleRunSlot). 진행 중인 싱글 런이 있으면 true + 요약
	// 있으면 던전 입장은 이걸 이어서만 할 수 있음 (새 싱글 게임 불가)
	bool FindSingleRunSave(FRunSaveSummary& OutSummary);

	static const FString SingleRunSlot;

	// 세이브가 생기거나 지워졌을 때 (메인 메뉴 이어하기 버튼 표시용)
	FSimpleMulticastDelegate OnRunSavesChanged;

	// 지금 런 저장 (서버만). 실패하면 false
	bool SaveCurrentRun(UWorld* World);

	UTerminusRunSave* LoadRunSave(const FString& SlotName) const;

	bool DeleteRunSave(const FString& SlotName);

	// 이어하기. 세이브를 읽어 두고 엶. 못 열면 false + 사유
	//  - 싱글: 세션 없이 주점 -> 바로 출발
	//  - 멀티: 세이브 인원으로 세션을 열고 던전으로 바로. 세이브 플레이어가 다 들어올 때까지 이공간 (던전 게임모드)
	bool ContinueRun(const UObject* WorldContext, const FString& SlotName, const FString& TavernMapPath, const FString& DungeonMapPath, FText& OutError);

	// 이어하기로 연 주점이면 그 세이브. 주점 게임모드가 꺼내 씀
	UTerminusRunSave* GetPendingLoad() const { return PendingLoad; }

	// 새 판을 시작하거나 메뉴로 돌아오면 비움 (안 비우면 다음 주점이 이어하기로 열림)
	void ClearPendingLoad() { PendingLoad = nullptr; }

	// 새 런 슬롯 이름 ("Run_20261005_143210")
	static FString MakeNewSlotName();

	// 목록에 띄울 글 ("멀티 3인 · 1층 표층 · 방 4개 지남" / "무도가 Joe, 성기사 Kim")
	static FText DescribeTitle(const FRunSaveSummary& Summary);
	static FText DescribePlayers(const FRunSaveSummary& Summary);

private:
	UPROPERTY()
	TObjectPtr<UTerminusRunSave> PendingLoad;

	UTerminusRunSaveIndex* LoadIndex() const;
	void SaveIndex(UTerminusRunSaveIndex* Index) const;
};
