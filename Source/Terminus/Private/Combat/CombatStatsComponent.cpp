// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/CombatStatsComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UCombatStatsComponent::UCombatStatsComponent()
{

	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UCombatStatsComponent::InitFrom(const FCharacterStats& InStats)
{
	if (!HasAuth())
	{
		return;
	}
	
	Stats = InStats;
	
	State = FCombatState();
	State.Health = Stats.MaxHealth;
	State.Energy = Stats.MaxEnergy;
	
	// 연무장 '스킬 에너지' 강화만큼 갖고 시작
	State.SkillEnergy = FMath::Clamp(Stats.StartSkillEnergy, 0, FMath::Max(0, Stats.MaxSkillEnergy));

	NotifyStateChanged();
}

void UCombatStatsComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UCombatStatsComponent, Stats);
	DOREPLIFETIME(UCombatStatsComponent, State);
}

int32 UCombatStatsComponent::ApplyDamage(int32 Amount, UCombatStatsComponent* Instigator)
{
	if (!HasAuth())
	{
		return 0;
	}
	if (Amount <= 0)
	{
		return 0;
	}
	if (IsDead())
	{
		return 0;
	}

	// ① 공격자가 공포면 최종 데미지 -25%, 내가 급소면 받는 데미지 +25%
	if (Instigator && Instigator->HasStatus(EStatusEffect::Fear))
	{
		Amount = FMath::RoundToInt(Amount * 0.75f);
	}
	if (HasStatus(EStatusEffect::Mark))
	{
		Amount = FMath::RoundToInt(Amount * 1.25f);
	}

	// 보호막 있는 대상을 때림: 공격자 유물(파쇄의 권갑 등) 만큼 +%, 그리고 OnHitShield 유물 알림
	const bool bHadShield = State.Shield > 0;
	if (Instigator && Instigator != this && bHadShield && Instigator->ShieldBonusPercent > 0)
	{
		Amount = FMath::RoundToInt(Amount * (100 + Instigator->ShieldBonusPercent) / 100.f);
	}

	// ② 불굴 그룹(불굴 / 강철 / 정령화 / 실명): 한 번에 받는 데미지 1 고정. 철벽: 수치만큼 줄어듦
	if (Amount > 0 && HasStatus(EStatusEffect::Indomitable))
	{
		Amount = 1;
	}
	Amount = FMath::Max(0, Amount - GetStatusValue(EStatusEffect::IronWall));

	// ③ 보호막 먼저, 남은 만큼 체력
	//    체술: 보호막이 받고 남은 피해를 회피만큼 줄임. 회피 이하면 0 (노션 캐릭터 10-23)
	const int32 Absorbed = FMath::Min(State.Shield, Amount);
	State.Shield -= Absorbed;
	int32 Remain = Amount - Absorbed;
	if (bEvasionReduction)
	{
		Remain = FMath::Max(0, Remain - GetEvasion());
	}
	int32 HealthLost = FMath::Min(State.Health, Remain);

	// ④ 불사: 체력이 1 아래로 내려가지 않음
	if (HasStatus(EStatusEffect::Immortality) && State.Health - HealthLost < 1)
	{
		HealthLost = FMath::Max(0, State.Health - 1);
	}

	State.Health -= HealthLost;
	NotifyStateChanged();

	if (HealthLost > 0)
	{
		// ⑤ 흡수: 깎인 체력의 수치% 만큼 회복 (죽는 한 대면 회복 안 함)
		const int32 AbsorbPercent = GetStatusValue(EStatusEffect::Absorption);
		if (AbsorbPercent > 0 && !IsDead())
		{
			Heal(FMath::RoundToInt(HealthLost * AbsorbPercent / 100.f));
		}

		OnDamaged.Broadcast(this, HealthLost, Instigator);
	}

	// ⑥ 반격: 맞으면(보호막으로 막았어도) 때린 쪽에게 수치만큼 1 회. 반격끼리 끝없이 주고받지 않게 공격자는 비움
	const int32 CounterDamage = GetStatusValue(EStatusEffect::Counter);
	if (Instigator && Instigator != this && CounterDamage > 0 && Amount > 0)
	{
		Instigator->ApplyDamage(CounterDamage, nullptr);
	}

	// 위에서 이미 죽은 건 걸렀으니 여기서 죽어 있으면 방금 이 한 대로 죽은 것
	// 부활 유물 등이 막으면(PreventDeath 가 true) 안 죽은 걸로
	if (IsDead())
	{
		const bool bPrevented = PreventDeath.IsBound() && PreventDeath.Execute(this) && !IsDead();
		if (!bPrevented)
		{
			NotifyDied();
		}
	}

	if (Instigator && Instigator != this && bHadShield)
	{
		Instigator->OnHitShieldTarget.Broadcast(Instigator, Amount);
	}
	return HealthLost;
}

void UCombatStatsComponent::AddShield(int32 Amount)
{
	if (!HasAuth())
	{
		return;
	}
	if (IsDead())
	{
		return;
	}
	if (Amount <= 0)
	{
		return;
	}
	
	State.Shield += Amount;
	NotifyStateChanged();
	OnShieldGained.Broadcast(this, Amount);
}

void UCombatStatsComponent::Heal(int32 Amount)
{
	if (!HasAuth())
	{
		return;
	}
	if (IsDead())
	{
		return;
	}
	if (Amount <= 0)
	{
		return;
	}
	
	const int32 Before = State.Health;
	State.Health = FMath::Min(State.Health + Amount, Stats.MaxHealth);
	NotifyStateChanged();

	// 실제로 오른 만큼만 알림 (가득 차 있으면 회복이 아님)
	if (State.Health > Before)
	{
		OnHealed.Broadcast(this, State.Health - Before);
	}
}

bool UCombatStatsComponent::SpendEnergy(int32 Cost)
{
	if (!HasAuth() || Cost < 0)
	{
		return false;
	}
	
	if (State.Energy < Cost)
	{
		return false;
	}
	
	State.Energy -= Cost;
	State.EnergySpentCounter += Cost;
	
	int32 Gained = State.EnergySpentCounter / 2;
	if (Gained > 0)
	{
		State.EnergySpentCounter %= 2;
		State.SkillEnergy = FMath::Min(State.SkillEnergy + Gained, Stats.MaxSkillEnergy);
	}
	NotifyStateChanged();

	if (Cost > 0)
	{
		// 역류: 에너지를 쓸 때마다 수치만큼 피해
		if (const int32 Reflux = GetStatusValue(EStatusEffect::Reflux); Reflux > 0)
		{
			ApplyDamage(Reflux);
		}
		OnEnergySpent.Broadcast(this, Cost);
	}
	return true;
}

bool UCombatStatsComponent::SpendSkillEnergy(int32 Cost)
{
	if (!HasAuth() || Cost < 0 || State.SkillEnergy < Cost)
	{
		return false;
	}

	if (Cost > 0)
	{
		State.SkillEnergy -= Cost;
		NotifyStateChanged();

		// 내상: 스킬 에너지를 쓸 때마다 수치만큼 피해
		if (const int32 Injury = GetStatusValue(EStatusEffect::InternalInjury); Injury > 0)
		{
			ApplyDamage(Injury);
		}
		OnSkillEnergySpent.Broadcast(this, Cost);
	}
	return true;
}

void UCombatStatsComponent::AddEnergy(int32 Amount, bool bAllowOverMax)
{
	if (!HasAuth() || Amount <= 0)
	{
		return;
	}

	State.Energy = bAllowOverMax ? State.Energy + Amount : FMath::Min(State.Energy + Amount, FMath::Max(State.Energy, Stats.MaxEnergy));
	NotifyStateChanged();
}

int32 UCombatStatsComponent::GetEvasion() const
{
	// 기본 / 연무장 회피 + 민첩(은신 같은 스킬)
	return FMath::Max(0, Stats.Evasion + GetStatusValue(EStatusEffect::Evasion));
}

int32 UCombatStatsComponent::GetEffectiveEnergyCost(int32 BaseCost) const
{
	// 과욕(산성): 에너지만 적용 -> 에너지를 쓰는 스킬만 +1
	return (BaseCost > 0 && HasStatus(EStatusEffect::Acid)) ? BaseCost + 1 : BaseCost;
}

void UCombatStatsComponent::RefillEnergy()
{
	if (!HasAuth())
	{
		return;
	}

	State.Energy = Stats.MaxEnergy;
	NotifyStateChanged();
}

void UCombatStatsComponent::DrainEnergy()
{
	if (!HasAuth() || State.Energy == 0)
	{
		return;
	}

	State.Energy = 0;
	NotifyStateChanged();
}

void UCombatStatsComponent::ClearShield()
{
	if (!HasAuth() || State.Shield == 0)
	{
		return;
	}

	State.Shield = 0;
	NotifyStateChanged();
}

void UCombatStatsComponent::ApplyStatus(EStatusEffect Type, int32 Value, int32 Duration)
{
	if (!HasAuth())
	{
		return;
	}
	if (IsDead())
	{
		return;
	}
	// 반동 피해는 스킬의 SelfDamage 칸이 바로 처리함. 상태로 남길 게 없음
	if (Type == EStatusEffect::None || Type == EStatusEffect::SelfDamage)
	{
		return;
	}
	// 퓨전은 걸리는 순간 중독 용암 얼음 중 하나로 바뀌는 거라 저장할 상태가 아님
	// -> 스킬 실행 쪽이 셋 중 하나를 골라서 넘겨야 함. 여기 오면 호출부 버그
	if (Type == EStatusEffect::Fusion)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] 퓨전이 그대로 들어옴. 스킬 쪽에서 중독/용암/얼음 중 하나로 바꿔서 넘길 것"));
		return;
	}
	// 0 은 걸 게 없음. -1 은 전투 끝까지
	if (Duration == 0)
	{
		return;
	}
	
	
	// 같은 그룹이 이미 걸려 있나 찾기. 찾기만 하고 만들진 않음
	// 배열 안 진짜 항목의 포인터라 여기다 바로 쓰면 원본이 바뀜. 없으면 nullptr 라서 else 에서 직접 Add
	// 그룹이 같으면 이름(Type)도 새 걸로 바꿈 -> 화면에 최근에 걸린 테마 이름이 보임
	FStatusInstance* Existing = FindStatus(Type);
	if (Existing)
	{
		Existing->Type = Type;
		Existing->Value = Value;
		Existing->Duration = Duration;
	}
	else
	{
		FStatusInstance NewStatus;
		NewStatus.Type = Type;
		NewStatus.Value = Value;
		NewStatus.Duration = Duration;
		State.Statuses.Add(NewStatus);
	}
	
	NotifyStateChanged();
}

void UCombatStatsComponent::RemoveStatus(EStatusEffect Type)
{
	if (!HasAuth())
	{
		return;
	}

	const EStatusEffect Group = GetStatusGroup(Type);
	if (State.Statuses.RemoveAll([Group](const FStatusInstance& S) { return GetStatusGroup(S.Type) == Group; }) > 0)
	{
		NotifyStateChanged();
	}
}

void UCombatStatsComponent::ClearCombatEffects()
{
	if (!HasAuth() || (State.Statuses.Num() == 0 && State.Shield == 0))
	{
		return;
	}

	State.Statuses.Reset();
	State.Shield = 0;
	NotifyStateChanged();
}

void UCombatStatsComponent::ModifyMaxHealth(int32 Delta)
{
	if (!HasAuth() || Delta == 0)
	{
		return;
	}

	Stats.MaxHealth = FMath::Max(1, Stats.MaxHealth + Delta);
	if (!IsDead())
	{
		State.Health = FMath::Clamp(State.Health + FMath::Max(0, Delta), 1, Stats.MaxHealth);
	}
	NotifyStateChanged();
}

void UCombatStatsComponent::Revive(float HealthRatio)
{
	if (!HasAuth())
	{
		return;
	}

	State.Health = FMath::Clamp(FMath::RoundToInt(Stats.MaxHealth * HealthRatio), 1, Stats.MaxHealth);
	NotifyStateChanged();
}

bool UCombatStatsComponent::HasStatus(EStatusEffect Type) const
{
	return FindStatus(Type) != nullptr;
}

int32 UCombatStatsComponent::GetStatusValue(EStatusEffect Type) const
{
	const FStatusInstance* Existing = FindStatus(Type);
	return Existing ? Existing->Value : 0;
}

FStatusInstance* UCombatStatsComponent::FindStatus(EStatusEffect Type)
{
	const EStatusEffect Group = GetStatusGroup(Type);
	return State.Statuses.FindByPredicate([Group](const FStatusInstance& S) { return GetStatusGroup(S.Type) == Group; });
}

const FStatusInstance* UCombatStatsComponent::FindStatus(EStatusEffect Type) const
{
	// const 함수 안이라 멤버가 전부 읽기 전용 -> 포인터도 const 로 받아야 함
	const EStatusEffect Group = GetStatusGroup(Type);
	return State.Statuses.FindByPredicate([Group](const FStatusInstance& S) { return GetStatusGroup(S.Type) == Group; });
}

void UCombatStatsComponent::OnTurnEnd()
{
	if (!HasAuth())
	{
		return;
	}

	// 독 걸려있으면 턴 끝날때마다 대미지.
	const int32 Poison = GetStatusValue(EStatusEffect::Poison);
	if (Poison > 0)
	{
		ApplyDamage(Poison);
	}
	// ApplyDamage에 끝났음을 알리는 델리게이트 있어서 여기선 안부름
}

void UCombatStatsComponent::OnCycleEnd()
{
	if (!HasAuth())
	{
		return;
	}
	if (State.Statuses.Num() == 0)
	{
		return;
	}

	// 지속 효과 전부 1씩 깎기, &로 해야 원본을 건듬. 음수(-1)는 전투 끝까지라 안 깎음
	for (FStatusInstance& S : State.Statuses)
	{
		if (S.Duration > 0)
		{
			S.Duration -= 1;
		}
	}

	// 뒤에서부터 검사해야 당겨지는 문제가 없음
	for (int32 i = State.Statuses.Num() - 1; i >= 0; --i)
	{
		if (State.Statuses[i].Duration == 0)
		{
			State.Statuses.RemoveAt(i);
		}
	}

	NotifyStateChanged();

}


void UCombatStatsComponent::OnRep_State(const FCombatState& OldState)
{
	NotifyStateChanged();
	
	// 클라는 ApplyDamage 가 안 돌아서 복제로 알아채야 함
	// 이전엔 살아 있었는데 지금 죽었으면 -> 죽은 순간
	if (OldState.Health > 0 && IsDead())
	{
		NotifyDied();
	}
}

void UCombatStatsComponent::NotifyStateChanged()
{
	OnCombatStateChanged.Broadcast();
}

void UCombatStatsComponent::NotifyDied()
{
	UE_LOG(LogTemp, Warning, TEXT("[Died] %s (%s)"),
		*GetNameSafe(GetOwner()),
		HasAuth() ? TEXT("서버") : TEXT("클라"));

	OnCombatDied.Broadcast();
	OnDiedNative.Broadcast(this);
}

bool UCombatStatsComponent::HasAuth() const
{
	const AActor* MyOwner = GetOwner();
	if (!MyOwner || !MyOwner->HasAuthority())
	{
		return false;
	}
	
	return true;
}

