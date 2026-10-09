// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/StatTypes.h"
#include "CombatStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatDied);

class UCombatStatsComponent;

// 서버 전용 전투 사건 알림 (유물 발동 등). 누가 / 얼마나
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnStatsDamaged, UCombatStatsComponent* /*Self*/, int32 /*HealthLost*/, UCombatStatsComponent* /*Instigator*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatsAmount, UCombatStatsComponent* /*Self*/, int32 /*Amount*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatsDied, UCombatStatsComponent* /*Self*/);

// 체력이 0 이 되는 순간 물어봄. true 를 돌려주면 죽지 않은 걸로 (부활 유물 등. 돌려준 쪽이 체력을 채워야 함)
DECLARE_DELEGATE_RetVal_OneParam(bool, FPreventDeath, UCombatStatsComponent* /*Self*/);

/**
 * 체력 보호막 에너지를 들고 있는 전투용 컴포넌트.
 *
 * 플레이어랑 몬스터가 범용으로 쓰는 컴포넌트
 *
 * 피해 계산 순서 (ApplyDamage):
 *  공격자 공포 -25% -> 내 급소 +25% -> 불굴 / 강철 1 고정 -> 철벽 수치만큼 감소 -> 보호막 -> 체력
 *  -> 불사(체력 1 아래로 안 내려감) -> 흡수(깎인 만큼 % 회복) -> 반격(공격자에게 수치만큼)
 */

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TERMINUS_API UCombatStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatStatsComponent();

	void InitFrom(const FCharacterStats& InStats);

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Terminus|Combat")
	const FCharacterStats& GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "Terminus|Combat")
	const FCombatState& GetCombatState() const { return State; }

	UPROPERTY(BlueprintAssignable, Category = "Terminus|Combat")
	FOnCombatStateChanged OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Terminus|Combat")
	FOnCombatDied OnCombatDied;

	// --- 서버 전용 사건 알림 (유물 발동용). 클라에선 안 불림
	FOnStatsDamaged OnDamaged;            // 체력이 실제로 깎였을 때 (보호막만 깎이면 안 불림)
	FOnStatsAmount OnShieldGained;        // 보호막이 늘었을 때
	FOnStatsAmount OnHealed;              // 체력이 실제로 회복됐을 때
	FOnStatsAmount OnEnergySpent;         // 에너지를 썼을 때
	FOnStatsAmount OnSkillEnergySpent;    // 스킬 에너지를 썼을 때
	FOnStatsDied OnDiedNative;            // 죽었을 때 (누가 죽었는지 알 수 있게)
	FPreventDeath PreventDeath;           // 죽기 직전 (부활)

	// State 값 물어보는 함수라서 무조건 InitFrom 이후에 불려야함
	UFUNCTION(BlueprintPure, Category = "Terminus|Combat")
	bool IsDead() const { return State.Health <= 0; }

	// 전투 함수
	// Instigator = 때린 쪽 (공포 / 반격 / 타격 판정에 씀). 중독처럼 때린 쪽이 없으면 nullptr
	void ApplyDamage(int32 Amount, UCombatStatsComponent* Instigator = nullptr);
	void AddShield(int32 Amount);
	void Heal(int32 Amount);
	bool SpendEnergy(int32 Cost);

	// 강화 스킬 비용. 모자라면 false 하고 아무것도 안 함
	bool SpendSkillEnergy(int32 Cost);

	// 즉시 에너지 회복 (신의 축복 같은 스킬). bAllowOverMax 면 최대치를 넘길 수 있음 (보조 배터리 유물: 3 -> 4)
	void AddEnergy(int32 Amount, bool bAllowOverMax = false);
	void RefillEnergy();
	// 턴 끝에 남은 에너지 버림 (기획: 에너지는 매 턴 종료 시 0)
	void DrainEnergy();
	// 보호막 버림 (용어 설명: 보호막은 사이클 종료 시 사라진다). 언제 부를지는 전투 진행이 정함
	void ClearShield();

	// Duration -1 = 전투 끝까지 (사이클이 지나도 안 줄어듦)
	void ApplyStatus(EStatusEffect Type, int32 Value, int32 Duration);
	void RemoveStatus(EStatusEffect Type);

	// 전투가 끝날 때: 상태이상과 보호막을 모두 지움
	void ClearCombatEffects();

	// 최대 체력 변경 (유물). 올리면 현재 체력도 같이 오르고, 내리면 현재 체력이 최대치를 넘지 않게
	void ModifyMaxHealth(int32 Delta);

	// 죽은 상태에서 체력을 되돌림 (부활 유물). Ratio = 최대 체력 대비
	void Revive(float HealthRatio);

	UFUNCTION(BlueprintPure, Category = "Terminus|Combat")
	int32 GetStatusValue(EStatusEffect Type) const;

	// 수치가 0 인 상태(급소 / 공포 / 불굴)도 있어서 걸려 있는지는 따로 봄
	UFUNCTION(BlueprintPure, Category = "Terminus|Combat")
	bool HasStatus(EStatusEffect Type) const;

	// 과욕(산성): 에너지를 쓰는 스킬은 소모량 +1
	int32 GetEffectiveEnergyCost(int32 BaseCost) const;

	// 자기 턴 끝
	void OnTurnEnd();
	// 모두 턴 끝 = 사이클 하나 끝
	void OnCycleEnd();

protected:
	// 클래스가 준 고정 스텟
	UPROPERTY(Replicated)
	FCharacterStats Stats;

	// 전투 중에만 사는 현재값. OnRep가 불리는 식으로 클라에서 복제되어야 함
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FCombatState State;

	UFUNCTION()
	void OnRep_State(const FCombatState& OldState);

	void NotifyStateChanged();

	void NotifyDied();

private:
	bool HasAuth() const;
};
