// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerState.h"

#include "Net/UnrealNetwork.h"

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
