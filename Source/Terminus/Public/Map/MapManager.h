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

// 테마 끝 층 보스 뒤 선택지
UENUM(BlueprintType)
enum class EFloorChoice : uint8
{
	None,
	NextFloor,   // 다음 층 (마지막 층엔 없음)
	Escape,      // 탈출 -> 정산
	Betray       // 배신 (멀티만, 선착순 1명)
};

UENUM(BlueprintType)
enum class EFloorVotePhase : uint8
{
	None,        // 투표 없음
	Voting,      // 투표 중 (VoteSeconds)
	Revealing,   // 다 골랐음 -> 공개 타이머 (RevealSeconds)
	Done         // 공개 + 결과 (잠깐 보여주고 적용)
};

USTRUCT(BlueprintType)
struct FFloorVoteEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerState> Player;

	UPROPERTY(BlueprintReadOnly)
	EFloorChoice Choice = EFloorChoice::None;
};

// 모두에게 복제되는 투표 상태. 누가 골랐는지(RevealedVotes)는 공개 단계가 끝나야 채워짐
USTRUCT(BlueprintType)
struct FFloorVoteState
{
	GENERATED_BODY()

	// 투표마다 다른 번호 (화면이 새 투표인지 알게)
	UPROPERTY(BlueprintReadOnly) int32 VoteId = 0;
	UPROPERTY(BlueprintReadOnly) EFloorVotePhase Phase = EFloorVotePhase::None;
	UPROPERTY(BlueprintReadOnly) int32 Floor = 0;
	UPROPERTY(BlueprintReadOnly) TArray<EFloorChoice> Options;
	// 선택지별 표 수 (Options 와 같은 순서)
	UPROPERTY(BlueprintReadOnly) TArray<int32> Counts;
	UPROPERTY(BlueprintReadOnly) int32 VotedCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalVoters = 0;
	// 이 단계가 끝나는 서버 시각 (GameState::GetServerWorldTimeSeconds 기준)
	UPROPERTY(BlueprintReadOnly) float PhaseEndTime = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FFloorVoteEntry> RevealedVotes;
	UPROPERTY(BlueprintReadOnly) EFloorChoice Result = EFloorChoice::None;
};

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

	// [서버] 보스방이 끝났을 때 (구역 서브시스템이 부름)
	// 테마가 안 바뀌는 층이면 바로 다음 층(true, 저장까지 함). 테마 끝 층이면 행선지 투표 시작(false, 결과가 나올 때까지 저장 안 함)
	bool HandleBossCleared();

	// -------------------------------------------------------------
	// 테마 끝 층 선택 (다음 층 / 탈출 / 배신). 멀티는 투표
	// -------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_VoteState, BlueprintReadOnly, Category = "Map|Vote")
	FFloorVoteState VoteState;

	bool IsFloorVoteActive() const { return VoteState.Phase != EFloorVotePhase::None; }

	// [서버] 표 넣기 (PC 의 Server_CastFloorVote). 한 사람 한 번, 배신은 선착순 1명. 거절하면 false
	bool CastFloorVote(APlayerState* Voter, EFloorChoice Choice);

	// 투표 시간 / 공개 타이머 / 결과를 보여주는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings|Vote")
	float VoteSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings|Vote")
	float RevealSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings|Vote")
	float ResultSeconds = 3.f;

	// 배신 전투 보정 (사용자 결정 10-08: 배신당한 쪽 현재 체력 일정 % 감소 + 배신자 공격 / 방어 상승. 수치는 임시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings|Betrayal", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float BetrayalVictimHealthCut = 0.3f;

	// 배신자 공격 / 방어 +(상대 수 x 이 값)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Settings|Betrayal", meta = (ClampMin = "0"))
	int32 BetrayerStatPerOpponent = 1;

	// [서버] 배신 전투가 끝남 (전투가 부름). 결과대로 각자 정산 조정을 붙여 런을 끝냄
	//  - 이긴 쪽이 보스 유물을 가짐: 배신자 승리면 동료들의 보스 유물이 배신자 정산 후보로
	//  - 배신자 패배: 배신자의 보스 유물을 팔아 그 골드를 나머지가 나눠 가짐
	//  - 진 쪽은 이번 보스전에서 얻은 유물 / 스킬을 정산에서 못 가져감. 나머지는 평소 탈출 정산
	void HandleBetrayalResult(ATerminusPlayerState* Betrayer, bool bBetrayerWon);

	// 런이 끝났는가 (정산으로 넘어감). 끝난 뒤 나가는 사람은 이공간으로 안 감
	bool IsRunEnded() const { return bRunEnded; }

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

	UFUNCTION()
	void OnRep_VoteState();

public: 
	UFUNCTION(BlueprintCallable, Category = "Map")
	TArray<FRoomNode> GenerateMap();

	// 한 파티 최대 인원. 퀘스트 / 보스 / 방이 하나뿐인 줄은 정원을 이 값으로 (인원을 잘못 알아도 막히지 않게)
	static constexpr int32 MaxPartySize = 4;

	// 지도에 쓸 인원: 주점에서 정한 인원과 지금 접속한 인원 중 큰 값 (주점을 안 거친 PIE / 이어하기 대비)
	void RefreshPlayerCount();

	// 방이 하나뿐인 줄(퀘스트 / 보스 포함)과 퀘스트 방이 없는 층의 첫 줄은 정원을 최대 인원으로. 예전에 저장된 지도도 고쳐짐
	static void OpenSingleRoomRows(TArray<FRoomNode>& InOutRooms);

	// [서버] 전멸 (싱글 사망 / 멀티 전원 사망)로 런 끝. 세이브 삭제 + 사망 정산 (유물만 골드로, 강화 스킬 / 던전 재화 소멸)
	void EndRunByDeath(const FString& Reason);

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

	// 투표 진행 (서버)
	TMap<TWeakObjectPtr<APlayerState>, EFloorChoice> FloorVotes;
	FTimerHandle VoteTimer;

	void StartFloorVote();
	void EndFloorVoting();
	void ResolveFloorVote();
	void ApplyFloorChoice();
	void NotifyVoteChanged();

	// 탈출로 런 끝: 세이브 삭제 + 각자 탈출 정산
	void EndRunByEscape(const FString& Reason);

	// 런 끝 공통: 세이브 삭제 + 각자 정산 대기로. Results 가 있으면 사람마다 안내 / 정산 조정 (배신 결과)
	struct FRunEndResult
	{
		FString Message;
		FSettlementAdjust Adjust;
	};
	void EndRun(const FString& Reason, bool bDeath, const TMap<APlayerState*, FRunEndResult>* Results = nullptr);

	// 투표에서 배신이 나옴 -> 배신 전투 시작
	void StartBetrayalBattle();

	bool bRunEnded = false;

	// 이번 런이 1인인가. 기획의 방 선택 규칙이 싱글/멀티로 갈린다
	bool IsSinglePlayerRun() const;

	// 각자 고른 방(SelectedRoomId)으로 입장. 방마다 던전 구역(ADungeonArea)을 배정해서 거기서 진행
	// 레벨에 구역이 없으면(배치 전) 콘텐츠 없이 즉시 클리어 처리
	void EnterSelectedRooms(const TArray<ATerminusPlayerState*>& Players);

	// 멀티: 전원이 선택을 마쳤으면 각자 고른 방으로 입장시킨다
	void CheckAllPlayersReadyAndStart();
};