// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Data/TerminusDataSettings.h"

void ATerminusPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ATerminusPlayerState, RunState);
	DOREPLIFETIME(ATerminusPlayerState, bReady);
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
	
	OnRep_RunState();
}

void ATerminusPlayerState::Test_ClearAndMoveToRoom(int32 TargetRoomId, int32 TargetRow)
{
	if (HasAuthority())
	{
		// 1. 선택했던 다음 방 ID 초기화
		RunState.SelectedRoomId = -1;

		// 2. 현재 방 위치 및 진행 레벨(Row + 1) 업데이트
		RunState.CurrentRoomId = TargetRoomId;
		RunState.CurrentMapLevel = TargetRow + 1; // 방을 클리어했으므로 다음 레벨 진입 가능하게 함

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
