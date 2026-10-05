// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonArea.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Player/TerminusPlayerController.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"

void ATerminusPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATerminusPlayerState, RunState);
	DOREPLIFETIME(ATerminusPlayerState, bReady);
	DOREPLIFETIME(ATerminusPlayerState, bClassLocked);
	DOREPLIFETIME(ATerminusPlayerState, CurrentArea);
}

void ATerminusPlayerState::SetCurrentArea(ADungeonArea* InArea)
{
	if (!HasAuthority() || CurrentArea == InArea)
	{
		return;
	}

	CurrentArea = InArea;

	// 리슨 서버 호스트는 OnRep 이 안 불려서 직접
	OnRep_CurrentArea();
}

void ATerminusPlayerState::OnRep_CurrentArea()
{
	// PS 의 Owner 는 그 플레이어의 PC. 클라에선 자기 PC 만 존재하니 남의 PS 면 여기서 걸러짐
	ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(GetOwner());
	if (PC && PC->IsLocalController())
	{
		PC->ViewDungeonArea(CurrentArea);
	}
}

void ATerminusPlayerState::SetCharacterClass(ECharacterClass InClass)
{
	if (!HasAuthority())
	{
		return;
	}
	
	RunState.CharacterClass = InClass;
}

void ATerminusPlayerState::LockClassFromSave(ECharacterClass InClass)
{
	if (!HasAuthority()) return;

	RunState.CharacterClass = InClass;
	bClassLocked = true;
	OnRep_RunState();
}

int32 ATerminusPlayerState::ConsumeSavedHealth()
{
	if (!HasAuthority() || RunState.SavedHealth < 0) return -1;

	const int32 Health = RunState.SavedHealth;
	RunState.SavedHealth = -1;
	return Health;
}

void ATerminusPlayerState::SetReady(bool bInReady)
{
	if (!HasAuthority())
	{
		return;
	}
	
	bReady = bInReady;
}

void ATerminusPlayerState::BeginRun()
{
	if (!HasAuthority())
	{
		return;
	}
	
	// 강화 전 기본 스텟에서 시작. 던전에서 늘어나는 값은 이후 여기(RunState.Stats)에 쌓임
	if (const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(RunState.CharacterClass))
	{
		RunState.Stats = Row->BaseStats;
		RunState.bStatsInitialized = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PS: %s 행이 없어 런 스텟을 못 채움"),
			*UEnum::GetValueAsString(RunState.CharacterClass));
	}
	
	RunState.CurrentRoomId = -1;
	RunState.CurrentMapLevel = 0;
	RunState.SelectedRoomId = -1;
	RunState.VisitedRoomIds.Reset();
	RunState.EnhanceSkills.Reset();
	RunState.bStartSkillChosen = false;
	RunState.bStartRelicsChosen = false;
	RunState.Currency = 0;
	RunState.SavedHealth = -1;

	// 직업 기본 유물 = 패시브. 데이터에 아직 없는 직업은 빈 채로
	RunState.Relics.Reset();
	const FName BasicRelic = GetBasicRelicRow(RunState.CharacterClass);
	if (!BasicRelic.IsNone())
	{
		RunState.Relics.Add(BasicRelic);
	}

	OnRep_RunState();
}

void ATerminusPlayerState::ChooseStartSkill(FName SkillRow)
{
	if (!HasAuthority() || RunState.bStartSkillChosen)
	{
		return;
	}

	// 보유 여부는 각자 컴퓨터의 세이브라 서버가 확인할 수 없음 -> 장착 가능한 스킬인지만 봄
	if (!SkillRow.IsNone())
	{
		if (UTerminusProfileSubsystem::IsEquippableSkill(SkillRow, RunState.CharacterClass))
		{
			RunState.EnhanceSkills.Reset();
			RunState.EnhanceSkills.Add(SkillRow);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PS: %s 가 장착할 수 없는 '%s' 를 골라 장착 없이 넘어감"),
				*GetPlayerName(), *SkillRow.ToString());
		}
	}

	RunState.bStartSkillChosen = true;
	OnRep_RunState();
}

void ATerminusPlayerState::ResetStartSkill()
{
	if (!HasAuthority()) return;

	RunState.EnhanceSkills.Reset();
	RunState.bStartSkillChosen = false;
	OnRep_RunState();
}

void ATerminusPlayerState::ChooseStartRelics(const TArray<FName>& RelicRows)
{
	if (!HasAuthority() || RunState.bStartRelicsChosen) return;

	// 보유 여부는 각자 컴퓨터의 창고라 서버가 확인할 수 없음 -> 보관 가능한 유물인지 / 이 직업이 가질 수 있는지만 봄
	int32 Taken = 0;
	for (const FName& Row : RelicRows)
	{
		if (Taken >= MaxStartRelics) break;
		if (UTerminusProfileSubsystem::IsStorableRelic(Row) && GainRelic(Row))
		{
			++Taken;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Relic] %s 시작 유물 %d개 장착"), *GetPlayerName(), Taken);

	RunState.bStartRelicsChosen = true;
	OnRep_RunState();
}

void ATerminusPlayerState::SetHardMode(bool bInHardMode)
{
	if (!HasAuthority() || RunState.bHardMode == bInHardMode) return;

	RunState.bHardMode = bInHardMode;
	OnRep_RunState();
}

void ATerminusPlayerState::ResetStartRelics()
{
	if (!HasAuthority()) return;

	RunState.bStartRelicsChosen = false;
	OnRep_RunState();
}

bool ATerminusPlayerState::CanClassHoldRelic(ECharacterClass InClass, const FRelicRow& Relic)
{
	if (Relic.OwnerClass == ESkillOwner::Shared) return true;

	const FString ClassName = StaticEnum<ECharacterClass>()->GetNameStringByValue(static_cast<int64>(InClass));
	const FString OwnerName = StaticEnum<ESkillOwner>()->GetNameStringByValue(static_cast<int64>(Relic.OwnerClass));
	return OwnerName == ClassName;
}

const FSkillRow* ATerminusPlayerState::GetCombatSkill(int32 SlotIndex) const
{
	const FName Row = GetCombatSkillRow(SlotIndex);
	return Row.IsNone() ? nullptr : UTerminusDataSettings::FindSkillRow(Row);
}

int32 ATerminusPlayerState::GetEnhanceSlotCount() const
{
	// 런 스텟이 아직 없으면(던전 맵 바로 PIE) 기본 3칸
	const int32 Slots = RunState.bStatsInitialized ? RunState.Stats.EnhanceSlots : NumEnhanceSkills;
	return FMath::Clamp(Slots, 0, NumEnhanceSkills);
}

bool ATerminusPlayerState::EquipEnhanceSkill(FName SkillRow, int32 ReplaceIndex)
{
	if (!HasAuthority() || !UTerminusDataSettings::FindSkillRow(SkillRow)) return false;

	if (RunState.EnhanceSkills.Num() < GetEnhanceSlotCount())
	{
		RunState.EnhanceSkills.Add(SkillRow);
	}
	else if (RunState.EnhanceSkills.IsValidIndex(ReplaceIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[Skill] %s: 강화 칸 %d %s -> %s"), *GetPlayerName(), ReplaceIndex,
			*RunState.EnhanceSkills[ReplaceIndex].ToString(), *SkillRow.ToString());
		RunState.EnhanceSkills[ReplaceIndex] = SkillRow;
	}
	else
	{
		return false;
	}

	OnRep_RunState();
	return true;
}

FName ATerminusPlayerState::GetCombatSkillRow(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < NumBasicSkills)
	{
		// 기본 스킬은 직업으로 찾은 행을 다시 행 이름으로 (FindBasicSkills 와 같은 순서: 공격 방어 특수)
		const TArray<const FSkillRow*> Basic = UTerminusDataSettings::FindBasicSkills(RunState.CharacterClass);
		const FSkillRow* Wanted = Basic.IsValidIndex(SlotIndex) ? Basic[SlotIndex] : nullptr;
		const UDataTable* Table = Wanted ? UTerminusDataSettings::Get()->SkillTable.LoadSynchronous() : nullptr;
		if (Table)
		{
			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				if (reinterpret_cast<const FSkillRow*>(Pair.Value) == Wanted) return Pair.Key;
			}
		}
		return NAME_None;
	}

	const int32 EnhanceIndex = SlotIndex - NumBasicSkills;
	if (EnhanceIndex >= 0 && EnhanceIndex < NumEnhanceSkills && RunState.EnhanceSkills.IsValidIndex(EnhanceIndex))
	{
		return RunState.EnhanceSkills[EnhanceIndex];
	}

	return NAME_None;
}

// =====================================================================
// 유물
// =====================================================================

FName ATerminusPlayerState::GetBasicRelicRow(ECharacterClass InClass)
{
	const FString ClassName = StaticEnum<ECharacterClass>()->GetNameStringByValue(static_cast<int64>(InClass));
	const FName Row(*FString::Printf(TEXT("RLC_%s_001"), *ClassName));
	return UTerminusDataSettings::FindRelicRow(Row) ? Row : NAME_None;
}

bool ATerminusPlayerState::GainRelic(FName RelicRow)
{
	if (!HasAuthority()) return false;

	const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(RelicRow);
	if (!Relic)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] '%s' 는 DT_Relic 에 없음"), *RelicRow.ToString());
		return false;
	}

	// 가질 수 있는 유물인가: 공용이거나 내 직업 것
	if (!CanClassHoldRelic(RunState.CharacterClass, *Relic))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] %s 는 다른 직업 / 몬스터 전용이라 가질 수 없음"), *RelicRow.ToString());
		return false;
	}

	if (RunState.Relics.Contains(RelicRow)) return false;

	if (RunState.Stats.MaxRelics > 0 && RunState.Relics.Num() >= RunState.Stats.MaxRelics)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] 유물 칸이 가득 참 (%d)"), RunState.Stats.MaxRelics);
		return false;
	}

	// 업그레이드: 베이스가 되는 같은 직업의 Basic 유물은 사라짐 (노션 유물 페이지)
	if (Relic->RelicTier == ERelicTier::Upgrade)
	{
		RunState.Relics.RemoveAll([Relic](const FName& Owned)
		{
			const FRelicRow* Other = UTerminusDataSettings::FindRelicRow(Owned);
			return Other && Other->RelicTier == ERelicTier::Basic && Other->OwnerClass == Relic->OwnerClass;
		});
	}

	RunState.Relics.Add(RelicRow);
	UE_LOG(LogTemp, Log, TEXT("[Relic] %s 획득: %s"), *GetPlayerName(), *Relic->RelicName.ToString());

	// 얻는 순간 효과 (팔아도 유지되는 것들)
	if (Relic->TriggerTiming == ERelicTrigger::OnGainRelic)
	{
		ApplyRelicMetaEffect(*Relic);
	}

	OnRep_RunState();
	return true;
}

bool ATerminusPlayerState::RemoveRelic(FName RelicRow)
{
	if (!HasAuthority() || RunState.Relics.Remove(RelicRow) == 0) return false;

	OnRep_RunState();
	return true;
}

void ATerminusPlayerState::ApplyRelicMetaEffect(const FRelicRow& Relic)
{
	if (!HasAuthority()) return;

	ATerminusBattler* Battler = Cast<ATerminusBattler>(GetPawn());
	UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;

	switch (Relic.ActionKind)
	{
	case EActionKind::GainMoney:
		AddCurrency(Relic.BaseValue);
		break;

	case EActionKind::MaxHPUp:
	case EActionKind::MaxHPDown:
	{
		// 런 스텟(다음 레벨에도 유지)과 지금 배틀러 둘 다
		const int32 Delta = Relic.ActionKind == EActionKind::MaxHPUp ? Relic.BaseValue : -Relic.BaseValue;
		RunState.Stats.MaxHealth = FMath::Max(1, RunState.Stats.MaxHealth + Delta);
		if (Stats) Stats->ModifyMaxHealth(Delta);
		OnRep_RunState();
		break;
	}

	case EActionKind::HealSelf:
		if (Stats) Stats->Heal(Relic.BaseValue);
		break;

	case EActionKind::RandomUpgrade:
		// TODO: 픽업 스킬 강화 단계 데이터가 생기면 (스킬 강화 기획: 최대 +3)
		UE_LOG(LogTemp, Warning, TEXT("[Relic] %s: 픽업 스킬 강화는 아직 없어 효과 없음"), *Relic.RelicName.ToString());
		break;

	default:
		break;
	}
}

void ATerminusPlayerState::AddCurrency(int32 Amount)
{
	if (!HasAuthority() || Amount == 0) return;

	RunState.Currency = FMath::Max(0, RunState.Currency + Amount);
	OnRep_RunState();
}

void ATerminusPlayerState::AdvanceToRoom(int32 TargetRoomId, int32 TargetRow)
{
	if (HasAuthority())
	{
		// 1. 선택했던 다음 방 ID 초기화
		RunState.SelectedRoomId = -1;

		// 2. 현재 방 위치 및 진행 레벨(Row + 1) 업데이트
		RunState.CurrentRoomId = TargetRoomId;
		RunState.CurrentMapLevel = TargetRow + 1; // 방을 클리어했으므로 다음 레벨 진입 가능하게 함
		RunState.VisitedRoomIds.Add(TargetRoomId);   // 지도에 지나온 길 표시용

		// 3. 변경 사항 동기화 전파
		OnRep_RunState();
	}
}

void ATerminusPlayerState::CopyProperties(APlayerState* NewPS)
{
	Super::CopyProperties(NewPS);
	
	if (ATerminusPlayerState* New = Cast<ATerminusPlayerState>(NewPS))
	{
		New->RunState = RunState;
		
		UE_LOG(LogTemp, Log, TEXT("PS: %s 의 %s 를 넘김"),
			*GetPlayerName(),
			*UEnum::GetValueAsString(RunState.CharacterClass));
	}
}

void ATerminusPlayerState::SetSelectedRoomId(int32 InRoomId)
{
	// 오직 서버에서만 실행
	if (HasAuthority())
	{
		RunState.SelectedRoomId = InRoomId;

		// 리슨 서버(Listen Server, 호스트 플레이어) 자신도 UI 및 델리게이트를 즉시 갱신하기 위함
		OnRep_RunState();
	}
}

void ATerminusPlayerState::SetCurrentRoomInfo(int32 InRoomId, int32 InMapLevel)
{
	if (HasAuthority())
	{
		RunState.CurrentRoomId = InRoomId;
		RunState.CurrentMapLevel = InMapLevel;
		OnRep_RunState();
	}
}

void ATerminusPlayerState::SetRunState(const FRunState& InRunState)
{
	if (HasAuthority())
	{
		RunState = InRunState;
		OnRep_RunState();
	}
}

void ATerminusPlayerState::OnRep_RunState()
{
	OnRunStateChanged.Broadcast(RunState);
}
