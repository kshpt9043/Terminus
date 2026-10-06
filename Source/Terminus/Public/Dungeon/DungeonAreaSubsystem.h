#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RunTypes.h"
#include "DungeonAreaSubsystem.generated.h"

class ADungeonArea;
class ATerminusPlayerState;
class UDungeonThemeData;
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
	// Theme 은 이 층의 테마. 구역마다 이 테마의 무대를 띄움
	// 구역이 없거나 모자라면 아무것도 안 하고 false -> 부른 쪽이 대체 처리
	bool StartSelectedRooms(const TArray<ATerminusPlayerState*>& Players, const TArray<FRoomNode>& MapRooms, const UDungeonThemeData* Theme);

	// [서버] 구역 하나가 끝났을 때 구역이 부름. 전 구역이 끝났으면 전원 진행 + 지도로 복귀
	void NotifyAreaCleared(ADungeonArea* Area);

	// [서버] 누가 게임을 나갔을 때 (멀티). 끝나면 모든 구역이 닫혀 있음 (전원 이공간으로 가기 전 정리)
	//  - 아직 싸우는 구역이 있으면: 이번 방은 무효 (롤백). 전원 방에 들어가기 직전 상태로 되돌림
	//  - 모든 구역이 이미 이겼으면(보상 중): 결과는 인정하고 평소처럼 진행. 아직 안 고른 보상은 건너뜀
	void HandlePlayerLeft(ATerminusPlayerState* Leaver);

	// [서버 / 테스트] 진행 중인 방을 전부 닫음. 지도 위치 그대로, 고른 방만 비움 (보상 / 진행 없음)
	void AbortAllRooms();

private:
	// 방에 들어가기 직전 상태 (롤백용). 방이 정상으로 끝나면 비움
	struct FRoomStartSnapshot
	{
		FRunState RunState;
		FCharacterStats BattlerStats;   // 배틀러가 실제로 쓰던 스텟 (런 스텟이 없으면 클래스 기본값)
		int32 Health = -1;
	};
	TMap<TWeakObjectPtr<ATerminusPlayerState>, FRoomStartSnapshot> RoomStartSnapshots;

	void TakeRoomStartSnapshots(const TArray<ATerminusPlayerState*>& Players);
	void RollbackToRoomStart();

	// 번호순 정렬된 구역 목록
	TArray<ADungeonArea*> GetSortedAreas() const;

	// 모든 구역을 닫고 각자 들어갔던 방으로 지도상 위치를 진행시킴
	void FinishAllRooms();

	TArray<TWeakObjectPtr<ADungeonArea>> Areas;
};
