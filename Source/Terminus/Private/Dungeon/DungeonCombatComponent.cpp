#include "Dungeon/DungeonCombatComponent.h"

#include "Character/TerminusBattler.h"
#include "Character/TerminusMonster.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/SkillExecutor.h"
#include "Data/MonsterTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Data/RelicTypes.h"
#include "Dungeon/DungeonArea.h"
#include "Dungeon/DungeonThemeData.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/TerminusPlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Game/TerminusRunSubsystem.h"
#include "Kismet/GameplayStatics.h"
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

	CurrentRoomType = Room.Type;
	CurrentRoomId = Room.RoomId;
	PendingRewards.Reset();
	SpawnMonsters(Room, Theme);
	BuildRelicHolders();

	// 이벤트 방에서 받은 일시 버프: 이번 전투 동안 용기(공격) / 용암(방어 감소)
	for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
	{
		ApplyTempBuffs(PS.Get());
	}


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

void UDungeonCombatComponent::ApplyTempBuffs(ATerminusPlayerState* PS)
{
	UCombatStatsComponent* Stats = GetStats(PS);
	if (!PS || !Stats) return;

	for (const FTempStatBuff& Buff : PS->GetRunState().TempBuffs)
	{
		if (Buff.Attack > 0)  Stats->ApplyStatus(EStatusEffect::Brave, Buff.Attack, -1);
		if (Buff.Defense < 0) Stats->ApplyStatus(EStatusEffect::Lava, -Buff.Defense, -1);
	}
}

void UDungeonCombatComponent::ResumeWithPlayers(const TArray<ATerminusPlayerState*>& Joiners)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Phase != ECombatPhase::Defeat) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}

	// 기획 사망 순서도 '난입': 남아 있는 몬스터들과 새롭게 전투. 몬스터 체력은 그대로, 쓰러진 사람은 이기면 체력 1 로
	// 들어온 사람만 유물 홀더 추가 (몬스터 유물의 남은 횟수 같은 건 그대로 둠)
	TArray<int32> JoinerHolders;
	for (ATerminusPlayerState* PS : Joiners)
	{
		if (!PS || Players.Contains(PS)) continue;
		Players.Add(PS);
		ApplyTempBuffs(PS);

		const int32 HolderIndex = AddRelicHolder(GetStats(PS), PS, false, PS->GetRelics());
		if (HolderIndex != INDEX_NONE) JoinerHolders.Add(HolderIndex);
	}
	PendingRewards.Reset();

	UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %d번 방 난입: %d명 합류, 남은 몬스터 %d"), CurrentRoomId, Joiners.Num(), GetAliveMonsterStats().Num());
	StartCycle();

	// 들어온 사람에겐 새 전투 -> 전투 시작 유물 발동 (첫 턴 에너지 회복 뒤, 보통 전투와 같은 순서)
	for (const int32 HolderIndex : JoinerHolders)
	{
		FireRelics(HolderIndex, ERelicTrigger::OnBattleStart);
		if (CurrentRoomType == ERoomType::GUARDIAN) FireRelics(HolderIndex, ERelicTrigger::OnGuardianBattleStart);
		if (CurrentRoomType == ERoomType::BOSS)     FireRelics(HolderIndex, ERelicTrigger::OnBossBattleStart);
	}
}

void UDungeonCombatComponent::EndCombat()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}

	// 싸우는 도중에 닫힘(누가 나가서 방 취소): 전투에서만 의미 있는 효과를 지우고, 쓰러진 사람은 체력 1 로
	if (Phase == ECombatPhase::PlayerTurn || Phase == ECombatPhase::MonsterTurn)
	{
		for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
		{
			if (UCombatStatsComponent* Stats = GetStats(PS.Get()))
			{
				Stats->ClearCombatEffects();
				if (Stats->IsDead())
				{
					const FCharacterStats Saved = Stats->GetStats();
					Stats->InitFrom(Saved);
					Stats->ApplyDamage(Saved.MaxHealth - 1);
				}
			}
		}
	}

	// 보상 화면이 떠 있던 사람은 닫음
	for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, FPendingReward>& Pair : PendingRewards)
	{
		ATerminusPlayerState* PS = Pair.Key.Get();
		ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr;
		if (PC && !Pair.Value.bDone)
		{
			PC->Client_CloseMonsterReward();
		}
	}

	ClearRelicHolders();

	for (ATerminusMonster* Monster : Monsters)
	{
		if (IsValid(Monster)) Monster->Destroy();
	}


	Monsters.Reset();
	MonsterRows.Reset();
	Intents.Reset();
	PendingRewards.Reset();
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

	// 가디언: 그 테마 가디언이 전부 한 번씩 나오기 전엔 중복으로 안 나옴 (기획 10-07). 다 나왔으면 다시 처음부터
	// 이번 런에 나온 가디언은 런 서브시스템이 들고 있고 세이브에도 들어감
	if (Category == EMonsterCategory::Guardian)
	{
		const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UTerminusRunSubsystem* Run = GI ? GI->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
		{
			TArray<FName> Fresh = Pool.FilterByPredicate([Run](const FName& Row) { return !Run->GetSeenGuardians().Contains(Row); });
			if (Fresh.Num() == 0)
			{
				Run->ForgetSeenGuardians(Pool);
				Fresh = Pool;
			}

			const FName Picked = Fresh[FMath::RandRange(0, Fresh.Num() - 1)];
			Run->AddSeenGuardian(Picked);
			Out.Add(Picked);
			return Out;
		}
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

	// 기획(10-07): 테마의 두 번째 층 몬스터 / 가디언(보스 제외)은 기본 체력 +30%, 공방 +1
	const AMapManager* FloorMap = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	const bool bSecondFloorOfTheme = FloorMap && AMapManager::IsThemeEndFloor(FloorMap->CurrentFloor);

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

		// 스텟: 층 보정 -> 인원 체력 보정 -> 개체 랜덤 (기획 순서)
		FCharacterStats Stats = Row->ToStats();
		const bool bBoss = Row->MonsterCategory == EMonsterCategory::Boss;
		if (bSecondFloorOfTheme && !bBoss)
		{
			Stats.MaxHealth = FMath::Max(1, FMath::RoundToInt(Stats.MaxHealth * (1.f + SecondFloorHealthBonus)));
			Stats.Attack += SecondFloorStatBonus;
			Stats.Defense += SecondFloorStatBonus;
		}

		// 멀티: 방에 들어온 플레이어 1명당 공방 +1 (보스 포함, 혼자여도 +1)
		Stats.Attack += PartyStatBonusPerPlayer * PartySize;
		Stats.Defense += PartyStatBonusPerPlayer * PartySize;

		// 체력 랜덤은 보스 포함 (사용자 결정 10-08), 공방 랜덤은 보스 제외
		float HealthScale = 1.f + PartyBonus;
		HealthScale *= 1.f + FMath::FRandRange(MonsterHealthRandom.X, MonsterHealthRandom.Y);
		if (!bBoss)
		{
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

	Phase = ECombatPhase::PlayerTurn;

	// 전투 시작 유물은 첫 턴 에너지 회복 "뒤" (보조 배터리: 3 -> 4)
	if (Cycle == 1)
	{
		FireRelicsForAll(ERelicTrigger::OnBattleStart, true, true);
		if (CurrentRoomType == ERoomType::GUARDIAN) FireRelicsForAll(ERelicTrigger::OnGuardianBattleStart, true, true);
		if (CurrentRoomType == ERoomType::BOSS)     FireRelicsForAll(ERelicTrigger::OnBossBattleStart, true, true);
	}

	// 플레이어 턴 시작: 활력(턴 시작 에너지 +수치) 다음 턴 시작 유물
	for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
	{
		UCombatStatsComponent* Stats = GetStats(PS.Get());
		if (!Stats || Stats->IsDead()) continue;

		if (const int32 Vitality = Stats->GetStatusValue(EStatusEffect::Vitality); Vitality > 0)
		{
			Stats->AddEnergy(Vitality, true);
		}
		FireRelics(FindHolder(Stats), ERelicTrigger::OnTurnStart);
	}

	// 유탄 발사기처럼 시작하자마자 몬스터가 다 죽을 수도 있음
	HideDeadMonsters();
	if (CheckCombatEnd()) return;

	ChooseIntents();


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

	// 기본 스킬은 훈련소 강화만큼 수치가 오름 (복사본에만)
	FSkillRow Skill = *SkillPtr;
	PS->ApplyBasicSkillUpgrade(SkillIndex, Skill);

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
	const int32 EnergyCost = Caster->GetEffectiveEnergyCost(Skill.EnergyCost);   // 과욕(산성) +1
	if (CasterState.Energy < EnergyCost || CasterState.SkillEnergy < Skill.SkillEnergyCost)
	{
		UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %s: 에너지 부족 (%s)"), *PS->GetPlayerName(), *Skill.DisplayName_KR.ToString());
		return;
	}
	Caster->SpendEnergy(EnergyCost);
	Caster->SpendSkillEnergy(Skill.SkillEnergyCost);

	FSkillExecutor::Execute(Skill, Caster, Targets);
	FireRelics(FindHolder(Caster), ERelicTrigger::OnSkillUsed, 0, PS->GetCombatSkillRow(SkillIndex), &Skill);


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
		FireRelics(FindHolder(Stats), ERelicTrigger::OnTurnEnd);
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
		// 테스트용이라 몬스터 유물 / 상태를 무시하고 확실히 죽임
		// (왕의 위엄 = 피해 1 고정, 고대의 슬라임 코어 = 부활 -> 보스방에서 안 죽던 원인)
		Stats->PreventDeath.Unbind();
		Stats->ClearCombatEffects();
		Stats->ApplyDamage(Stats->GetCombatState().Health + Stats->GetCombatState().Shield);

		if (!Stats->IsDead())
		{
			UE_LOG(LogDungeonCombat, Warning, TEXT("[Debug] %s 가 안 죽음 (체력 %d)"), *GetNameSafe(Stats->GetOwner()), Stats->GetCombatState().Health);
		}
	}

	HideDeadMonsters();
	CheckCombatEnd();
}

void UDungeonCombatComponent::DebugKillAllPlayers()
{
	if (!IsInCombat() || Phase == ECombatPhase::Victory || Phase == ECombatPhase::Defeat) return;

	for (UCombatStatsComponent* Stats : GetAlivePlayerStats())
	{
		// 부활 / 피해 감소 유물을 무시하고 확실히 쓰러뜨림
		Stats->PreventDeath.Unbind();
		Stats->ClearCombatEffects();
		Stats->ApplyDamage(Stats->GetCombatState().Health + Stats->GetCombatState().Shield);
	}

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
		FireRelics(FindHolder(Stats), ERelicTrigger::OnTurnStart);
		if (Stats->IsDead()) continue;


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
		FireRelics(FindHolder(Stats), ERelicTrigger::OnTurnEnd);
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

	// 살아 있는 플레이어의 전투 종료 유물 (재화 / 회복 / 최대 체력). 그다음 전투에서만 의미 있는 것들을 지움
	FireRelicsForAll(ERelicTrigger::OnBattleEnd, true, false);
	for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
	{
		if (UCombatStatsComponent* Stats = GetStats(PS.Get()))
		{
			Stats->ClearCombatEffects();
		}

		// 이벤트 일시 버프는 전투 한 번이 끝날 때마다 1 씩
		if (ATerminusPlayerState* Player = PS.Get())
		{
			Player->ConsumeTempBuffBattle();
		}
	}


	// 기획 사망 순서도
	//  - 이김: 쓰러졌던 사람은 체력 1 로 부활
	//    반드시 위의 전투 종료 유물(OnBattleEnd) 발동 '뒤'에 살릴 것: 쓰러져 있던 사람(난입으로 살아나는 사람 포함)은
	//    전투 종료 유물이 발동하면 안 됨 (사용자 결정 10-08). FireRelics 는 죽은 사람을 건너뜀
	//  - 이 구역 전멸: 잠시 뒤 구역에 알림 -> 다른 방 동료가 구출 / 난입, 아무도 없으면 런 끝 (구역 서브시스템이 판단)
	if (bVictory)
	{
		for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
		{
			UCombatStatsComponent* Stats = GetStats(PS.Get());
			if (Stats && Stats->IsDead())
			{
				Stats->Revive(0.f);   // 최소 1
				UE_LOG(LogDungeonCombat, Log, TEXT("[Combat] %s 체력 1 로 부활"), *PS->GetPlayerName());
			}
		}
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UDungeonCombatComponent::ReportWipe, CombatEndDelay, false);
		return;
	}

	// 몬스터 / 가디언 / 보스방 승리: 잠시 뒤 각자 보상 화면 -> 전원 '다음으로' 를 누르면 구역 클리어
	const bool bRewardRoom = CurrentRoomType == ERoomType::MONSTER || CurrentRoomType == ERoomType::GUARDIAN || CurrentRoomType == ERoomType::BOSS;
	if (bVictory && bRewardRoom && Players.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UDungeonCombatComponent::StartRoomRewards, RewardDelay, false);
		return;
	}

	// 그 외: 잠시 결과를 보여 준 뒤 구역 클리어 -> 모든 구역이 끝나면 지도로
	GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UDungeonCombatComponent::ClearArea, CombatEndDelay, false);
}

void UDungeonCombatComponent::ReportWipe()
{
	if (ADungeonArea* Area = GetArea())
	{
		Area->MarkWiped();
	}
}

void UDungeonCombatComponent::ClearArea()
{
	if (ADungeonArea* Area = GetArea())
	{
		Area->MarkCleared();
	}
}

// =====================================================================
// 몬스터방 보상
// =====================================================================

void UDungeonCombatComponent::StartRoomRewards()
{
	PendingRewards.Reset();

	// 방 종류별 보상 (사용자 결정 2026-10-06)
	//  몬스터: 재화 + 픽업 스킬 후보 중 하나 / 가디언: 유물 하나 / 보스: 유물 하나 + 픽업 스킬 하나
	const bool bMonster  = CurrentRoomType == ERoomType::MONSTER;
	const bool bGuardian = CurrentRoomType == ERoomType::GUARDIAN;
	const bool bBoss     = CurrentRoomType == ERoomType::BOSS;
	const FIntPoint CurrencyRange = bBoss ? BossRewardCurrency : (bGuardian ? GuardianRewardCurrency : MonsterRewardCurrency);
	const int32 SkillCount = bMonster ? RewardSkillChoices : (bBoss ? BossRewardSkillChoices : 0);
	const int32 RelicCount = (bGuardian || bBoss) ? 1 : 0;

	for (const TWeakObjectPtr<ATerminusPlayerState>& Weak : Players)
	{
		ATerminusPlayerState* PS = Weak.Get();
		ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr;
		if (!PS || !PC) continue;

		FRoomRewardOffer Offer;
		Offer.RoomType = CurrentRoomType;

		// 던전 재화는 바로 지급
		Offer.Currency = FMath::RandRange(FMath::Min(CurrencyRange.X, CurrencyRange.Y), FMath::Max(CurrencyRange.X, CurrencyRange.Y));
		if (Offer.Currency > 0)
		{
			PS->AddCurrency(Offer.Currency);
		}

		if (SkillCount > 0) Offer.SkillOffers = PickRewardSkills(PS, SkillCount);
		if (RelicCount > 0)
		{
			Offer.RelicOffers = PickRewardRelics(PS, RelicCount);
			PS->MarkRelicsSeen(Offer.RelicOffers);   // 안 골라도 이번 런엔 다시 안 나옴
		}

		FPendingReward& Reward = PendingRewards.Add(PS);
		Reward.Offers = Offer.SkillOffers;
		Reward.RelicOffers = Offer.RelicOffers;

		UE_LOG(LogDungeonCombat, Log, TEXT("[Reward] %d번 방(%s) %s: 재화 %d, 스킬 후보 %d개, 유물 후보 %d개"),
			CurrentRoomId, *UEnum::GetValueAsString(CurrentRoomType), *PS->GetPlayerName(), Offer.Currency, Offer.SkillOffers.Num(), Offer.RelicOffers.Num());
		PC->Client_ShowRoomReward(Offer);
	}

	FinishRewardsIfAllDone();   // 받을 사람이 없으면 바로 끝
}

TArray<FName> UDungeonCombatComponent::PickRewardRelics(const ATerminusPlayerState* PS, int32 Count) const
{
	TArray<FName> Pool;
	const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous();
	if (!PS || !Table || Count <= 0) return Pool;

	// 지금 계층 등급 (1~2층 표층, 3~4층 중층, 5~6층 심층)
	const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	const int32 Floor = MapMgr ? MapMgr->CurrentFloor : 1;
	const ERelicTier Tier = Floor >= 5 ? ERelicTier::Deep : (Floor >= 3 ? ERelicTier::Mid : ERelicTier::Surface);

	for (const FName& Row : Table->GetRowNames())
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		if (!Relic || Relic->RelicTier != Tier) continue;

		// 공용이거나 내 직업 것 (몬스터 유물 / 다른 직업 X), 이번 런에 이미 나왔던 건 빼고 (가진 것 포함)
		if (!ATerminusPlayerState::CanClassHoldRelic(PS->GetCharacterClass(), *Relic)) continue;
		if (PS->GetRelics().Contains(Row) || PS->HasSeenRelic(Row)) continue;

		Pool.Add(Row);
	}

	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		Pool.Swap(i, FMath::RandRange(0, i));
	}
	Pool.SetNum(FMath::Min(Pool.Num(), Count));
	return Pool;
}

TArray<FName> UDungeonCombatComponent::PickRewardSkills(const ATerminusPlayerState* PS, int32 Count) const
{
	TArray<FName> Pool;
	const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous();
	if (!PS || !Table) return Pool;

	// 지금 층의 티어 (테마 하나 = 2개 층: 1~2 표층, 3~4 중층, 5~6 심층)
	const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	const int32 Floor = MapMgr ? MapMgr->CurrentFloor : 1;
	const ESkillTier MaxTier = Floor >= 5 ? ESkillTier::Deep : (Floor >= 3 ? ESkillTier::Mid : ESkillTier::Surface);

	const TArray<FName>& Equipped = PS->GetRunState().EnhanceSkills;

	for (const FName& Row : Table->GetRowNames())
	{
		const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
		if (!Skill) continue;

		// 기획: 공용 스킬 + 내 직업 전용 스킬 (기본 / 몬스터 스킬은 안 나옴)
		if (!UTerminusProfileSubsystem::IsEquippableSkill(Row, PS->GetCharacterClass())) continue;

		// 픽업 스킬 = 스킬 에너지를 쓰는 스킬 (사용자 결정)
		if (Skill->SkillEnergyCost <= 0) continue;
		if (bRewardExcludeEventSkills && Skill->SkillCategory == ESkillCategory::Event) continue;
		if (bRewardExcludeHigherTier && Skill->SkillTier > MaxTier) continue;
		if (!bRewardIncludeEquippedSkills && Equipped.Contains(Row)) continue;

		Pool.Add(Row);
	}

	// 섞어서 앞에서부터
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		Pool.Swap(i, FMath::RandRange(0, i));
	}
	Pool.SetNum(FMath::Min(Pool.Num(), FMath::Max(1, Count)));
	return Pool;
}

void UDungeonCombatComponent::HandleRewardFinished(ATerminusPlayerState* PS, FName ChosenSkill, int32 ReplaceSlot, FName ChosenRelic, FName ReplaceRelic)
{
	FPendingReward* Reward = PS ? PendingRewards.Find(PS) : nullptr;
	if (!Reward || Reward->bDone) return;

	// 보여 준 후보 중 하나만 인정
	if (!ChosenSkill.IsNone())
	{
		if (Reward->Offers.Contains(ChosenSkill))
		{
			if (!PS->EquipEnhanceSkill(ChosenSkill, ReplaceSlot))
			{
				UE_LOG(LogDungeonCombat, Warning, TEXT("[Reward] %s: 강화 칸이 꽉 찼는데 바꿀 칸이 없어 '%s' 를 못 넣음"), *PS->GetPlayerName(), *ChosenSkill.ToString());
			}
		}
		else
		{
			UE_LOG(LogDungeonCombat, Warning, TEXT("[Reward] %s: 후보에 없던 스킬 '%s' 요청 무시"), *PS->GetPlayerName(), *ChosenSkill.ToString());
		}
	}

	// 유물: 보여 준 후보만. 칸이 꽉 찼으면 고른 유물(직업 기본 유물 제외)을 버리고 받음
	if (!ChosenRelic.IsNone())
	{
		if (Reward->RelicOffers.Contains(ChosenRelic))
		{
			const FRelicRow* Discard = ReplaceRelic.IsNone() ? nullptr : UTerminusDataSettings::FindRelicRow(ReplaceRelic);
			if (Discard && Discard->RelicTier != ERelicTier::Basic && PS->GetRelics().Contains(ReplaceRelic)
				&& PS->GetRelics().Num() >= PS->GetRelicCapacity())
			{
				PS->RemoveRelic(ReplaceRelic);
			}

			if (!PS->GainRelic(ChosenRelic))
			{
				UE_LOG(LogDungeonCombat, Warning, TEXT("[Reward] %s: 유물 '%s' 를 못 받음 (칸이 꽉 참 등)"), *PS->GetPlayerName(), *ChosenRelic.ToString());
			}
		}
		else
		{
			UE_LOG(LogDungeonCombat, Warning, TEXT("[Reward] %s: 후보에 없던 유물 '%s' 요청 무시"), *PS->GetPlayerName(), *ChosenRelic.ToString());
		}
	}

	Reward->bDone = true;
	FinishRewardsIfAllDone();
}

void UDungeonCombatComponent::FinishRewardsIfAllDone()
{
	for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, FPendingReward>& Pair : PendingRewards)
	{
		// 나간 사람은 기다리지 않음
		if (Pair.Key.IsValid() && !Pair.Value.bDone) return;
	}

	PendingRewards.Reset();
	GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UDungeonCombatComponent::ClearArea, 0.3f, false);
}

// =====================================================================
// 유물
// =====================================================================

int32 UDungeonCombatComponent::AddRelicHolder(UCombatStatsComponent* Stats, ATerminusPlayerState* PS, bool bMonster, const TArray<FName>& Relics)
{
	if (!Stats) return INDEX_NONE;

	const int32 Index = Holders.AddDefaulted();
	FRelicHolder& Holder = Holders[Index];
	Holder.Stats = Stats;
	Holder.Player = PS;
	Holder.bMonster = bMonster;
	for (const FName& Row : Relics)
	{
		if (UTerminusDataSettings::FindRelicRow(Row)) Holder.Relics.Add(Row);
	}

	Stats->OnDamaged.AddUObject(this, &UDungeonCombatComponent::HandleStatsDamaged);
	Stats->OnShieldGained.AddUObject(this, &UDungeonCombatComponent::HandleShieldGained);
	Stats->OnHealed.AddUObject(this, &UDungeonCombatComponent::HandleHealed);
	Stats->OnEnergySpent.AddUObject(this, &UDungeonCombatComponent::HandleEnergySpent);
	Stats->OnSkillEnergySpent.AddUObject(this, &UDungeonCombatComponent::HandleSkillEnergySpent);
	Stats->OnDiedNative.AddUObject(this, &UDungeonCombatComponent::HandleStatsDied);
	Stats->PreventDeath.BindUObject(this, &UDungeonCombatComponent::HandlePreventDeath);
	return Index;
}

void UDungeonCombatComponent::BuildRelicHolders()
{
	ClearRelicHolders();

	auto AddHolder = [this](UCombatStatsComponent* Stats, ATerminusPlayerState* PS, bool bMonster, const TArray<FName>& Relics)
	{
		AddRelicHolder(Stats, PS, bMonster, Relics);
	};

	// 플레이어: 런 보유 유물 (직업 기본 유물 포함)
	for (const TWeakObjectPtr<ATerminusPlayerState>& PS : Players)
	{
		if (ATerminusPlayerState* Player = PS.Get())
		{
			AddHolder(GetStats(Player), Player, false, Player->GetRelics());
		}
	}

	// 몬스터: DT_Monster 의 PassiveID (세미콜론으로 여러 개). 유물 행 이름이 아닌 건 건너뜀
	for (int32 i = 0; i < Monsters.Num(); ++i)
	{
		TArray<FName> Relics;
		if (const FMonsterRow* Row = MonsterRows.IsValidIndex(i) ? UTerminusDataSettings::FindMonsterRow(MonsterRows[i]) : nullptr)
		{
			TArray<FString> Parts;
			Row->PassiveID.ParseIntoArray(Parts, TEXT(";"));
			for (FString& Part : Parts)
			{
				Part.TrimStartAndEndInline();
				if (Part.IsEmpty()) continue;
				if (UTerminusDataSettings::FindRelicRow(FName(*Part)))
				{
					Relics.Add(FName(*Part));
				}
				else
				{
					UE_LOG(LogDungeonCombat, Verbose, TEXT("[Relic] %s 의 PassiveID '%s' 는 유물이 아님 (DT_Relic 에 없음)"), *MonsterRows[i].ToString(), *Part);
				}
			}
		}
		AddHolder(Monsters[i] ? Monsters[i]->GetCombatStats() : nullptr, nullptr, true, Relics);
	}
}

void UDungeonCombatComponent::ClearRelicHolders()
{
	for (const FRelicHolder& Holder : Holders)
	{
		if (UCombatStatsComponent* Stats = Holder.Stats.Get())
		{
			Stats->OnDamaged.RemoveAll(this);
			Stats->OnShieldGained.RemoveAll(this);
			Stats->OnHealed.RemoveAll(this);
			Stats->OnEnergySpent.RemoveAll(this);
			Stats->OnSkillEnergySpent.RemoveAll(this);
			Stats->OnDiedNative.RemoveAll(this);
			Stats->PreventDeath.Unbind();
		}
	}
	Holders.Reset();
	RelicDepth = 0;
}

int32 UDungeonCombatComponent::FindHolder(const UCombatStatsComponent* Stats) const
{
	return Holders.IndexOfByPredicate([Stats](const FRelicHolder& H) { return H.Stats.Get() == Stats; });
}

TArray<UCombatStatsComponent*> UDungeonCombatComponent::GetSideStats(bool bMonsterSide) const
{
	return bMonsterSide ? GetAliveMonsterStats() : GetAlivePlayerStats();
}

void UDungeonCombatComponent::FireRelicsForAll(ERelicTrigger Trigger, bool bPlayers, bool bMonsters)
{
	for (int32 i = 0; i < Holders.Num(); ++i)
	{
		if ((Holders[i].bMonster && bMonsters) || (!Holders[i].bMonster && bPlayers))
		{
			FireRelics(i, Trigger);
		}
	}
}

void UDungeonCombatComponent::FireRelics(int32 HolderIndex, ERelicTrigger Trigger, int32 Amount, FName UsedSkill, const FSkillRow* UsedSkillRow)
{
	if (!Holders.IsValidIndex(HolderIndex)) return;

	// 유물 -> 사건 -> 유물 ... 이 끝없이 이어지지 않게
	if (RelicDepth >= 8)
	{
		UE_LOG(LogDungeonCombat, Warning, TEXT("[Relic] 연쇄 발동이 너무 깊어 멈춤 (%s)"), *UEnum::GetValueAsString(Trigger));
		return;
	}

	const UCombatStatsComponent* Owner = Holders[HolderIndex].Stats.Get();
	if (!Owner || Owner->IsDead()) return;

	const TArray<FName> Relics = Holders[HolderIndex].Relics;   // 실행 중 목록이 바뀌어도 안전하게 복사
	for (const FName& Row : Relics)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		if (!Relic || Relic->TriggerTiming != Trigger) continue;

		// OnSkillUsed: OnSkill 칸이 쓴 스킬의 행 이름이나 SkillID 와 같을 때만
		if (Trigger == ERelicTrigger::OnSkillUsed)
		{
			const bool bMatch = !Relic->OnSkill.IsNone()
				&& (Relic->OnSkill == UsedSkill || (UsedSkillRow && Relic->OnSkill.ToString() == UsedSkillRow->SkillID));
			if (!bMatch) continue;
		}

		UE_LOG(LogDungeonCombat, Log, TEXT("[Relic] %s 발동 (%s)"), *Relic->RelicName.ToString(), *UEnum::GetValueAsString(Trigger));

		++RelicDepth;
		ExecuteRelic(HolderIndex, *Relic, Amount);
		--RelicDepth;
	}
}

void UDungeonCombatComponent::ExecuteRelic(int32 HolderIndex, const FRelicRow& Relic, int32 Amount)
{
	if (!Holders.IsValidIndex(HolderIndex)) return;
	UCombatStatsComponent* Owner = Holders[HolderIndex].Stats.Get();
	ATerminusPlayerState* Player = Holders[HolderIndex].Player.Get();
	const bool bMonster = Holders[HolderIndex].bMonster;
	if (!Owner || Owner->IsDead()) return;

	switch (Relic.ActionKind)
	{
	// 부활은 죽는 순간(HandlePreventDeath)에서
	case EActionKind::RevivalSelfAll:
	case EActionKind::RevivalSelfHalf:
		return;

	// 런 전체에 남는 효과 (플레이어만)
	case EActionKind::GainMoney:
	case EActionKind::MaxHPUp:
	case EActionKind::MaxHPDown:
	case EActionKind::RandomUpgrade:
		if (Player) Player->ApplyRelicMetaEffect(Relic);
		return;

	// 에너지는 최대치를 넘어도 됨 (보조 배터리: 3 -> 4)
	case EActionKind::GainEnergy:
		Owner->AddEnergy(Relic.BaseValue, true);
		return;

	// 아군이 죽으면 풀리는 불굴 (왕의 위엄). 풀어 주는 건 HandleStatsDied
	case EActionKind::IndomitableSelfAllyDead:
		Owner->ApplyStatus(EStatusEffect::Indomitable, Relic.StatusValue, Relic.StatusDuration != 0 ? Relic.StatusDuration : -1);
		return;

	default:
		break;
	}

	// 나머지는 스킬과 같은 실행기로. 유물 칸을 스킬 행 모양으로 옮겨 담음
	FSkillRow AsSkill;
	AsSkill.DisplayName_KR = Relic.RelicName;
	AsSkill.ActionKind = Relic.ActionKind;
	AsSkill.TargetType = Relic.TargetType;
	AsSkill.BaseValue = Relic.BaseValue;
	AsSkill.ScalingStat = Relic.ScalingStat;
	AsSkill.ScalingRatio = Relic.ScalingRatio;
	AsSkill.HitCount = 1;
	AsSkill.StatusEffect = Relic.StatusEffect;
	AsSkill.StatusValue = Relic.StatusValue;
	AsSkill.StatusDuration = Relic.StatusDuration;

	// 대상: 유물 주인 기준 (몬스터 유물이면 적 = 플레이어)
	const TArray<UCombatStatsComponent*> Opponents = GetSideStats(!bMonster);
	const TArray<UCombatStatsComponent*> Allies = GetSideStats(bMonster);
	TArray<UCombatStatsComponent*> Targets;
	switch (Relic.TargetType)
	{
	case ETargetType::Self:        Targets.Add(Owner); break;
	case ETargetType::AllEnemies:  Targets = Opponents; break;
	case ETargetType::AllAllies:   Targets = Allies; break;
	case ETargetType::SingleEnemy: if (Opponents.Num() > 0) Targets.Add(Opponents[FMath::RandRange(0, Opponents.Num() - 1)]); break;
	case ETargetType::SingleAlly:  if (Allies.Num() > 0) Targets.Add(Allies[FMath::RandRange(0, Allies.Num() - 1)]); break;
	}
	if (Targets.Num() == 0) return;

	FSkillExecutor::Execute(AsSkill, Owner, Targets);
}

// ---- 전투 사건 -> 유물

void UDungeonCombatComponent::HandleStatsDamaged(UCombatStatsComponent* Self, int32 HealthLost, UCombatStatsComponent* Instigator)
{
	const int32 Victim = FindHolder(Self);
	if (Victim == INDEX_NONE) return;

	FireRelics(Victim, ERelicTrigger::OnTakeDamage, HealthLost);

	// 때린 쪽 (상대편일 때만 '타격')
	const int32 Attacker = FindHolder(Instigator);
	if (Attacker != INDEX_NONE && Holders[Attacker].bMonster != Holders[Victim].bMonster)
	{
		FireRelics(Attacker, ERelicTrigger::OnHitEnemy, HealthLost);
	}

	// 같은 편 다른 사람
	for (int32 i = 0; i < Holders.Num(); ++i)
	{
		if (i != Victim && Holders[i].bMonster == Holders[Victim].bMonster)
		{
			FireRelics(i, ERelicTrigger::OnAllyTakeDamage, HealthLost);
		}
	}
}

void UDungeonCombatComponent::HandleShieldGained(UCombatStatsComponent* Self, int32 Amount)     { FireRelics(FindHolder(Self), ERelicTrigger::OnGainShield, Amount); }
void UDungeonCombatComponent::HandleHealed(UCombatStatsComponent* Self, int32 Amount)           { FireRelics(FindHolder(Self), ERelicTrigger::OnHealHP, Amount); }
void UDungeonCombatComponent::HandleEnergySpent(UCombatStatsComponent* Self, int32 Amount)      { FireRelics(FindHolder(Self), ERelicTrigger::OnUseEnergy, Amount); }
void UDungeonCombatComponent::HandleSkillEnergySpent(UCombatStatsComponent* Self, int32 Amount) { FireRelics(FindHolder(Self), ERelicTrigger::OnUseSkillEnergy, Amount); }

void UDungeonCombatComponent::HandleStatsDied(UCombatStatsComponent* Self)
{
	const int32 Dead = FindHolder(Self);
	if (Dead == INDEX_NONE) return;

	// 같은 편이 죽으면 '아군 사망 시 해제' 불굴이 풀림 (왕의 위엄)
	for (int32 i = 0; i < Holders.Num(); ++i)
	{
		if (i == Dead || Holders[i].bMonster != Holders[Dead].bMonster) continue;

		UCombatStatsComponent* Ally = Holders[i].Stats.Get();
		if (!Ally || Ally->IsDead()) continue;

		for (const FName& Row : Holders[i].Relics)
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			if (Relic && Relic->ActionKind == EActionKind::IndomitableSelfAllyDead)
			{
				Ally->RemoveStatus(EStatusEffect::Indomitable);
				UE_LOG(LogDungeonCombat, Log, TEXT("[Relic] 아군이 쓰러져 %s 해제"), *Relic->RelicName.ToString());
			}
		}
	}
}

bool UDungeonCombatComponent::HandlePreventDeath(UCombatStatsComponent* Self)
{
	const int32 Index = FindHolder(Self);
	if (Index == INDEX_NONE) return false;

	FRelicHolder& Holder = Holders[Index];
	for (const FName& Row : Holder.Relics)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		if (!Relic || Relic->TriggerTiming != ERelicTrigger::OnDead) continue;
		if (Relic->ActionKind != EActionKind::RevivalSelfAll && Relic->ActionKind != EActionKind::RevivalSelfHalf) continue;

		// 부활 횟수 = BaseValue (Enum_Reference: "BaseValue 만큼 부활")
		int32& Left = Holder.UsesLeft.FindOrAdd(Row, FMath::Max(1, Relic->BaseValue));
		if (Left <= 0) continue;
		--Left;

		Self->Revive(Relic->ActionKind == EActionKind::RevivalSelfAll ? 1.f : 0.5f);
		UE_LOG(LogDungeonCombat, Log, TEXT("[Relic] %s: 부활 (남은 횟수 %d)"), *Relic->RelicName.ToString(), Left);
		return true;
	}
	return false;
}
