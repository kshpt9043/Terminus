#include "Dungeon/DungeonCombatComponent.h"

#include "Character/TerminusBattler.h"
#include "Character/TerminusMonster.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/SkillExecutor.h"
#include "Data/MonsterTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonArea.h"
#include "Dungeon/DungeonThemeData.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/TerminusPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDungeonCombat, Log, All);

namespace
{
	// 몬스터가 쓰는 스킬의 대상 해석. 몬스터 입장에서 "적" = 플레이어, "아군" = 몬스터
	TArray<UCombatStatsComponent*> ResolveTargets(ETargetType Type, UCombatStatsComponent* Self,
		const TArray<UCombatStatsComponent*>& Enemies, const TArray<UCombatStatsComponent*>& Allies)
	{
		TArray<UCombatStatsComponent*> Out;
		switch (Type)
		{
		case ETargetType::Self:
			Out.Add(Self);
			break;
		case ETargetType::SingleEnemy:
			if (Enemies.Num() > 0) Out.Add(Enemies[FMath::RandRange(0, Enemies.Num() - 1)]);
			break;
		case ETargetType::AllEnemies:
			Out = Enemies;
			break;
		case ETargetType::SingleAlly:
			if (Allies.Num() > 0) Out.Add(Allies[FMath::RandRange(0, Allies.Num() - 1)]);
			break;
		case ETargetType::AllAllies:
			Out = Allies;
			break;
		}
		return Out;
	}

	EMonsterIntentKind ToIntentKind(const FSkillRow& Skill)
	{
		switch (Skill.SkillType)
		{
		case ESkillType::Attack:  return EMonsterIntentKind::Attack;
		case ESkillType::Defense: return EMonsterIntentKind::Defend;
		default:
			// 특수: 자기편에게 걸면 버프, 상대에게 걸면 디버프
			return (Skill.TargetType == ETargetType::SingleEnemy || Skill.TargetType == ETargetType::AllEnemies)
				? EMonsterIntentKind::Debuff
				: EMonsterIntentKind::Buff;
		}
	}

	bool IsCombatRoom(ERoomType Type)
	{
		return Type == ERoomType::MONSTER || Type == ERoomType::GUARDIAN || Type == ERoomType::BOSS;
	}
}

UDungeonCombatComponent::UDungeonCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDungeonCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDungeonCombatComponent, Phase);
	DOREPLIFETIME(UDungeonCombatComponent, Cycle);
	DOREPLIFETIME(UDungeonCombatComponent, Monsters);
	DOREPLIFETIME(UDungeonCombatComponent, Intents);
	DOREPLIFETIME(UDungeonCombatComponent, EndedTurn);
}

// =====================================================================
// 조회
// =====================================================================

ADungeonArea* UDungeonCombatComponent::GetArea() const
{
	return Cast<ADungeonArea>(GetOwner());
}

const FMonsterIntent* UDungeonCombatComponent::FindIntent(const ATerminusMonster* Monster) const
{
	return Intents.FindByPredicate([Monster](const FMonsterIntent& I) { return I.Monster == Monster; });
}

bool UDungeonCombatComponent::HasEndedTurn(const ATerminusPlayerState* PS) const
{
	return PS && EndedTurn.Contains(PS);
}

UCombatStatsComponent* UDungeonCombatComponent::GetStats(const ATerminusPlayerState* PS) const
{
	const ATerminusBattler* Battler = PS ? Cast<ATerminusBattler>(PS->GetPawn()) : nullptr;
	return Battler ? Battler->GetCombatStats() : nullptr;
}

TArray<UCombatStatsComponent*> UDungeonCombatComponent::GetAlivePlayerStats() const
{
	TArray<UCombatStatsComponent*> Out;
	for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
	{
		UCombatStatsComponent* Stats = GetStats(PS.Get());
		if (Stats && !Stats->IsDead()) Out.Add(Stats);
	}
	return Out;
}

TArray<UCombatStatsComponent*> UDungeonCombatComponent::GetAliveMonsterStats() const
{
	TArray<UCombatStatsComponent*> Out;
	for (ATerminusMonster* Monster : Monsters)
	{
		UCombatStatsComponent* Stats = Monster ? Monster->GetCombatStats() : nullptr;
		if (Stats && !Stats->IsDead()) Out.Add(Stats);
	}
	return Out;
}

// =====================================================================
// 시작 / 정리
// =====================================================================

void UDungeonCombatComponent::StartCombat(const FRoomNode& Room, const TArray<ATerminusPlayerState*>& InPlayers, const UDungeonThemeData* Theme)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	EndCombat();   // 혹시 남은 게 있으면

	if (!IsCombatRoom(Room.Type)) return;

	for (ATerminusPlayerState* PS : InPlayers)
	{
		if (PS) Players.Add(PS);
	}

	SpawnMonsters(Room, Theme);

	if (Monsters.Num() == 0)
	{
		UE_LOG(LogDungeonCombat, Warning, TEXT("[Combat] %d번 방: 스폰된 몬스터가 없어 바로 클리어"), Room.RoomId);
		FinishCombat(true);
		return;
	}

	UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %d번 방(%s) 전투 시작. 플레이어 %d, 몬스터 %d"),
		Room.RoomId, *UEnum::GetValueAsString(Room.Type), Players.Num(), Monsters.Num());

	Cycle = 0;
	StartCycle();
}

void UDungeonCombatComponent::EndCombat()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}

	for (ATerminusMonster* Monster : Monsters)
	{
		if (IsValid(Monster)) Monster->Destroy();
	}

	Monsters.Reset();
	MonsterRows.Reset();
	Intents.Reset();
	EndedTurn.Reset();
	Players.Reset();
	Cycle = 0;
	Phase = ECombatPhase::None;
}

// =====================================================================
// 몬스터 스폰
// =====================================================================

TArray<FName> UDungeonCombatComponent::PickMonsterRows(ERoomType RoomType, const UDungeonThemeData* Theme, int32 MaxCount) const
{
	TArray<FName> Out;

	const UDataTable* Table = UTerminusDataSettings::Get()->MonsterTable.LoadSynchronous();
	if (!Table || MaxCount <= 0) return Out;

	const EMonsterCategory Category =
		RoomType == ERoomType::GUARDIAN ? EMonsterCategory::Guardian :
		RoomType == ERoomType::BOSS     ? EMonsterCategory::Boss :
		                                  EMonsterCategory::Normal;

	// 테마 이름 = 몬스터 Theme_KR (예: "슬라임 왕국")
	const FString ThemeName = Theme ? Theme->DisplayName.ToString() : FString();

	TArray<FName> SameCategory;
	TArray<FName> SameCategoryAndTheme;

	Table->ForeachRow<FMonsterRow>(TEXT("PickMonsterRows"), [&](const FName& Key, const FMonsterRow& Row)
	{
		if (Row.MonsterCategory != Category) return;

		SameCategory.Add(Key);
		if (!ThemeName.IsEmpty() && Row.Theme_KR.ToString() == ThemeName)
		{
			SameCategoryAndTheme.Add(Key);
		}
	});

	const TArray<FName>& Pool = SameCategoryAndTheme.Num() > 0 ? SameCategoryAndTheme : SameCategory;
	if (Pool.Num() == 0) return Out;

	if (SameCategoryAndTheme.Num() == 0)
	{
		UE_LOG(LogDungeonCombat, Warning, TEXT("[Combat] 테마 '%s' 의 %s 몬스터가 없어 테마 무시하고 고름 (테마 DisplayName 과 DT_Monster Theme_KR 을 맞출 것)"),
			*ThemeName, *UEnum::GetValueAsString(Category));
	}

	// 몬스터방 1~3 마리, 가디언 / 보스는 1 마리
	const int32 Count = (Category == EMonsterCategory::Normal)
		? FMath::Clamp(FMath::RandRange(NormalMonsterCount.X, NormalMonsterCount.Y), 1, MaxCount)
		: 1;

	for (int32 i = 0; i < Count; ++i)
	{
		Out.Add(Pool[FMath::RandRange(0, Pool.Num() - 1)]);
	}
	return Out;
}

void UDungeonCombatComponent::SpawnMonsters(const FRoomNode& Room, const UDungeonThemeData* Theme)
{
	ADungeonArea* Area = GetArea();
	UWorld* World = GetWorld();
	if (!Area || !World) return;

	TSubclassOf<ATerminusMonster> SpawnClass = MonsterClass;
	if (!SpawnClass)
	{
		SpawnClass = LoadClass<ATerminusMonster>(nullptr, TEXT("/Game/KSH/Characters/Monster/BP_Monster.BP_Monster_C"));
		UE_LOG(LogDungeonCombat, Warning, TEXT("[Combat] MonsterClass 가 비어 있어 BP_Monster 를 경로로 찾음 (%s). BP_DungeonArea > Combat 에서 지정할 것"),
			SpawnClass ? TEXT("찾음") : TEXT("못 찾음"));
	}
	if (!SpawnClass) SpawnClass = ATerminusMonster::StaticClass();

	const TArray<FName> Rows = PickMonsterRows(Room.Type, Theme, FMath::Max(1, Area->GetNumMonsterSlots()));

	// 기획: 멀티 체력 보정은 그 방에 들어온 인원 기준
	const int32 PartySize = FMath::Max(1, Players.Num());
	const float PartyBonus = PartyHealthBonus.IsValidIndex(PartySize - 1) ? PartyHealthBonus[PartySize - 1]
		: (PartyHealthBonus.Num() > 0 ? PartyHealthBonus.Last() : 0.f);

	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const FMonsterRow* Row = UTerminusDataSettings::FindMonsterRow(Rows[i]);
		if (!Row) continue;

		const FTransform SpawnTransform(Area->GetMonsterSlotLocation(i));
		ATerminusMonster* Monster = World->SpawnActorDeferred<ATerminusMonster>(SpawnClass, SpawnTransform, Area, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Monster) continue;

		// 구역끼리 떨어져 있어도 모든 클라가 받게 (관전 대비). 몇 마리 안 됨
		Monster->bAlwaysRelevant = true;
		Monster->FinishSpawning(SpawnTransform);

		// 스텟: 인원 보정 먼저, 그다음 개체 랜덤 (기획 순서). 보스는 개체 랜덤 없음
		FCharacterStats Stats = Row->ToStats();
		const bool bBoss = Row->MonsterCategory == EMonsterCategory::Boss;

		float HealthScale = 1.f + PartyBonus;
		if (!bBoss)
		{
			HealthScale *= 1.f + FMath::FRandRange(MonsterHealthRandom.X, MonsterHealthRandom.Y);
			Stats.Attack += FMath::RandRange(0, MonsterStatRandomMax);
			Stats.Defense += FMath::RandRange(0, MonsterStatRandomMax);
		}
		Stats.MaxHealth = FMath::Max(1, FMath::RoundToInt(Stats.MaxHealth * HealthScale));

		// BeginPlay 에서 기본 행으로 한 번 초기화된 걸 실제 값으로 덮어씀
		Monster->GetCombatStats()->InitFrom(Stats);

		Monsters.Add(Monster);
		MonsterRows.Add(Rows[i]);

		UE_LOG(LogDungeonCombat, Log, TEXT("[Combat]   %s 스폰: 체력 %d 공격 %d 방어 %d"),
			*Rows[i].ToString(), Stats.MaxHealth, Stats.Attack, Stats.Defense);
	}
}

FName UDungeonCombatComponent::PickMonsterSkill(FName MonsterRow) const
{
	TArray<TPair<FName, int32>> Candidates;

	if (const FMonsterRow* Row = UTerminusDataSettings::FindMonsterRow(MonsterRow))
	{
		const TPair<FName, int32> Slots[] = {
			{ Row->Skill1_ID, Row->Skill1_Weight },
			{ Row->Skill2_ID, Row->Skill2_Weight },
			{ Row->Skill3_ID, Row->Skill3_Weight }
		};
		for (const TPair<FName, int32>& Slot : Slots)
		{
			if (!Slot.Key.IsNone() && Slot.Value > 0) Candidates.Add(Slot);
		}
	}

	if (Candidates.Num() == 0)
	{
		// DT_Monster 의 스킬 칸이 비어 있음 -> 임시 스킬
		if (!WarnedFallbackRows.Contains(MonsterRow))
		{
			WarnedFallbackRows.Add(MonsterRow);
			UE_LOG(LogDungeonCombat, Warning, TEXT("[Combat] %s: DT_Monster 에 스킬이 비어 있어 임시 스킬(FallbackMonsterSkills)을 씀"), *MonsterRow.ToString());
		}

		for (const TPair<FName, int32>& Pair : FallbackMonsterSkills)
		{
			if (Pair.Value > 0) Candidates.Add(Pair);
		}
	}

	int32 Total = 0;
	for (const TPair<FName, int32>& C : Candidates) Total += C.Value;
	if (Total <= 0) return NAME_None;

	int32 Roll = FMath::RandRange(1, Total);
	for (const TPair<FName, int32>& C : Candidates)
	{
		Roll -= C.Value;
		if (Roll <= 0) return C.Key;
	}
	return Candidates.Last().Key;
}

// =====================================================================
// 사이클
// =====================================================================

void UDungeonCombatComponent::StartCycle()
{
	++Cycle;
	EndedTurn.Reset();

	// 플레이어 보호막은 사이클이 끝나면 사라짐 (몬스터 턴까지 버티고 여기서 버림), 에너지는 최대치로
	for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
	{
		if (UCombatStatsComponent* Stats = GetStats(PS.Get()))
		{
			if (Stats->IsDead()) continue;
			Stats->ClearShield();
			Stats->RefillEnergy();
		}
	}

	ChooseIntents();
	Phase = ECombatPhase::PlayerTurn;

	UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] 사이클 %d 시작. 플레이어 턴"), Cycle);
}

void UDungeonCombatComponent::ChooseIntents()
{
	Intents.Reset();

	for (int32 i = 0; i < Monsters.Num(); ++i)
	{
		ATerminusMonster* Monster = Monsters[i];
		UCombatStatsComponent* Stats = Monster ? Monster->GetCombatStats() : nullptr;
		if (!Stats || Stats->IsDead()) continue;

		const FName SkillName = PickMonsterSkill(MonsterRows[i]);
		const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(SkillName);
		if (!Skill)
		{
			UE_LOG(LogDungeonCombat, Warning, TEXT("[Combat] %s 의 스킬 '%s' 이 DT_Skill 에 없음"), *MonsterRows[i].ToString(), *SkillName.ToString());
			continue;
		}

		FMonsterIntent& Intent = Intents.AddDefaulted_GetRef();
		Intent.Monster = Monster;
		Intent.SkillRow = SkillName;
		Intent.Kind = ToIntentKind(*Skill);
		Intent.Amount = FSkillExecutor::PreviewAmount(*Skill, Stats);
		Intent.HitCount = FMath::Max(1, Skill->HitCount);
	}
}

void UDungeonCombatComponent::HandleUseSkill(ATerminusPlayerState* PS, int32 SkillIndex, int32 TargetIndex)
{
	if (Phase != ECombatPhase::PlayerTurn || !PS || EndedTurn.Contains(PS)) return;
	if (!Players.Contains(PS)) return;

	UCombatStatsComponent* Caster = GetStats(PS);
	if (!Caster || Caster->IsDead()) return;

	// 0~2 기본 스킬, 3~5 장착한 강화 스킬
	const FSkillRow* SkillPtr = PS->GetCombatSkill(SkillIndex);
	if (!SkillPtr) return;
	const FSkillRow& Skill = *SkillPtr;

	// 대상 먼저 확인 (에너지만 날리고 실패하지 않게)
	TArray<UCombatStatsComponent*> Targets;
	switch (Skill.TargetType)
	{
	case ETargetType::Self:
		Targets.Add(Caster);
		break;
	case ETargetType::SingleEnemy:
		if (Monsters.IsValidIndex(TargetIndex) && Monsters[TargetIndex])
		{
			UCombatStatsComponent* T = Monsters[TargetIndex]->GetCombatStats();
			if (T && !T->IsDead()) Targets.Add(T);
		}
		break;
	case ETargetType::AllEnemies:
		Targets = GetAliveMonsterStats();
		break;
	case ETargetType::SingleAlly:
		if (Players.IsValidIndex(TargetIndex))
		{
			UCombatStatsComponent* T = GetStats(Players[TargetIndex].Get());
			if (T && !T->IsDead()) Targets.Add(T);
		}
		break;
	case ETargetType::AllAllies:
		Targets = GetAlivePlayerStats();
		break;
	}

	if (Targets.Num() == 0)
	{
		UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %s: 대상이 없어 '%s' 취소"), *PS->GetPlayerName(), *Skill.DisplayName_KR.ToString());
		return;
	}

	// 비용은 둘 다 되는지 먼저 보고 나서 씀 (한쪽만 빠지지 않게)
	const FCombatState& CasterState = Caster->GetCombatState();
	if (CasterState.Energy < Skill.EnergyCost || CasterState.SkillEnergy < Skill.SkillEnergyCost)
	{
		UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %s: 에너지 부족 (%s)"), *PS->GetPlayerName(), *Skill.DisplayName_KR.ToString());
		return;
	}
	Caster->SpendEnergy(Skill.EnergyCost);
	Caster->SpendSkillEnergy(Skill.SkillEnergyCost);

	FSkillExecutor::Execute(Skill, Caster, Targets);

	UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %s -> %s"), *PS->GetPlayerName(), *Skill.DisplayName_KR.ToString());

	HideDeadMonsters();
	CheckCombatEnd();
}

void UDungeonCombatComponent::HandleEndTurn(ATerminusPlayerState* PS)
{
	if (Phase != ECombatPhase::PlayerTurn || !PS || !Players.Contains(PS) || EndedTurn.Contains(PS)) return;

	EndedTurn.Add(PS);

	// 자기 턴 끝: 중독 피해, 남은 에너지 버림
	if (UCombatStatsComponent* Stats = GetStats(PS))
	{
		Stats->OnTurnEnd();
		Stats->DrainEnergy();
	}

	if (CheckCombatEnd()) return;

	// 살아 있는 사람이 전원 턴 종료했으면 몬스터 턴
	for (const TWeakObjectPtr<ATerminusPlayerState>& Other : Players)
	{
		UCombatStatsComponent* Stats = GetStats(Other.Get());
		if (Stats && !Stats->IsDead() && !EndedTurn.Contains(Other.Get())) return;
	}

	BeginMonsterTurn();
}

void UDungeonCombatComponent::DebugKillAllMonsters()
{
	if (!IsInCombat() || Phase == ECombatPhase::Victory || Phase == ECombatPhase::Defeat) return;

	for (UCombatStatsComponent* Stats : GetAliveMonsterStats())
	{
		Stats->ApplyDamage(Stats->GetCombatState().Health + Stats->GetCombatState().Shield);
	}

	HideDeadMonsters();
	CheckCombatEnd();
}

void UDungeonCombatComponent::BeginMonsterTurn()
{
	Phase = ECombatPhase::MonsterTurn;
	NextMonsterIndex = 0;

	UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] 사이클 %d 몬스터 턴"), Cycle);

	GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UDungeonCombatComponent::RunNextMonsterAction, MonsterActionInterval, false);
}

void UDungeonCombatComponent::RunNextMonsterAction()
{
	if (Phase != ECombatPhase::MonsterTurn) return;

	// 다음으로 살아 있는 몬스터
	while (Monsters.IsValidIndex(NextMonsterIndex))
	{
		ATerminusMonster* Monster = Monsters[NextMonsterIndex++];
		UCombatStatsComponent* Stats = Monster ? Monster->GetCombatStats() : nullptr;
		if (!Stats || Stats->IsDead()) continue;

		// 몬스터 보호막은 "다음 자기 턴이 돌아올 때까지" -> 행동 직전에 버림
		Stats->ClearShield();

		const FMonsterIntent* Intent = FindIntent(Monster);
		const FSkillRow* Skill = Intent ? UTerminusDataSettings::FindSkillRow(Intent->SkillRow) : nullptr;
		if (Skill)
		{
			const TArray<UCombatStatsComponent*> Targets =
				ResolveTargets(Skill->TargetType, Stats, GetAlivePlayerStats(), GetAliveMonsterStats());

			FSkillExecutor::Execute(*Skill, Stats, Targets);
			UE_LOG(LogDungeonCombat, Log, TEXT("[Combat]   몬스터 %d -> %s"), NextMonsterIndex - 1, *Skill->DisplayName_KR.ToString());
		}

		HideDeadMonsters();
		if (CheckCombatEnd()) return;

		// 한 마리씩 간격을 두고
		GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UDungeonCombatComponent::RunNextMonsterAction, MonsterActionInterval, false);
		return;
	}

	FinishCycle();
}

void UDungeonCombatComponent::FinishCycle()
{
	// 몬스터 턴 끝: 몬스터 중독 피해
	for (UCombatStatsComponent* Stats : GetAliveMonsterStats())
	{
		Stats->OnTurnEnd();
	}

	// 사이클 끝: 모두의 상태이상 지속시간 1 감소
	for (UCombatStatsComponent* Stats : GetAliveMonsterStats()) Stats->OnCycleEnd();
	for (UCombatStatsComponent* Stats : GetAlivePlayerStats())  Stats->OnCycleEnd();

	HideDeadMonsters();
	if (CheckCombatEnd()) return;

	StartCycle();
}

// =====================================================================
// 승패
// =====================================================================

void UDungeonCombatComponent::HideDeadMonsters()
{
	for (ATerminusMonster* Monster : Monsters)
	{
		UCombatStatsComponent* Stats = Monster ? Monster->GetCombatStats() : nullptr;
		if (Stats && Stats->IsDead() && !Monster->IsHidden())
		{
			Monster->SetActorHiddenInGame(true);   // 복제됨
		}
	}
}

bool UDungeonCombatComponent::CheckCombatEnd()
{
	if (Phase == ECombatPhase::Victory || Phase == ECombatPhase::Defeat || Phase == ECombatPhase::None) return true;

	if (GetAliveMonsterStats().Num() == 0)
	{
		FinishCombat(true);
		return true;
	}

	if (GetAlivePlayerStats().Num() == 0)
	{
		FinishCombat(false);
		return true;
	}

	return false;
}

void UDungeonCombatComponent::FinishCombat(bool bVictory)
{
	Phase = bVictory ? ECombatPhase::Victory : ECombatPhase::Defeat;
	Intents.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}

	UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] 전투 %s (사이클 %d)"), bVictory ? TEXT("승리") : TEXT("패배"), Cycle);

	if (!bVictory)
	{
		// TODO: 사망 로직 (기획 사망 순서도: 멀티면 구출 / 난입, 싱글이면 유물만 판매하고 로비로)
		// 아직 없어서 임시로 체력 1 로 일으켜 세우고 방을 넘김 -> 흐름이 멈추지 않게
		UE_LOG(LogDungeonCombat, Warning, TEXT("[Combat] 사망 로직 미구현: 임시로 체력 1 로 부활시키고 방을 끝냄"));

		for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
		{
			if (UCombatStatsComponent* Stats = GetStats(PS.Get()))
			{
				if (Stats->IsDead())
				{
					// 죽은 상태에선 Heal 이 막혀 있어서 같은 스텟으로 다시 초기화한 뒤 1 만 남김
					const FCharacterStats Saved = Stats->GetStats();
					Stats->InitFrom(Saved);
					Stats->ApplyDamage(Saved.MaxHealth - 1);
				}
			}
		}
	}

	// 잠시 결과를 보여 준 뒤 구역 클리어 -> 모든 구역이 끝나면 지도로
	TWeakObjectPtr<ADungeonArea> WeakArea = GetArea();
	GetWorld()->GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateWeakLambda(this, [WeakArea]()
	{
		if (ADungeonArea* Area = WeakArea.Get())
		{
			Area->MarkCleared();
		}
	}), CombatEndDelay, false);
}
