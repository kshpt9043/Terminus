// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonArea.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Player/TerminusPlayerController.h"

void ATerminusPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATerminusPlayerState, RunState);
	DOREPLIFETIME(ATerminusPlayerState, bReady);
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
	RunState.Currency = 0;

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

const FSkillRow* ATerminusPlayerState::GetCombatSkill(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < NumBasicSkills)
	{
		const TArray<const FSkillRow*> Basic = UTerminusDataSettings::FindBasicSkills(RunState.CharacterClass);
		return Basic.IsValidIndex(SlotIndex) ? Basic[SlotIndex] : nullptr;
	}

	const int32 EnhanceIndex = SlotIndex - NumBasicSkills;
	if (EnhanceIndex >= 0 && EnhanceIndex < NumEnhanceSkills && RunState.EnhanceSkills.IsValidIndex(EnhanceIndex))
	{
		return UTerminusDataSettings::FindSkillRow(RunState.EnhanceSkills[EnhanceIndex]);
	}

	return nullptr;
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
