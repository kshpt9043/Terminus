#include "Dungeon/DungeonThemeData.h"

#include "Dungeon/DungeonAreaSet.h"

namespace
{
	TSubclassOf<ADungeonAreaSet> PickValid(const TArray<TSubclassOf<ADungeonAreaSet>>& Candidates)
	{
		// 비워 둔 칸(None)은 건너뜀
		TArray<TSubclassOf<ADungeonAreaSet>> Valid;
		for (const TSubclassOf<ADungeonAreaSet>& Set : Candidates)
		{
			if (Set) Valid.Add(Set);
		}

		return Valid.Num() > 0 ? Valid[FMath::RandRange(0, Valid.Num() - 1)] : nullptr;
	}
}

TSubclassOf<ADungeonAreaSet> UDungeonThemeData::PickAreaSet(ERoomType RoomType) const
{
	if (const FDungeonAreaSetList* TypeSets = RoomTypeAreaSets.Find(RoomType))
	{
		if (TSubclassOf<ADungeonAreaSet> Picked = PickValid(TypeSets->Sets))
		{
			return Picked;
		}
	}

	return PickValid(AreaSets);
}
