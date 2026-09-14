// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/StatTypes.h"
#include "CombatStatsComponent.generated.h"

/**
 * 체력 보호막 에너지를 들고 있는 전투용 컴포넌트.
 *
 * 플레이어랑 몬스터가 범용으로 쓰는 컴포넌트
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
	
protected:
	// 클래스가 준 고정 스텟
	UPROPERTY(Replicated)
	FCharacterStats Stats;

	// 전투 중에만 사는 현재값
	UPROPERTY(Replicated)
	FCombatState State;
};
