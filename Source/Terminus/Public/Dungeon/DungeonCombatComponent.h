#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SkillTypes.h"
#include "Map/MapManager.h"
#include "Data/RelicTypes.h"
#include "DungeonCombatComponent.generated.h"

class ADungeonArea;
class ATerminusMonster;
class ATerminusPlayerState;
class UCombatStatsComponent;
class UDungeonThemeData;
struct FSkillRow;

UENUM(BlueprintType)
enum class ECombatPhase : uint8
{
	None,          // 전투 없음
	PlayerTurn,    // 이 구역 플레이어들이 동시에 행동. 전원 턴 종료하면 몬스터 턴
	MonsterTurn,   // 몬스터들이 예고한 행동을 차례로
	Victory,       // 몬스터 전멸. 잠시 뒤 구역 클리어
	Defeat         // 플레이어 전멸
};

// 머리 위 행동 예고 아이콘 종류 (기획: 공격 / 방어 / 버프 / 디버프)
UENUM(BlueprintType)
enum class EMonsterIntentKind : uint8
{
	Attack,
	Defend,
	Buff,
	Debuff
};

// 몬스터 한 마리의 이번 사이클 행동. 사이클 시작 때 정해서 모두에게 보여줌
USTRUCT(BlueprintType)
struct FMonsterIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ATerminusMonster> Monster;

	// DT_Skill 행 이름
	UPROPERTY(BlueprintReadOnly)
	FName SkillRow;

	UPROPERTY(BlueprintReadOnly)
	EMonsterIntentKind Kind = EMonsterIntentKind::Attack;

	// 공격이면 한 대 피해량 (기획: 공격은 수치도 같이 표기)
	UPROPERTY(BlueprintReadOnly)
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 HitCount = 1;
};

/**
 * 던전 구역 하나의 전투 진행. 구역(ADungeonArea)마다 하나씩 붙어 있음
 * -> 파티가 갈라져 여러 방에서 동시에 싸워도 구역마다 따로 돈다
 *
 * 사이클 = 플레이어 턴 + 몬스터 턴 (기획 전투 로직)
 *  1. 사이클 시작: 몬스터 행동 예고, 플레이어 보호막 제거 + 에너지 회복
 *  2. 플레이어 턴: 이 구역 플레이어 전원이 동시에 스킬 사용. 각자 턴 종료 (중독 피해, 에너지 0)
 *  3. 전원 턴 종료 -> 몬스터 턴: 살아 있는 몬스터가 예고대로 한 마리씩 (몬스터 보호막은 자기 차례 직전에 제거)
 *  4. 사이클 끝: 몬스터 중독 피해, 모두의 상태이상 지속시간 1 감소 -> 다음 사이클
 *  몬스터 전멸 = 승리 -> 구역 클리어 (지도로). 플레이어 전멸 = 패배
 *
 * 판정은 전부 서버. 클라는 자기 PC 의 Server_UseSkill / Server_EndTurn 으로 요청만 보냄
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TERMINUS_API UDungeonCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDungeonCombatComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------------------------------------------------------------
	// [서버 전용]
	// -------------------------------------------------------------

	// 몬스터를 스폰하고 첫 사이클 시작. 전투 방(몬스터 / 가디언 / 보스)일 때 구역이 부름
	void StartCombat(const FRoomNode& Room, const TArray<ATerminusPlayerState*>& InPlayers, const UDungeonThemeData* Theme);

	// 전투 정리 (몬스터 제거, 타이머 정지). 구역을 비울 때
	void EndCombat();

	// 플레이어 요청. SkillIndex = 기본 스킬 0~2 (공격 방어 특수), 강화 스킬 3~5 (장착 칸)
	// TargetIndex = 적 1명 스킬이면 GetMonsters() 의 인덱스, 아군 1명 스킬이면 구역 Occupants 의 인덱스
	void HandleUseSkill(ATerminusPlayerState* PS, int32 SkillIndex, int32 TargetIndex);
	void HandleEndTurn(ATerminusPlayerState* PS);

	// [테스트] 몬스터 전부 처치 -> 승리 (PC 의 DebugWinCombat)
	void DebugKillAllMonsters();

	// 몬스터방 보상을 마침 (플레이어가 '다음으로'). ChosenSkill = 고른 보상 스킬 (None = 안 고름), ReplaceSlot = 칸이 꽉 찼을 때 바꿀 칸
	// 구역의 모든 플레이어가 마치면 구역 클리어 -> 지도
	// 보상 마침 (PC 의 Server_FinishRoomReward). 보여 준 후보 중에서만 인정
	void HandleRewardFinished(ATerminusPlayerState* PS, FName ChosenSkill, int32 ReplaceSlot, FName ChosenRelic, FName ReplaceRelic);

	// -------------------------------------------------------------
	// [조회] 클라 HUD 가 씀
	// -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInCombat() const { return Phase != ECombatPhase::None; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetCycle() const { return Cycle; }

	const TArray<TObjectPtr<ATerminusMonster>>& GetMonsters() const { return Monsters; }
	const TArray<FMonsterIntent>& GetIntents() const { return Intents; }

	// 이 몬스터의 이번 사이클 예고. 없으면 nullptr
	const FMonsterIntent* FindIntent(const ATerminusMonster* Monster) const;

	bool HasEndedTurn(const ATerminusPlayerState* PS) const;

	// -------------------------------------------------------------
	// [설정] BP_DungeonArea 의 Combat 컴포넌트에서 조정
	// -------------------------------------------------------------

	// 스폰할 몬스터 클래스. 비워 두면 /Game/KSH/Characters/Monster/BP_Monster 를 찾아 씀
	UPROPERTY(EditAnywhere, Category = "Combat|Monster")
	TSubclassOf<ATerminusMonster> MonsterClass;

	// 몬스터방 몬스터 수 (최소, 최대). 기획: 1 ~ 3 마리. 구역 몬스터 슬롯 수를 넘지 않음
	UPROPERTY(EditAnywhere, Category = "Combat|Monster")
	FIntPoint NormalMonsterCount = FIntPoint(1, 3);

	// 멀티 체력 보정. 인덱스 = 이 방에 들어온 인원 - 1. 기획: 2인 70% / 3인 140% / 4인 220% 상승
	UPROPERTY(EditAnywhere, Category = "Combat|Monster")
	TArray<float> PartyHealthBonus = { 0.f, 0.7f, 1.4f, 2.2f };

	// 몬스터(보스 제외) 개체별 체력 랜덤 보정. 기획: -5% ~ +10%
	UPROPERTY(EditAnywhere, Category = "Combat|Monster")
	FVector2D MonsterHealthRandom = FVector2D(-0.05f, 0.10f);

	// 테마의 두 번째 층(2 / 4 / 6층) 몬스터 기본 체력 보정 (보스 제외). 기획: +10%
	UPROPERTY(EditAnywhere, Category = "Combat|Balance")
	float SecondFloorHealthBonus = 0.1f;

	// 몬스터(보스 제외) 공격 / 방어 랜덤 가산 최대치. 기획서 "최대 +2" (KSH 코드 주석은 +0~1 -> 기획 확인 필요)
	UPROPERTY(EditAnywhere, Category = "Combat|Monster")
	int32 MonsterStatRandomMax = 2;

	// 몬스터 데이터에 스킬이 비어 있을 때 쓸 임시 스킬 (DT_Skill 행 이름)과 가중치
	// 지금 DT_Monster 의 Skill1~3 이 전부 비어 있어서 넣어 둠. 데이터가 채워지면 자동으로 그걸 씀
	UPROPERTY(EditAnywhere, Category = "Combat|Monster")
	TMap<FName, int32> FallbackMonsterSkills = { { TEXT("slime_strike"), 70 }, { TEXT("condense"), 30 } };

	// 몬스터 행동 사이 간격(초). 한꺼번에 하면 무슨 일이 일어났는지 안 보임
	UPROPERTY(EditAnywhere, Category = "Combat|Timing")
	float MonsterActionInterval = 0.8f;

	// 승리 / 패배 후 구역을 끝내기까지 대기(초)
	UPROPERTY(EditAnywhere, Category = "Combat|Timing")
	float CombatEndDelay = 1.5f;

	// ---- 몬스터방 보상 (기획: 던전 재화 + 랜덤 강화 스킬 2개 중 1개, 안 골라도 됨)

	// 몬스터방 던전 재화 (최소, 최대)
	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	FIntPoint MonsterRewardCurrency = FIntPoint(10, 10);

	// 가디언 / 보스방 던전 재화 (사용자 결정 2026-10-06: 몬스터 10 / 가디언 20 / 보스 40. X~Y 사이 랜덤)
	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	FIntPoint GuardianRewardCurrency = FIntPoint(20, 20);

	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	FIntPoint BossRewardCurrency = FIntPoint(40, 40);

	// 보스방 픽업 스킬 후보 수 (기본 1 = 그 하나를 받거나 말거나)
	UPROPERTY(EditAnywhere, Category = "Combat|Reward", meta = (ClampMin = "1"))
	int32 BossRewardSkillChoices = 1;

	// 보상 스킬 후보 수
	UPROPERTY(EditAnywhere, Category = "Combat|Reward", meta = (ClampMin = "1"))
	int32 RewardSkillChoices = 2;

	// 이미 장착한 스킬도 후보에 넣을지 (넣으면 같은 스킬을 두 칸에 낄 수 있음)
	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	bool bRewardIncludeEquippedSkills = false;

	// 지금 층보다 높은 티어 스킬은 뺄지 (표층 = 모든 층, 중층 = 3층~, 심층 = 5층~)
	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	bool bRewardExcludeHigherTier = true;

	// 이벤트 스킬을 뺄지 (기획: 이벤트 스킬은 이벤트 방에서만)
	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	bool bRewardExcludeEventSkills = true;

	// 승리 후 보상 화면을 띄우기까지 대기(초). 마지막 공격이 보이게
	UPROPERTY(EditAnywhere, Category = "Combat|Reward")
	float RewardDelay = 1.f;

private:
	// ---- 복제되는 진행 상태

	UPROPERTY(Replicated)
	ECombatPhase Phase = ECombatPhase::None;

	UPROPERTY(Replicated)
	int32 Cycle = 0;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ATerminusMonster>> Monsters;

	UPROPERTY(Replicated)
	TArray<FMonsterIntent> Intents;

	// 이번 플레이어 턴에 턴 종료를 누른 사람
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ATerminusPlayerState>> EndedTurn;

	// ---- 서버만

	// 이 구역의 플레이어 (구역 Occupants 와 같은 순서)
	TArray<TWeakObjectPtr<ATerminusPlayerState>> Players;

	// Monsters 와 같은 순서의 DT_Monster 행 이름 (스킬 고를 때)
	TArray<FName> MonsterRows;

	// 몬스터 턴에서 다음에 행동할 몬스터
	int32 NextMonsterIndex = 0;

	// 스킬이 비어 임시 스킬을 쓴다고 이미 알린 몬스터 행 (로그 한 번만)
	mutable TSet<FName> WarnedFallbackRows;

	FTimerHandle StepTimer;

	ADungeonArea* GetArea() const;
	UCombatStatsComponent* GetStats(const ATerminusPlayerState* PS) const;
	TArray<UCombatStatsComponent*> GetAlivePlayerStats() const;
	TArray<UCombatStatsComponent*> GetAliveMonsterStats() const;

	// 방 타입 / 테마에 맞는 몬스터 행 고르기
	TArray<FName> PickMonsterRows(ERoomType RoomType, const UDungeonThemeData* Theme, int32 MaxCount) const;
	void SpawnMonsters(const FRoomNode& Room, const UDungeonThemeData* Theme);

	// 몬스터 행의 스킬 중 가중치 랜덤 (비어 있으면 FallbackMonsterSkills)
	FName PickMonsterSkill(FName MonsterRow) const;

	void StartCycle();
	void ChooseIntents();
	void BeginMonsterTurn();
	void RunNextMonsterAction();
	void FinishCycle();

	// 승패 판정. 끝났으면 true (다음 진행 멈춤)
	bool CheckCombatEnd();

	// ---- 몬스터방 보상 (서버만)
	struct FPendingReward
	{
		TArray<FName> Offers;        // 보여 준 스킬 후보
		TArray<FName> RelicOffers;   // 보여 준 유물 후보
		bool bDone = false;
	};
	TMap<TWeakObjectPtr<ATerminusPlayerState>, FPendingReward> PendingRewards;

	// 지금 방 번호 (로그용)
	int32 CurrentRoomId = INDEX_NONE;

	// 몬스터 / 가디언 / 보스방 클리어 보상
	void StartRoomRewards();
	TArray<FName> PickRewardSkills(const ATerminusPlayerState* PS, int32 Count) const;

	// 유물 후보: 공용이거나 내 직업, 지금 계층 등급(표층 / 중층 / 심층), 아직 안 가진 것 중 Count 개
	TArray<FName> PickRewardRelics(const ATerminusPlayerState* PS, int32 Count) const;
	void FinishRewardsIfAllDone();
	void ClearArea();

	// -------------------------------------------------------------
	// 유물 (서버만). 전투에 참가한 쪽(플레이어 / 몬스터)마다 보유 유물을 들고, 전투 사건이 오면 발동 시점이 맞는 것을 실행
	// -------------------------------------------------------------
	struct FRelicHolder
	{
		TWeakObjectPtr<UCombatStatsComponent> Stats;
		TWeakObjectPtr<ATerminusPlayerState> Player;   // 플레이어면 (던전 재화 / 최대 체력 같은 런 효과용)
		bool bMonster = false;
		TArray<FName> Relics;
		TMap<FName, int32> UsesLeft;                    // 부활 같은 횟수 제한
	};
	TArray<FRelicHolder> Holders;

	// 지금 방 종류 (가디언 / 보스 전투 시작 발동)
	ERoomType CurrentRoomType = ERoomType::MONSTER;

	// 유물 효과가 또 유물을 부르는 깊이 (무한 반복 방지)
	int32 RelicDepth = 0;

	void BuildRelicHolders();
	void ClearRelicHolders();
	int32 FindHolder(const UCombatStatsComponent* Stats) const;

	// 그 쪽의 유물 중 Trigger 가 맞는 것 실행. Amount = 사건 수치 (깎인 체력 등), UsedSkill = OnSkillUsed 판정용
	void FireRelics(int32 HolderIndex, ERelicTrigger Trigger, int32 Amount = 0, FName UsedSkill = NAME_None, const FSkillRow* UsedSkillRow = nullptr);
	void FireRelicsForAll(ERelicTrigger Trigger, bool bPlayers, bool bMonsters);
	void ExecuteRelic(int32 HolderIndex, const FRelicRow& Relic, int32 Amount);

	TArray<UCombatStatsComponent*> GetSideStats(bool bMonsterSide) const;

	// 전투 사건 (UCombatStatsComponent 알림)
	void HandleStatsDamaged(UCombatStatsComponent* Self, int32 HealthLost, UCombatStatsComponent* Instigator);
	void HandleShieldGained(UCombatStatsComponent* Self, int32 Amount);
	void HandleHealed(UCombatStatsComponent* Self, int32 Amount);
	void HandleEnergySpent(UCombatStatsComponent* Self, int32 Amount);
	void HandleSkillEnergySpent(UCombatStatsComponent* Self, int32 Amount);
	void HandleStatsDied(UCombatStatsComponent* Self);
	bool HandlePreventDeath(UCombatStatsComponent* Self);
	void HideDeadMonsters();
	void FinishCombat(bool bVictory);
};
