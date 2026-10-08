// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/CharacterTypes.h"
#include "Data/RunTypes.h"
#include "Data/UpgradeTypes.h"
#include "TerminusPlayerState.generated.h"

class ADungeonArea;
struct FSkillRow;
struct FRelicRow;

/**
 * 
 */
UCLASS()
class TERMINUS_API ATerminusPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	ECharacterClass GetCharacterClass() const { return RunState.CharacterClass; }
	
	void SetCharacterClass(ECharacterClass InClass);

	// 이어하기 주점에서 세이브 당시 직업으로 고정 (서버만). 고정되면 직업 변경 요청을 무시
	void LockClassFromSave(ECharacterClass InClass);
	bool IsClassLocked() const { return bClassLocked; }

	// 세이브에서 불러온 체력을 꺼내고 비움 (서버만). 없으면 -1
	int32 ConsumeSavedHealth();
	
	bool IsReady() const { return bReady; }
	
	void SetReady(bool bInReady);
	
	// 주점에서 던전으로 출발할 때 서버가 부름. 클래스 기본 스텟을 런 스텟으로 복사하고 진행도 초기화
	void BeginRun();
	
	// 런 시작 강화 스킬 장착. NAME_None 이면 고를 게 없어 건너뜀. 한 런에 한 번만 (서버만)
	void ChooseStartSkill(FName SkillRow);

	bool HasChosenStartSkill() const { return RunState.bStartSkillChosen; }

	// [테스트] 시작 강화 스킬 고르기를 안 한 상태로 되돌림 (서버만)
	void ResetStartSkill();

	// 런 시작 때 창고에서 고른 유물 장착 (최대 MaxStartRelics 개, 비어도 됨). 한 런에 한 번 (서버만)
	void ChooseStartRelics(const TArray<FName>& RelicRows);

	bool HasChosenStartRelics() const { return RunState.bStartRelicsChosen; }

	// 하드 모드 (서버만 바꿈. 주점 게임모드가 파티 전원에게 같은 값을 넣음)
	void SetHardMode(bool bInHardMode);
	bool IsHardMode() const { return RunState.bHardMode; }

	// [테스트] 유물 고르기를 안 한 상태로 (이미 장착한 유물은 그대로)
	void ResetStartRelics();

	// 던전에 들고 갈 수 있는 창고 유물 수 (기획: 창고 유물 중 1개, 안 골라도 됨)
	static constexpr int32 MaxStartRelics = 1;

	// 인게임 유물 칸 (패시브 + 시작 유물 포함). 기본 6, 연무장 '유물 최대치' 강화로 최대 15 (중간 수치 미정)
	static constexpr int32 BaseRelicCapacity = 6;
	static constexpr int32 MaxRelicCapacity = 15;

	// 지금 가질 수 있는 유물 수 (런 스텟 MaxRelics, 없으면 기본 6)
	int32 GetRelicCapacity() const;

	// 이 직업이 가질 수 있는 유물인가 (공용이거나 그 직업 전용)
	static bool CanClassHoldRelic(ECharacterClass InClass, const FRelicRow& Relic);

	// 전투에서 쓰는 스킬 칸: 0~2 = 기본 스킬(공격 방어 특수), 3~5 = 장착한 강화 스킬
	static constexpr int32 NumBasicSkills = 3;
	static constexpr int32 NumEnhanceSkills = 5;   // 기본 3 + 연무장 '픽업 스킬 칸' 강화 2

	// 그 칸의 스킬. 비어 있으면 nullptr
	const FSkillRow* GetCombatSkill(int32 SlotIndex) const;

	// 그 칸의 스킬 행 이름 (DT_Skill). 유물 OnSkillUsed 판정용
	FName GetCombatSkillRow(int32 SlotIndex) const;

	// 강화 스킬 칸 수 (캐릭터 EnhanceSlots, 최대 NumEnhanceSkills)
	int32 GetEnhanceSlotCount() const;

	// 거점 강화 (서버만 들고 있음). 손님은 자기 PC 의 Server_ReportUpgrades 로 알려 줌
	// BeginRun 때 고른 직업의 강화를 런 스텟 / 기본 스킬 단계에 넣음
	void SetReportedUpgrades(const TArray<FClassUpgrades>& InUpgrades) { ReportedUpgrades = InUpgrades; }

	// 기본 스킬(0~2 칸)에 훈련소 강화를 더함. BaseValue 가 있으면 BaseValue, 없으면 StatusValue
	void ApplyBasicSkillUpgrade(int32 SlotIndex, FSkillRow& InOutSkill) const;

	// 강화 스킬 장착 (서버만). 빈 칸이 있으면 거기에, 꽉 찼으면 ReplaceIndex 칸을 바꿈. 못 넣으면 false
	bool EquipEnhanceSkill(FName SkillRow, int32 ReplaceIndex = INDEX_NONE);

	// -------------------------------------------------------------
	// [유물] 서버만 바꿈. 목록은 RunState.Relics (모두에게 복제)
	// -------------------------------------------------------------

	// 유물 얻기. 못 가지는 유물(다른 직업 / 몬스터 / 이미 있음 / 칸 가득)이면 false
	// Upgrade 등급이면 같은 직업의 Basic 유물을 뺌. OnGainRelic 효과(재화 / 최대 체력 등)는 여기서 바로 적용
	bool GainRelic(FName RelicRow);

	bool RemoveRelic(FName RelicRow);

	// [이벤트] 유물 하나를 다른 유물로 (같은 자리). 새 유물을 못 얻으면 원래대로 두고 false
	bool SwapRelic(FName OldRelic, FName NewRelic);

	// [이벤트] 이번 런 동안 스텟 올리기 (서버만). StatKind 0 = 최대 체력, 1 = 공격, 2 = 방어
	void ApplyPermanentStat(int32 StatKind, int32 Amount);

	// [보스방] 보상으로 받은 유물 / 스킬 기록 (배신 결과용). None 이면 그 칸은 그대로
	void RecordBossReward(FName Skill, FName Relic);

	// [이벤트] 일시 버프 (다음 전투 몇 번). 전투가 시작될 때 상태로 걸리고, 끝날 때마다 ConsumeTempBuffBattle 로 1 씩 줄어듦
	void AddTempBuff(const FTempStatBuff& Buff);
	void ConsumeTempBuffBattle();

	const TArray<FName>& GetRelics() const { return RunState.Relics; }

	// 이번 런에서 나온 적 있는 유물인가 (있으면 후보로 다시 안 나옴). 얻은 유물은 GainRelic 이 알아서 넣음
	bool HasSeenRelic(FName RelicRow) const { return RunState.SeenRelics.Contains(RelicRow); }

	// 후보로 보여준 유물 / 판 유물을 '나온 유물' 로 (서버만)
	void MarkRelicsSeen(const TArray<FName>& RelicRows);

	// 직업 기본 유물 행 이름 (RLC_Fighter_001 등). 데이터에 없으면 NAME_None
	static FName GetBasicRelicRow(ECharacterClass InClass);

	// 전투 밖에서도 의미 있는 유물 효과 (던전 재화 / 최대 체력 / 픽업 스킬 강화). 전투 중 발동도 여기로
	void ApplyRelicMetaEffect(const FRelicRow& Relic);

	// 던전 재화. 음수면 깎음 (0 아래로는 안 내려감)
	void AddCurrency(int32 Amount);

	// 방을 클리어하고 그 방으로 진행. 지도상 현재 위치와 진행 레벨을 갱신하고 선택을 비운다
	void AdvanceToRoom(int32 TargetRoomId, int32 TargetRow);

	// 다음 층으로 (서버만): 지도 위치 처음으로, 지나온 길 비움, 체력 전부 회복
	void BeginFloor();
	
	// Seamless Travel할 때 들고 가는게 아니라 복사시켜서 새로 만들어야 함
	virtual void CopyProperties(APlayerState* NewPS) override;
	
	// -------------------------------------------------------------
	// [Setter 함수들] - 오직 서버(Authority)에서만 동작
	// -------------------------------------------------------------
	void SetSelectedRoomId(int32 InRoomId);
	void SetCurrentRoomInfo(int32 InRoomId, int32 InMapLevel);
	void SetRunState(const FRunState& InRunState);
	
	// -------------------------------------------------------------
	// [Getter 함수들]
	// -------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE FRunState GetRunState() const { return RunState; }

	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE int32 GetSelectedRoomId() const { return RunState.SelectedRoomId; }

	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE int32 GetCurrentRoomId() const { return RunState.CurrentRoomId; }

	UFUNCTION(BlueprintCallable, Category = "Run")
	FORCEINLINE int32 GetCurrentMapLevel() const { return RunState.CurrentMapLevel; }
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunStateChanged, const FRunState&, NewRunState);
	UPROPERTY(BlueprintAssignable, Category = "Run")
	FOnRunStateChanged OnRunStateChanged;

	// -------------------------------------------------------------
	// [던전 구역]
	// -------------------------------------------------------------

	// 서버만. 구역 배정 / 해제 때 ADungeonArea 가 부름
	void SetCurrentArea(ADungeonArea* InArea);

	// 지금 들어가 있는 구역. 지도 화면이면 nullptr
	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	ADungeonArea* GetCurrentArea() const { return CurrentArea; }

protected:
	// 거점 강화 (모든 직업). 서버만, 복제 안 함
	TArray<FClassUpgrades> ReportedUpgrades;

	// 준비 완료 여부. 서버만 바꾸고 클라는 복제로 받기만 함
	// 준비는 처음에 들어갈 때만 중요한 요소이므로 RunState에는 안들어감
	UPROPERTY(Replicated)
	bool bReady = false;

	// 이어하기 주점에서 세이브 자리에 앉아 직업이 고정됐는가. 주점 화면이 직업 버튼을 잠글 때 씀
	UPROPERTY(Replicated)
	bool bClassLocked = false;
	
	// 이동 간 유지되어야 할 데이터
	// ReplicatedUsing 이어야 클라에서 OnRep_RunState 가 불림 -> 지도 UI 갱신이 여기에 걸려 있음
	UPROPERTY(ReplicatedUsing = OnRep_RunState)
	FRunState RunState;
	
	UFUNCTION()
	void OnRep_RunState();

	// 지금 들어가 있는 던전 구역. 방 하나 동안만 의미 있는 값이라 RunState 에는 안 넣음
	// 모든 클라에 복제 -> 누가 어느 구역에 있는지 다 알 수 있음 (관전 / 난입용)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentArea)
	TObjectPtr<ADungeonArea> CurrentArea;

	// 내 것이면 시점을 이 구역(없으면 지도)으로 돌림
	UFUNCTION()
	void OnRep_CurrentArea();
};
