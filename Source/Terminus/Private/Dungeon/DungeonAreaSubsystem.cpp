#include "Dungeon/DungeonAreaSubsystem.h"

#include "Dungeon/DungeonArea.h"
#include "Map/MapManager.h"
#include "Player/TerminusPlayerState.h"

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

bool UDungeonAreaSubsystem::StartSelectedRooms(const TArray<ATerminusPlayerState*>& Players, const TArray<FRoomNode>& MapRooms)
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

	for (int32 i = 0; i < RoomOrder.Num(); ++i)
	{
		const int32 RoomId = RoomOrder[i];

		const FRoomNode* Room = MapRooms.FindByPredicate([RoomId](const FRoomNode& Node) {
			return Node.RoomId == RoomId;
		});

		if (Room)
		{
			SortedAreas[i]->BeginRoom(*Room, PlayersByRoom[RoomId]);
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
}
