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
	const AActor* MyOwner = GetOwner();
	if (!MyOwner || !MyOwner->HasAuthority())
	{
		return;
	}
	
	Stats = InStats;
	
	State = FCombatState();
	State.Health = Stats.MaxHealth;
	State.Energy = Stats.MaxEnergy;
	
	State.SkillEnergy = 0;
}

void UCombatStatsComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UCombatStatsComponent, Stats);
	DOREPLIFETIME(UCombatStatsComponent, State);
}

