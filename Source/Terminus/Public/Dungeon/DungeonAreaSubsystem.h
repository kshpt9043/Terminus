#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DungeonAreaSubsystem.generated.h"

class ADungeonArea;
class ATerminusPlayerState;
struct FRoomNode;

/**
 * 던전 구역들을 관리. 레벨에 따로 놓을 필요 없음 (월드마다 자동 생성)
 *
 * - 레벨의 ADungeonArea 들이 BeginPlay 에서 스스로 등록
 * - 전원 선택이 끝나면 같은 방을 고른 사람끼리 구역 하나에 배정 (StartSelectedRooms)
 * - 모든 구역이 끝나야 지도로 돌아가서 다음 선택 (NotifyAreaCleared)
 *
 * 배정 / 진행은 서버에서만. 클라에서는 등록 목록 조회용으로만 씀
 */
UCLASS()
class TERMINUS_API UDungeonAreaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterArea(ADungeonArea* Area);
	void UnregisterArea(ADungeonArea* Area);

	// 레벨에 구역이 하나라도 있는가
	bool HasAreas() const;

	// 구역에서 방이 진행 중인가. 진행 중엔 지도에서 새 방을 고를 수 없음
	bool IsAnyRoomInProgress() const;

	// [서버] 각 플레이어가 고른 방(SelectedRoomId)으로 입장시킨다. 같은 방 = 같은 구역
	// 구역이 없거나 모자라면 아무것도 안 하고 false -> 부른 쪽이 대체 처리
	bool StartSelectedRooms(const TArray<ATerminusPlayerState*>& Players, const TArray<FRoomNode>& MapRooms);

	// [서버] 구역 하나가 끝났을 때 구역이 부름. 전 구역이 끝났으면 전원 진행 + 지도로 복귀
	void NotifyAreaCleared(ADungeonArea* Area);

private:
	// 번호순 정렬된 구역 목록
	TArray<ADungeonArea*> GetSortedAreas() const;

	// 모든 구역을 닫고 각자 들어갔던 방으로 지도상 위치를 진행시킴
	void FinishAllRooms();

	TArray<TWeakObjectPtr<ADungeonArea>> Areas;
};
