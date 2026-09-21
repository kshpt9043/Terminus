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

void UCombatStatsComponent::ApplyStatus(EStatusEffect Type, int32 Value, int32 Duration)
{
	if (!HasAuth())
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
	if (Duration <= 0)
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

	// 지속 효과 전부 1씩 깎기, &로 해야 원본을 건듬
	for (FStatusInstance& S : State.Statuses)
	{
		S.Duration -= 1;
	}

	// 뒤에서부터 검사해야 당겨지는 문제가 없음
	for (int32 i = State.Statuses.Num() - 1; i >= 0; --i)
	{
		if (State.Statuses[i].Duration <= 0)
		{
			State.Statuses.RemoveAt(i);
		}
	}

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

