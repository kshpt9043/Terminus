#include "Character/TerminusMonster.h"

#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "TimerManager.h"
#include "Combat/CombatStatsComponent.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonCombatComponent.h"

ATerminusMonster::ATerminusMonster()
{
	// 몬스터는 오른쪽에 서서 플레이어 쪽(왼쪽)을 봄
	bFacingRight = false;
}

void ATerminusMonster::BeginPlay()
{
	Super::BeginPlay();

	InitAsMonster(MonsterRow);

	if (HasAuthority())
	{
		CombatStats->OnDamaged.AddUObject(this, &ATerminusMonster::HandleDamaged);
		CombatStats->OnDiedNative.AddUObject(this, &ATerminusMonster::HandleDied);
	}
}

void ATerminusMonster::InitAsMonster(FName RowName)
{
	const FMonsterRow* Row = UTerminusDataSettings::FindMonsterRow(RowName);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("Monster: %s 행이 없음"), *RowName.ToString());
		return;
	}

	// 스텟은 서버만 정함(InitFrom 안에 가드). 클라는 복제로 받음. 플레이어랑 같은 길
	CombatStats->InitFrom(Row->ToStats());
}

void ATerminusMonster::PlayIntentAnim(EMonsterIntentKind Kind)
{
	UPaperZDAnimSequence* Sequence = nullptr;
	switch (Kind)
	{
	case EMonsterIntentKind::Attack:
		if (AttackSequences.Num() > 0) Sequence = AttackSequences[FMath::RandRange(0, AttackSequences.Num() - 1)];
		break;
	case EMonsterIntentKind::Defend:
		Sequence = DefendSequence;
		break;
	case EMonsterIntentKind::Buff:
	case EMonsterIntentKind::Debuff:
		Sequence = SpecialSequence;
		break;
	}

	if (Sequence) MulticastPlaySequence(Sequence);
}

void ATerminusMonster::HandleDamaged(UCombatStatsComponent* Self, int32 HealthLost, UCombatStatsComponent* DamageInstigator)
{
	// 이 한 대로 죽었으면 사망 연출이 따로 나감
	if (!HitSequence || CombatStats->IsDead()) return;

	MulticastPlaySequence(HitSequence);
}

void ATerminusMonster::HandleDied(UCombatStatsComponent* Self)
{
	if (DeathSequence) MulticastPlayDeath();
}

void ATerminusMonster::MulticastPlaySequence_Implementation(UPaperZDAnimSequence* Sequence)
{
	PlayOverride(Sequence, false);
}

void ATerminusMonster::MulticastPlayDeath_Implementation()
{
	PlayOverride(DeathSequence, true);
}

bool ATerminusMonster::PlayOverride(UPaperZDAnimSequence* Sequence, bool bDeath)
{
	UPaperZDAnimInstance* AnimInstance = Animation ? Animation->GetAnimInstance() : nullptr;
	if (!Sequence || !AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionAnim] %s 재생 안 함: Sequence=%s AnimInstance=%s"), *GetName(),
			*GetNameSafe(Sequence), *GetNameSafe(AnimInstance));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionAnim] %s -> %s"), *GetName(), *Sequence->GetName());

	// 사망은 끝나는 순간 대기로 돌아가 한 프레임 보이는 걸 막으려고 그 자리에서 그림을 끔
	FZDOnAnimationOverrideEndSignature OnEnd;
	if (bDeath) OnEnd.BindUObject(this, &ATerminusMonster::OnDeathOverrideEnd);

	// 슬롯이 ABP 에 없으면 false (PaperZD 가 PIE 메시지 로그에 경고)
	return AnimInstance->PlayAnimationOverride(Sequence, TEXT("DefaultSlot"), 1.f, 0.f, OnEnd);
}

void ATerminusMonster::OnDeathOverrideEnd(bool bCompleted)
{
	if (bCompleted && Sprite) Sprite->SetVisibility(false);
}

void ATerminusMonster::HideAfterDeath()
{
	if (bHideQueued) return;
	bHideQueued = true;

	const float Delay = DeathSequence ? DeathSequence->GetTotalDuration() : 0.f;
	if (Delay <= 0.f)
	{
		FinishHide();
		return;
	}
	GetWorldTimerManager().SetTimer(DeathHideTimer, this, &ATerminusMonster::FinishHide, Delay, false);
}

void ATerminusMonster::FinishHide()
{
	SetActorHiddenInGame(true);   // 복제됨
}
