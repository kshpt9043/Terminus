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

void UCombatStatsComponent::ApplyDamage(int32 Amount)
{
	if (!HasAuth())
	{
		return;
	}
	
	if (Amount <= 0)
	{
		return;
	}
	
	// 일단 회피 미구현
	int32 Absorbed = FMath::Min(State.Shield, Amount);
	State.Shield -= Absorbed;
	State.Health = FMath::Max(0, State.Health - ( Amount - Absorbed ));
	NotifyStateChanged();
}

void UCombatStatsComponent::AddShield(int32 Amount)
{
	if (!HasAuth())
	{
		return;
	}
	
	if (Amount <= 0)
	{
		return;
	}
	
	State.Shield += Amount;
	NotifyStateChanged();
}

void UCombatStatsComponent::Heal(int32 Amount)
{
	if (!HasAuth())
	{
		return;
	}
	
	if (Amount <= 0)
	{
		return;
	}
	
	State.Health = FMath::Min(State.Health + Amount, Stats.MaxHealth);
	NotifyStateChanged();
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
	return true;
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

void UCombatStatsComponent::AddBonusEvasion(int32 Amount)
{
	if (!HasAuth())
	{
		return;
	}
	
	if (Amount <= 0)
	{
		return;
	}
	
	State.BonusEvasion += Amount;
	NotifyStateChanged();
}

void UCombatStatsComponent::OnRep_State()
{
	NotifyStateChanged();
}

void UCombatStatsComponent::NotifyStateChanged()
{
	OnCombatStateChanged.Broadcast();
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

