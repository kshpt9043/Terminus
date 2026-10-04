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
	
	State.SkillEnergy = 0;
	
	NotifyStateChanged();
}

void UCombatStatsComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UCombatStatsComponent, Stats);
	DOREPLIFETIME(UCombatStatsComponent, State);
}

void UCombatStatsComponent::ApplyDamage(int32 Amount, UCombatStatsComponent* Instigator)
{
	if (!HasAuth())
	{
		return;
	}
	if (Amount <= 0)
	{
		return;
	}
	if (IsDead())
	{
		return;
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

	// ② 불굴 / 강철: 한 번에 받는 데미지 1 고정. 철벽: 수치만큼 줄어듦
	if (Amount > 0 && (HasStatus(EStatusEffect::Indomitable) || HasStatus(EStatusEffect::Metal)))
	{
		Amount = 1;
	}
	Amount = FMath::Max(0, Amount - GetStatusValue(EStatusEffect::IronWall));

	// ③ 보호막 먼저, 남은 만큼 체력. 회피(체술)는 암살자 기본 유물이 생기면 그쪽에서
	const int32 Absorbed = FMath::Min(State.Shield, Amount);
	State.Shield -= Absorbed;
	int32 HealthLost = FMath::Min(State.Health, Amount - Absorbed);

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
		if (PreventDeath.IsBound() && PreventDeath.Execute(this) && !IsDead())
		{
			return;
		}
		NotifyDied();
	}
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

		// 내상: 강화 에너지를 쓸 때마다 수치만큼 피해
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

int32 UCombatStatsComponent::GetEffectiveEnergyCost(int32 BaseCost) const
{
	// 과욕(산성): 기본 에너지만 적용 -> 기본 에너지를 쓰는 스킬만 +1
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
	if (Type == EStatusEffect::None)
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
	
	
	// 같은 종류가 이미 걸려 있나 찾기. 찾기만 하고 만들진 않음
	// 배열 안 진짜 항목의 포인터라 여기다 바로 쓰면 원본이 바뀜. 없으면 nullptr 라서 else 에서 직접 Add
	FStatusInstance* Existing = State.Statuses.FindByPredicate(
	[Type](const FStatusInstance& S) { return S.Type == Type; });
	if (Existing)
	{
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

	if (State.Statuses.RemoveAll([Type](const FStatusInstance& S) { return S.Type == Type; }) > 0)
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
	return State.Statuses.ContainsByPredicate([Type](const FStatusInstance& S) { return S.Type == Type; });
}

int32 UCombatStatsComponent::GetStatusValue(EStatusEffect Type) const
{
	// 같은 종류 찾기. const 함수 안이라 멤버가 전부 읽기 전용 -> 포인터도 const 로 받아야 함
	const FStatusInstance* Existing = State.Statuses.FindByPredicate(
	[Type](const FStatusInstance& S) { return S.Type == Type; });
	
	return Existing ? Existing->Value : 0;
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

