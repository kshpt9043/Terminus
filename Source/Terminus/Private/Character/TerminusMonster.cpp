#include "Character/TerminusMonster.h"

#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "TimerManager.h"
#include "Combat/CombatStatsComponent.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonCombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "PaperFlipbook.h"

ATerminusMonster::ATerminusMonster()
{
	// 몬스터는 오른쪽에 서서 플레이어 쪽(왼쪽)을 봄
	bFacingRight = false;
}

void ATerminusMonster::BeginPlay()
{
	Super::BeginPlay();

	InitAsMonster(MonsterRow);
	ApplyVisual();

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

void ATerminusMonster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerminusMonster, MonsterRow);
}

void ATerminusMonster::SetMonsterRow(FName RowName)
{
	MonsterRow = RowName;
	if (HasActorBegunPlay()) ApplyVisual();   // 보통은 FinishSpawning 전이라 BeginPlay 가 적용함
}

void ATerminusMonster::OnRep_MonsterRow()
{
	ApplyVisual();
}

void ATerminusMonster::ApplyVisual()
{
	const FMonsterVisualRow* Look = UTerminusDataSettings::FindMonsterVisualRow(MonsterRow);
	UPaperFlipbook* Idle = Look ? Look->Idle.LoadSynchronous() : nullptr;
	if (!Idle) return;   // 외형 표에 없음 -> BP 의 ABP 그대로

	VisualIdle = Idle;
	VisualAttack = Look->Attack.LoadSynchronous();
	VisualHit = Look->Hit.LoadSynchronous();
	VisualDefend = Look->Defend.LoadSynchronous();
	VisualSpecial = Look->Special.LoadSynchronous();
	VisualDeath = Look->Death.LoadSynchronous();

	// ABP 가 매 프레임 플립북을 덮어쓰니 끄고, 플립북을 직접 재생 (배틀러의 MulticastPlayActionFlipbook 경로)
	if (Animation) Animation->SetAnimInstanceClass(nullptr);
	if (Sprite)
	{
		Sprite->SetFlipbook(VisualIdle);
		Sprite->SetLooping(true);
		Sprite->PlayFromStart();
	}
	UE_LOG(LogTemp, Log, TEXT("[MonsterVisual] %s -> %s (%s)"), *GetName(), *MonsterRow.ToString(), *Look->Source);
}

void ATerminusMonster::PlayIntentAnim(EMonsterIntentKind Kind)
{
	if (UsesFlipbooks())
	{
		UPaperFlipbook* Flipbook = Kind == EMonsterIntentKind::Attack ? VisualAttack.Get()
			: Kind == EMonsterIntentKind::Defend ? VisualDefend.Get()
			: VisualSpecial.Get();
		if (Flipbook) MulticastPlayActionFlipbook(Flipbook);
		return;
	}

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
	if (CombatStats->IsDead()) return;
	if (UsesFlipbooks())
	{
		if (VisualHit) MulticastPlayActionFlipbook(VisualHit);
		return;
	}
	if (!HitSequence) return;

	MulticastPlaySequence(HitSequence);
}

void ATerminusMonster::HandleDied(UCombatStatsComponent* Self)
{
	if (DeathSequence || VisualDeath) MulticastPlayDeath();
}

void ATerminusMonster::MulticastPlaySequence_Implementation(UPaperZDAnimSequence* Sequence)
{
	PlayOverride(Sequence, false);
}

void ATerminusMonster::MulticastPlayDeath_Implementation()
{
	if (UsesFlipbooks())
	{
		if (!VisualDeath || !Sprite) return;
		// 끝나면 대기로 돌아가지 않고 그림을 끔
		Sprite->OnFinishedPlaying.RemoveAll(this);
		Sprite->OnFinishedPlaying.AddUniqueDynamic(this, &ATerminusMonster::OnDeathFlipbookFinished);
		RestFlipbook = nullptr;
		Sprite->SetFlipbook(VisualDeath);
		Sprite->SetLooping(false);
		Sprite->PlayFromStart();
		UE_LOG(LogTemp, Log, TEXT("[ActionAnim] %s -> %s"), *GetName(), *VisualDeath->GetName());
		return;
	}
	PlayOverride(DeathSequence, true);
}

void ATerminusMonster::OnDeathFlipbookFinished()
{
	Sprite->OnFinishedPlaying.RemoveDynamic(this, &ATerminusMonster::OnDeathFlipbookFinished);
	Sprite->SetVisibility(false);
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

	const float Delay = UsesFlipbooks()
		? (VisualDeath ? VisualDeath->GetTotalDuration() : 0.f)
		: (DeathSequence ? DeathSequence->GetTotalDuration() : 0.f);
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
