#include "Dungeon/DungeonAreaSubsystem.h"

#include "Dungeon/DungeonArea.h"
#include "Game/TerminusSaveSubsystem.h"
#include "Player/TerminusPlayerController.h"
#include "Map/MapManager.h"
#include "Player/TerminusPlayerState.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"

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
	// 아직 진행 중인 구역이 있으면 대기
	// TODO: 먼저 끝난 구역 사람은 여기서 관전 / 구출 / 난입 선택
	for (const TWeakObjectPtr<ADungeonArea>& Other : Areas)
	{
		if (Other.IsValid() && Other->IsInUse() && !Other->IsCleared())
		{
			return;
		}
	}

	FinishAllRooms();
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

	for (ADungeonArea* Area : GetSortedAreas())
	{
		if (!Area->IsInUse()) continue;

		// 지도상 위치를 들어갔던 방으로 진행 (선택도 여기서 비워짐)
		const FRoomNode& Room = Area->GetRoom();
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
