// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/TerminusBattler.h"
#include "TerminusMonster.generated.h"

class UCombatStatsComponent;
class UPaperFlipbook;
class UPaperZDAnimSequence;
struct FMonsterVisualRow;
enum class EMonsterIntentKind : uint8;

/**
 * 몬스터 폰. 배틀러 몸통(3층 구조, 좌우 반전, 전투 컴포넌트)을 그대로 물려받고
 * 초기화만 직업 DT 대신 몬스터 DT 에서 함
 *
 * 연출은 BP 의 Animation 컴포넌트에 붙인 ABP 가 맡음. 평소엔 ABP 상태머신(대기),
 * 행동/피격/사망은 아래 시퀀스를 ABP 의 "DefaultSlot" 으로 덮어 재생
 */

UCLASS()
class TERMINUS_API ATerminusMonster : public ATerminusBattler
{
	GENERATED_BODY()

public:
	ATerminusMonster();

	// 몬스터 턴에 행동할 때 서버가 부름. 공격 -> 공격, 방어 -> 방어, 버프/디버프 -> 스페셜
	void PlayIntentAnim(EMonsterIntentKind Kind);

	// 죽은 몬스터를 숨길 때 서버가 부름. 사망 시퀀스가 있으면 끝까지 보여주고 숨김
	void HideAfterDeath();

	// 어떤 몬스터인지 (DT_Monster 행). 스폰할 때 FinishSpawning 전에 서버가 정함. 복제돼서 각 화면이 외형을 고름
	void SetMonsterRow(FName RowName);
	FName GetMonsterRow() const { return MonsterRow; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	// 어떤 몬스터인지. DT_Monster 의 행 이름. 레벨에 세울 때 디테일에서 바꿈 / 전투가 스폰할 때 정함
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_MonsterRow, Category = "Terminus|Monster")
	FName MonsterRow = TEXT("mon_slime");

	UFUNCTION()
	void OnRep_MonsterRow();

	// DT_MonsterVisual 에 이 몬스터 행이 있으면 ABP 를 끄고 그 플립북으로. 없으면 ABP 그대로
	void ApplyVisual();

	void InitAsMonster(FName RowName);

	// 아래 시퀀스는 전부 BP 의 ABP 와 같은 애니메이션 소스 것. 오른쪽을 보는 그림 기준 (몬스터는 좌우 반전돼서 왼쪽을 봄)
	// 비어 있으면 그 연출만 없음
	// 임시: 몬스터 종류별 외형이 DT 로 빠지기 전까지 BP 에서 직접 지정

	// 공격 후보. 여러 개면 무작위
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Visual")
	TArray<TObjectPtr<UPaperZDAnimSequence>> AttackSequences;

	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperZDAnimSequence> DefendSequence;

	// 버프 / 디버프
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperZDAnimSequence> SpecialSequence;

	// 체력이 깎였을 때 (보호막으로만 막으면 안 나옴)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperZDAnimSequence> HitSequence;

	// 끝나면 대기로 돌아가지 않고 사라짐
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperZDAnimSequence> DeathSequence;

private:
	// 피격 / 사망은 전투 쪽이 아니라 몬스터가 직접 듣고 재생. 서버에서만 불림 (ApplyDamage 가 서버 전용)
	void HandleDamaged(UCombatStatsComponent* Self, int32 HealthLost, UCombatStatsComponent* DamageInstigator);
	void HandleDied(UCombatStatsComponent* Self);

	// 모든 화면에서 ABP 슬롯으로 한 번 재생
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySequence(UPaperZDAnimSequence* Sequence);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDeath();

	bool PlayOverride(UPaperZDAnimSequence* Sequence, bool bDeath);
	void OnDeathOverrideEnd(bool bCompleted);

	void FinishHide();

	FTimerHandle DeathHideTimer;
	bool bHideQueued = false;

	// DT_MonsterVisual 의 플립북 (로드해 둠). Idle 이 있으면 플립북 모드
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> VisualIdle;
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> VisualAttack;
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> VisualHit;
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> VisualDefend;
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> VisualSpecial;
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> VisualDeath;

	bool UsesFlipbooks() const { return VisualIdle != nullptr; }

	UFUNCTION()
	void OnDeathFlipbookFinished();
};
