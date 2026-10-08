#include "Dungeon/DungeonAreaSubsystem.h"

#include "Dungeon/DungeonArea.h"
#include "Game/TerminusSaveSubsystem.h"
#include "Player/TerminusPlayerController.h"
#include "Map/MapManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TerminusPlayerState.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "TimerManager.h"

void UDungeonAreaSubsystem::RegisterArea(ADungeonArea* Area)
{
	if (!Area) return;

	Areas.AddUnique(Area);

	// 같은 번호가 둘이면 배정 순서가 뒤죽박죽이 됨 -> 레벨 배치 실수 알림
	for (const TWeakObjectPtr<ADungeonArea>& Other : Areas)
	{
		if (Other.IsValid() && Other.Get() != Area && Other->AreaIndex == Area->AreaIndex)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AreaSubsystem] AreaIndex %d 가 겹침: %s / %s"),
				Area->AreaIndex, *Area->GetName(), *Other->GetName());
		}
	}
}

void UDungeonAreaSubsystem::UnregisterArea(ADungeonArea* Area)
{
	Areas.Remove(Area);
}

bool UDungeonAreaSubsystem::HasAreas() const
{
	for (const TWeakObjectPtr<ADungeonArea>& Area : Areas)
	{
		if (Area.IsValid()) return true;
	}
	return false;
}

bool UDungeonAreaSubsystem::IsAnyRoomInProgress() const
{
	for (const TWeakObjectPtr<ADungeonArea>& Area : Areas)
	{
		if (Area.IsValid() && Area->IsInUse()) return true;
	}
	return false;
}

TArray<ADungeonArea*> UDungeonAreaSubsystem::GetActiveAreas() const
{
	TArray<ADungeonArea*> Result = GetSortedAreas();
	Result.RemoveAll([](const ADungeonArea* Area) { return !Area->IsInUse(); });
	return Result;
}

TArray<ADungeonArea*> UDungeonAreaSubsystem::GetSortedAreas() const
{
	TArray<ADungeonArea*> Result;
	for (const TWeakObjectPtr<ADungeonArea>& Area : Areas)
	{
		if (Area.IsValid()) Result.Add(Area.Get());
	}

	Result.Sort([](const ADungeonArea& A, const ADungeonArea& B)
	{
		return A.AreaIndex < B.AreaIndex;
	});
	return Result;
}

bool UDungeonAreaSubsystem::StartSelectedRooms(const TArray<ATerminusPlayerState*>& Players, const TArray<FRoomNode>& MapRooms, const UDungeonThemeData* Theme)
{
	if (IsAnyRoomInProgress())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaSubsystem] 이미 진행 중인 방이 있어서 새로 시작 안 함"));
		return false;
	}

	// 같은 방을 고른 사람끼리 묶기. 순서는 PlayerArray 순 그대로 -> 슬롯도 그 순서로 채워짐
	TArray<int32> RoomOrder;
	TMap<int32, TArray<ATerminusPlayerState*>> PlayersByRoom;

	for (ATerminusPlayerState* PS : Players)
	{
		if (!PS || PS->GetSelectedRoomId() == -1) continue;

		const int32 RoomId = PS->GetSelectedRoomId();
		if (!PlayersByRoom.Contains(RoomId))
		{
			RoomOrder.Add(RoomId);
		}
		PlayersByRoom.FindOrAdd(RoomId).Add(PS);
	}

	if (RoomOrder.Num() == 0) return false;

	const TArray<ADungeonArea*> SortedAreas = GetSortedAreas();
	if (SortedAreas.Num() < RoomOrder.Num())
	{
		// 일부만 배정하면 나머지 사람이 붕 뜸 -> 전부 아니면 아예 안 함
		UE_LOG(LogTemp, Warning, TEXT("[AreaSubsystem] 구역 부족. 방 %d개 필요, 레벨에 구역 %d개"),
			RoomOrder.Num(), SortedAreas.Num());
		return false;
	}

	// 누가 나가서 이번 방이 무효가 되면 여기로 되돌림
	TakeRoomStartSnapshots(Players);

	for (int32 i = 0; i < RoomOrder.Num(); ++i)
	{
		const int32 RoomId = RoomOrder[i];

		const FRoomNode* Room = MapRooms.FindByPredicate([RoomId](const FRoomNode& Node) {
			return Node.RoomId == RoomId;
		});

		if (Room)
		{
			SortedAreas[i]->BeginRoom(*Room, PlayersByRoom[RoomId], Theme);
		}
	}

	return true;
}

void UDungeonAreaSubsystem::NotifyAreaCleared(ADungeonArea* Area)
{
	EvaluateAreas();
}

void UDungeonAreaSubsystem::NotifyAreaWiped(ADungeonArea* Area)
{
	EvaluateAreas();
}

void UDungeonAreaSubsystem::SplitAreas(TArray<ADungeonArea*>& OutSurvivors, TArray<ADungeonArea*>& OutWiped, bool& bOutStillFighting) const
{
	bOutStillFighting = false;
	for (ADungeonArea* Area : GetSortedAreas())
	{
		if (!Area->IsInUse()) continue;

		if (Area->IsWiped())        OutWiped.Add(Area);
		else if (Area->IsCleared()) OutSurvivors.Add(Area);
		else                        bOutStillFighting = true;   // 싸우는 중 / 보상 / 휴식 / 이벤트 고르는 중
	}
}

void UDungeonAreaSubsystem::EvaluateAreas()
{
	if (bRescueVoting) return;

	TArray<ADungeonArea*> Survivors, Wiped;
	bool bStillFighting = false;
	SplitAreas(Survivors, Wiped, bStillFighting);

	// 아직 진행 중인 구역이 있으면 대기 (그 구역도 끝나야 구출 / 난입을 고를 수 있음)
	// TODO: 먼저 끝난 구역 사람은 관전
	if (bStillFighting) return;

	if (Wiped.Num() == 0)
	{
		FinishAllRooms();
		return;
	}

	if (Survivors.Num() > 0)
	{
		StartRescueVote(Survivors, Wiped);
		return;
	}

	// 다른 방에 아무도 없음 -> 전멸. 런 끝 (세이브 삭제 + 사망 정산)
	int32 PlayerCount = 0;
	for (ADungeonArea* Area : Wiped) PlayerCount += Area->GetOccupants().Num();

	UE_LOG(LogTemp, Log, TEXT("[AreaSubsystem] 전멸. 런 끝"));
	if (AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass())))
	{
		MapMgr->EndRunByDeath(PlayerCount > 1 ? TEXT("파티가 전멸했습니다.") : TEXT("쓰러졌습니다."));
	}
}

void UDungeonAreaSubsystem::StartRescueVote(const TArray<ADungeonArea*>& SurvivorAreas, const TArray<ADungeonArea*>& WipedAreas)
{
	bRescueVoting = true;
	bRescueCanIntervene = WipedAreas.Num() == 1;   // 전멸한 방이 둘 이상이면 구출만 (한 번에 한 방에만 들어갈 수 있어서)
	RescueVoters.Reset();
	RescueVotes.Reset();

	TArray<FString> WipedNames;
	for (ADungeonArea* Area : WipedAreas)
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (PS) WipedNames.Add(PS->GetPlayerName());
		}
	}

	for (ADungeonArea* Area : SurvivorAreas)
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (!PS) continue;
			RescueVoters.Add(PS);
			if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(PS->GetOwner()))
			{
				PC->Client_ShowRescue(true, WipedNames, bRescueCanIntervene, RescueHealthCost, RescueVoteSeconds);
			}
		}
	}

	for (ADungeonArea* Area : WipedAreas)
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
			{
				PC->Client_ShowRescue(false, WipedNames, bRescueCanIntervene, RescueHealthCost, RescueVoteSeconds);
			}
		}
	}

	FChatMessage Notice;
	Notice.Kind = EChatMessageKind::System;
	Notice.Text = FString::Printf(TEXT("%s 님이 쓰러졌습니다. 다른 방의 동료가 구출 / 난입을 고릅니다."), *FString::Join(WipedNames, TEXT(", ")));
	ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);

	GetWorld()->GetTimerManager().SetTimer(RescueTimer, this, &UDungeonAreaSubsystem::ResolveRescueVote, RescueVoteSeconds, false);
}

void UDungeonAreaSubsystem::HandleRescueChoice(ATerminusPlayerState* Voter, bool bIntervene)
{
	if (!bRescueVoting || !Voter || !RescueVoters.Contains(Voter) || RescueVotes.Contains(Voter)) return;
	if (bIntervene && !bRescueCanIntervene) return;

	RescueVotes.Add(Voter, bIntervene);

	for (const TWeakObjectPtr<ATerminusPlayerState>& Weak : RescueVoters)
	{
		if (Weak.IsValid() && !RescueVotes.Contains(Weak)) return;
	}
	ResolveRescueVote();
}

void UDungeonAreaSubsystem::CancelRescueVote()
{
	if (!bRescueVoting) return;

	bRescueVoting = false;
	GetWorld()->GetTimerManager().ClearTimer(RescueTimer);
	RescueVoters.Reset();
	RescueVotes.Reset();

	for (ADungeonArea* Area : GetSortedAreas())
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
			{
				PC->Client_CloseRescue();
			}
		}
	}
}

void UDungeonAreaSubsystem::ResolveRescueVote()
{
	if (!bRescueVoting) return;

	// 다수결, 동점이면 랜덤, 아무도 안 골랐으면 구출
	int32 Rescue = 0, Intervene = 0;
	for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, bool>& Pair : RescueVotes)
	{
		(Pair.Value ? Intervene : Rescue) += 1;
	}
	const bool bIntervene = bRescueCanIntervene && (Intervene > Rescue || (Intervene == Rescue && Intervene > 0 && FMath::RandBool()));

	CancelRescueVote();   // 투표 화면 닫기

	TArray<ADungeonArea*> Survivors, Wiped;
	bool bStillFighting = false;
	SplitAreas(Survivors, Wiped, bStillFighting);
	if (Survivors.Num() == 0 || Wiped.Num() == 0)
	{
		EvaluateAreas();
		return;
	}

	FChatMessage Notice;
	Notice.Kind = EChatMessageKind::System;
	Notice.Text = bIntervene
		? FString::Printf(TEXT("난입! 전멸한 방에 들어가 남은 몬스터와 싸웁니다. (구출 %d / 난입 %d)"), Rescue, Intervene)
		: FString::Printf(TEXT("구출! 쓰러진 동료를 데리고 다음 방으로 갑니다. (구출 %d / 난입 %d)"), Rescue, Intervene);
	ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);

	if (bIntervene) ApplyIntervention(Survivors, Wiped[0]);
	else            ApplyRescue(Survivors, Wiped);
}

void UDungeonAreaSubsystem::ApplyRescue(const TArray<ADungeonArea*>& SurvivorAreas, const TArray<ADungeonArea*>& WipedAreas)
{
	// 살리러 간 방 (전멸한 방). 구출한 사람도 그 방에 같이 있는 걸로 침 -> 다음 선택은 그 방에서
	const FRoomNode TargetRoom = WipedAreas[0]->GetRoom();

	// 구출하는 사람: 현재 체력의 RescueHealthCost 만큼 잃음 (1 아래로는 안 내려감)
	for (ADungeonArea* Area : SurvivorAreas)
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (!PS) continue;

			const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
			UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
			if (Stats && !Stats->IsDead())
			{
				const int32 MaxHealth = FMath::Max(1, Stats->GetStats().MaxHealth);
				const int32 Health = Stats->GetCombatState().Health;
				const int32 NewHealth = FMath::Max(1, Health - FMath::RoundToInt(Health * RescueHealthCost));
				Stats->Revive(static_cast<float>(NewHealth) / MaxHealth);
			}
			PS->AdvanceToRoom(TargetRoom.RoomId, TargetRoom.Row);
		}
		Area->Release();
	}

	// 전멸한 사람: 체력 1. 보상 없음
	for (ADungeonArea* Area : WipedAreas)
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (!PS) continue;

			const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
			if (UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr)
			{
				Stats->Revive(0.f);   // 최소 1
			}
			PS->AdvanceToRoom(TargetRoom.RoomId, TargetRoom.Row);
		}
		Area->Release();
	}

	// 열린 구역이 없으니 지도 복귀 + 자동 저장만
	FinishAllRooms();
}

void UDungeonAreaSubsystem::ApplyIntervention(const TArray<ADungeonArea*>& SurvivorAreas, ADungeonArea* WipedArea)
{
	// 살아남은 사람들을 자기 구역에서 빼서 (지도 자리로 돌아온 뒤) 전멸한 구역으로
	TArray<ATerminusPlayerState*> Joiners;
	for (ADungeonArea* Area : SurvivorAreas)
	{
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (PS) Joiners.Add(PS);
		}
		Area->Release();
	}

	// 이기면 그 구역이 클리어 -> 전원 그 방(살리러 간 방)으로 진행. 지면 다시 전멸 -> 아무도 없으니 런 끝
	WipedArea->BeginIntervention(Joiners);
}

void UDungeonAreaSubsystem::HandlePlayerLeft(ATerminusPlayerState* Leaver)
{
	bool bAnyInUse = false;
	bool bAllFightsOver = true;
	for (ADungeonArea* Area : GetSortedAreas())
	{
		if (!Area->IsInUse()) continue;
		bAnyInUse = true;
		bAllFightsOver &= Area->IsFightOver();
	}

	if (!bAnyInUse) return;

	// 구출 / 난입 투표 중이었으면 닫음 (아래에서 방을 무효로 돌림)
	CancelRescueVote();

	if (bAllFightsOver)
	{
		// 이미 이긴 방: 결과 인정. 나간 사람도 같이 진행 (보상 화면은 닫히고 안 고른 스킬은 건너뜀)
		UE_LOG(LogTemp, Log, TEXT("[AreaSubsystem] %s 나감. 모든 방이 끝난 뒤라 결과를 인정하고 마무리"), Leaver ? *Leaver->GetPlayerName() : TEXT("?"));
		FinishAllRooms();
		return;
	}

	// 싸우는 중: 이번 방은 무효. 모든 구역을 닫고 방에 들어가기 전으로 (나간 사람 포함)
	UE_LOG(LogTemp, Log, TEXT("[AreaSubsystem] %s 나감. 진행 중인 방을 무효로 하고 들어가기 전으로 되돌림"), Leaver ? *Leaver->GetPlayerName() : TEXT("?"));

	for (ADungeonArea* Area : GetSortedAreas())
	{
		if (Area->IsInUse())
		{
			Area->Release();
		}
	}

	RollbackToRoomStart();
}

void UDungeonAreaSubsystem::AbortAllRooms()
{
	CancelRescueVote();

	for (ADungeonArea* Area : GetSortedAreas())
	{
		if (!Area->IsInUse()) continue;

		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (PS) PS->SetSelectedRoomId(-1);
		}
		Area->Release();
	}
	RoomStartSnapshots.Reset();
}

void UDungeonAreaSubsystem::TakeRoomStartSnapshots(const TArray<ATerminusPlayerState*>& Players)
{
	RoomStartSnapshots.Reset();

	for (ATerminusPlayerState* PS : Players)
	{
		if (!PS) continue;

		FRoomStartSnapshot& Snapshot = RoomStartSnapshots.Add(PS);
		Snapshot.RunState = PS->GetRunState();

		const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
		if (const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr)
		{
			Snapshot.BattlerStats = Stats->GetStats();
			Snapshot.Health = Stats->GetCombatState().Health;
		}
		else
		{
			Snapshot.BattlerStats = Snapshot.RunState.Stats;
		}
	}
}

void UDungeonAreaSubsystem::RollbackToRoomStart()
{
	for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, FRoomStartSnapshot>& Pair : RoomStartSnapshots)
	{
		ATerminusPlayerState* PS = Pair.Key.Get();
		if (!PS) continue;

		const FRoomStartSnapshot& Snapshot = Pair.Value;

		// 런 상태 (재화 / 유물 / 강화 스킬 / 최대 체력 / 지도 위치). 고른 방은 비움
		FRunState Restored = Snapshot.RunState;
		Restored.SelectedRoomId = -1;
		PS->SetRunState(Restored);

		// 배틀러: 스텟 / 체력 / 에너지 / 상태 효과 모두 들어가기 전으로
		const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
		if (UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr)
		{
			Stats->InitFrom(Snapshot.BattlerStats);
			const int32 MaxHealth = Snapshot.BattlerStats.MaxHealth;
			if (Snapshot.Health > 0 && MaxHealth > 0 && Snapshot.Health < MaxHealth)
			{
				Stats->Revive(static_cast<float>(Snapshot.Health) / MaxHealth);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[AreaSubsystem] %s 롤백 (체력 %d)"), *PS->GetPlayerName(), Snapshot.Health);
	}

	RoomStartSnapshots.Reset();
}

void UDungeonAreaSubsystem::FinishAllRooms()
{
	UE_LOG(LogTemp, Log, TEXT("[AreaSubsystem] 모든 구역 종료. 지도로 복귀"));

	bool bBossCleared = false;
	for (ADungeonArea* Area : GetSortedAreas())
	{
		if (!Area->IsInUse()) continue;

		// 지도상 위치를 들어갔던 방으로 진행 (선택도 여기서 비워짐)
		const FRoomNode& Room = Area->GetRoom();
		bBossCleared |= Room.Type == ERoomType::BOSS;
		for (ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (PS)
			{
				PS->AdvanceToRoom(Room.RoomId, Room.Row);
			}
		}

		Area->Release();
	}

	// 방이 정상으로 끝났으니 롤백할 일 없음
	RoomStartSnapshots.Reset();

	// 보스방이면 층 진행 / 행선지 투표. 저장은 새 층에 도착했을 때 (투표 중엔 저장 안 함 -> 끄면 보스 직전부터)
	if (bBossCleared)
	{
		if (AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass())))
		{
			MapMgr->HandleBossCleared();
			return;
		}
	}

	// 자동 저장: 방이 끝나고 전원이 지도로 돌아온 지금 (보상까지 받은 뒤)
	if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		if (Save->SaveCurrentRun(GetWorld()))
		{
			FChatMessage Notice;
			Notice.Kind = EChatMessageKind::System;
			Notice.Text = TEXT("진행 상황을 자동 저장했습니다.");
			ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
		}
	}
}
