// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/TerminusPlayerState.h"
#include "MapManager.generated.h"

UENUM(BlueprintType)
enum class ERoomType : uint8
{
	MONSTER UMETA(DisplayName = "MONSTER"),
	BREAK UMETA(DisplayName = "BREAK"),
	STORE UMETA(DisplayName = "STORE"),
	GUARDIAN UMETA(DisplayName = "GUARDIAN"),
	EVENT UMETA(DisplayName = "EVENT"),
	BOSS UMETA(DisplayName = "BOSS"),
	QUEST UMETA(DisplayName = "QUEST"),
};

USTRUCT(BlueprintType)
struct FRoomNode
{
	GENERATED_BODY()
    
	UPROPERTY(BlueprintReadWrite)
	int32 RoomId = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Row = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Col = 0;

	UPROPERTY(BlueprintReadWrite)
	ERoomType Type = ERoomType::MONSTER;
	
	UPROPERTY(BlueprintReadWrite)
	int32 MaxPlayers = 1;

	UPROPERTY(BlueprintReadWrite)
	TArray<int32> ConnectedRoomIds;
};

class ATerminusPlayerController;
class UDungeonThemeData;

// 계층 하나(표층 / 중층 / 심층)에 나올 수 있는 테마들. 계층이 바뀔 때 이 중 하나를 고름
USTRUCT(BlueprintType)
struct FTierThemes
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TObjectPtr<UDungeonThemeData>> Themes;
};

// 맵 데이터가 업데이트되었음을 UI 등에 알리기 위한 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMapGenerated, const TArray<FRoomNode>&, MapData);

UCLASS()
class TERMINUS_API AMapManager : public AActor
{
	GENERATED_BODY()
    
public: 
	AMapManager();

	UPROPERTY(ReplicatedUsing = OnRep_Rooms, BlueprintReadOnly, Category = "Map")
	TArray<FRoomNode> Rooms;

	UPROPERTY(BlueprintAssignable, Category = "Map")
	FOnMapGenerated OnMapGenerated;

	// 현재 게임 멀티플레이 참여 인원수 (서버에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	int32 CurrentPlayerCount = 4;
	
	// ★ 전체 레벨(층수) 개수 (기본값: 12)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "2", UIMin = "2"))
	int32 TotalLevels = 12;
	
	// ★ 한 레벨(줄) 당 생성 가능한 최대 방 개수 (기본값: 4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRoomsPerRow = 4;

	// 일반 방 비율 설정 (기본값: 몬스터 55%, 휴식 10%, 상점 10%, 가디언 10%, 이벤트 15%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	TMap<ERoomType, float> RoomTypeWeights;

	// 이 층(지도)의 테마. 방에 들어가면 구역에 이 테마의 무대가 뜬다. 계층이 바뀌면 TierThemes 에서 새로 고름
	// 모두에게 복제 (상단바 테마 이름). TierThemes 가 비어 있으면 이 값을 계속 씀
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Map Settings|Theme")
	TObjectPtr<UDungeonThemeData> FloorTheme;

	// 계층별 테마 후보 (0 = 표층, 1 = 중층, 2 = 심층). 기획: 테마 하나가 2개 층, 중층은 리자드의 늪 / 꽃의 정원
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings|Theme")
	TArray<FTierThemes> TierThemes;

	// 지금 층 (1~). 기획: 1~2층 표층 / 3~4층 중층 / 5~6층 심층. 보스방을 깨면 올라감
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Map Settings", meta = (ClampMin = "1"))
	int32 CurrentFloor = 1;

	// 지금 있는 마지막 층 (콘텐츠가 늘면 올릴 것). 이 층 보스 뒤엔 '다음 층' 이 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings", meta = (ClampMin = "1"))
	int32 LastFloor = 6;

	// 테마가 바뀌는 층인가 (2 / 4 / 6층). 이 층 보스 뒤에서만 다음 층 / 탈출 / 배신을 고름
	static bool IsThemeEndFloor(int32 Floor) { return Floor % 2 == 0; }

	// 퀘스트 방이 있는 층인가 = 테마가 시작되는 층 (1 / 3 / 5층). 아니면 퀘스트 줄 없이 일반 방부터 (줄 수 TotalLevels - 1)
	static bool HasQuestRoom(int32 Floor) { return !IsThemeEndFloor(Floor); }

	// 지금 지도의 줄 수 (퀘스트 줄이 없으면 TotalLevels - 1). 지도가 아직 없으면 TotalLevels
	int32 GetLevelCount() const;

	// [서버] 보스방이 끝났을 때 (구역 서브시스템이 부름). 테마가 안 바뀌는 층이면 바로 다음 층
	void HandleBossCleared();

	// [서버] 다음 층으로: 층 +1, (계층이 바뀌면) 새 테마, 새 지도, 전원 지도 처음 / 체력 회복
	void AdvanceFloor();

	// 층 이름 "표층" / "중층" / "심층"
	static FText GetTierName(int32 Floor);

	// 방 선택 요청 처리. 서버에서만 불림
	// 클라 -> 자기 PC 의 Server_RequestSelectRoom -> 여기. (이 액터는 서버 소유라 RPC 를 직접 못 받음)
	void HandleSelectRoomRequest(ATerminusPlayerController* Requester, int32 RoomId);
	
	


protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Rooms();

public: 
	UFUNCTION(BlueprintCallable, Category = "Map")
	TArray<FRoomNode> GenerateMap();

private:
	FRoomNode CreateRoom(int32 RoomId, int32 Row, int32 Col, ERoomType Type = ERoomType::MONSTER, int32 MaxPlayers = 1);

	// 아래 줄(Lower) -> 위 줄(Upper) 연결. 선끼리 교차하지 않고 모든 방이 위아래로 최소 하나씩 이어짐
	// 화면에서도 방이 열(Col) 위치에 그려지므로 교차가 없으면 지도에서도 선이 엇갈리지 않음
	void ConnectRowsWithoutCrossing(TArray<FRoomNode>& Map, TArray<int32> Lower, TArray<int32> Upper);

	// FirstRow ~ LastRow 방들의 타입을 RoomTypeWeights 비율대로 고르게 나눔.
	// 개수를 먼저 확정하고(비율 × 방 수), 타입마다 층 전체에 등간격으로 퍼뜨림
	// 배치 규칙은 Slay the Spire 와 같음 (초반 층 휴식/가디언 금지만 뺌). 연결이 먼저 만들어져 있어야 함
	//  - 휴식/상점/가디언은 바로 앞뒤 방(경로상)과 같은 종류 금지
	//  - 같은 방에서 갈라지는 방끼리 휴식/상점/가디언/이벤트 중복 금지, 몬스터 중복은 가능하면 피함
	//  - NoRestRow 줄에는 휴식터 금지 (보스 직전 휴식터 줄의 바로 앞 줄. 없으면 INDEX_NONE)
	void DistributeRoomTypes(TArray<FRoomNode>& Map, int32 FirstRow, int32 LastRow, int32 NoRestRow);

	// 방 타입별 최대 입장 인원.
	// 기획: 인원 제한은 몬스터/가디언 방에만 붙는다("몬스터 방 [인원 제한 가능]").
	// 휴식터/상점/이벤트/퀘스트/보스는 파티가 쪼개질 이유가 없으니 전원 수용
	int32 GetRoomCapacity(ERoomType Type) const;

	// 1레벨 -> 보스방 경로 존재 검증 (BFS)
	bool ValidatePathToBoss(const TArray<FRoomNode>& InMap, int32 LastRowIndex);

	// 정원 때문에 파티가 갇히는 구간이 없는지 검증
	bool ValidatePlayerCapacity(const TArray<FRoomNode>& InMap);

	bool IsValidNextRoom(const FRunState& PlayerRunState, const FRoomNode& TargetRoom);

	// 이 층의 테마. 같은 계층이면 지금 테마 그대로, 계층이 바뀌면 그 계층 후보 중 랜덤
	UDungeonThemeData* PickThemeForFloor(int32 Floor) const;

	// 이번 런이 1인인가. 기획의 방 선택 규칙이 싱글/멀티로 갈린다
	bool IsSinglePlayerRun() const;

	// 각자 고른 방(SelectedRoomId)으로 입장. 방마다 던전 구역(ADungeonArea)을 배정해서 거기서 진행
	// 레벨에 구역이 없으면(배치 전) 콘텐츠 없이 즉시 클리어 처리
	void EnterSelectedRooms(const TArray<ATerminusPlayerState*>& Players);

	// 멀티: 전원이 선택을 마쳤으면 각자 고른 방으로 입장시킨다
	void CheckAllPlayersReadyAndStart();
};