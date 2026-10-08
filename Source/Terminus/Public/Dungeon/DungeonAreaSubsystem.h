#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RunTypes.h"
#include "Engine/TimerHandle.h"
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

	// 지금 방이 열려 있는 구역들 (번호순). 클라에서도 됨 (관전 바)
	TArray<ADungeonArea*> GetActiveAreas() const;

	// [서버] 각 플레이어가 고른 방(SelectedRoomId)으로 입장시킨다. 같은 방 = 같은 구역
	// Theme 은 이 층의 테마. 구역마다 이 테마의 무대를 띄움
	// 구역이 없거나 모자라면 아무것도 안 하고 false -> 부른 쪽이 대체 처리
	bool StartSelectedRooms(const TArray<ATerminusPlayerState*>& Players, const TArray<FRoomNode>& MapRooms, const UDungeonThemeData* Theme);

	// [서버] 구역 하나가 끝났을 때 구역이 부름. 전 구역이 끝났으면 전원 진행 + 지도로 복귀
	void NotifyAreaCleared(ADungeonArea* Area);

	// [서버] 구역 하나가 전멸했을 때 구역이 부름. 기획 사망 순서도 (사용자 결정 10-08: 다이어그램대로)
	//  - 다른 구역이 아직 싸우는 중이면 기다림
	//  - 다른 구역에 살아남은 사람이 있으면: 그 사람들이 구출 / 난입 투표
	//      구출: 구출하는 사람 체력 -RescueHealthCost(현재 체력 비율), 전멸한 사람은 체력 1 (보상 없음)
	//      난입: 전멸한 방에 들어가 남은 몬스터와 이어서 싸움 (몬스터 체력 유지, 이기면 보상 + 쓰러진 사람 체력 1)
	//      둘 다 살리러 간 방에 같이 있는 걸로 침 -> 전원 지도 위치가 전멸한 방, 다음 선택도 그 방에서 (사용자 결정 10-08)
	//  - 아무도 없으면 (싱글 사망 / 멀티 전멸): 런 끝 -> 사망 정산 (유물만 골드로, 강화 스킬 / 던전 재화 소멸)
	void NotifyAreaWiped(ADungeonArea* Area);

	// [서버] 배신 전투를 첫 번째 구역에서 시작. 구역이 없거나 방이 진행 중이면 false
	bool StartBetrayal(const TArray<ATerminusPlayerState*>& Players, ATerminusPlayerState* Betrayer, const UDungeonThemeData* Theme, float VictimHealthCut, int32 StatPerOpponent);

	// 배신 전투가 진행 중인가 (누가 나가도 이공간으로 가지 않음: 끝나면 런이 끝남)
	bool IsBetrayalInProgress() const;

	// [서버] 구출 / 난입 투표 (PC 의 Server_ChooseRescue). 살아남은 사람만, 한 번
	void HandleRescueChoice(ATerminusPlayerState* Voter, bool bIntervene);

	// 구출 비용 (구출하는 사람의 현재 체력 비율). 사용자 결정 10-08: 40%
	static constexpr float RescueHealthCost = 0.4f;

	// 구출 / 난입 투표 시간 (초). 안 고른 사람은 기권, 아무도 안 고르면 구출
	static constexpr float RescueVoteSeconds = 20.f;

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

	// 구역 상태를 보고 다음 단계 (전원 진행 / 구출·난입 투표 / 런 끝)
	void EvaluateAreas();

	// 구출 / 난입 투표 (서버만)
	bool bRescueVoting = false;
	bool bRescueCanIntervene = false;
	TArray<TWeakObjectPtr<ATerminusPlayerState>> RescueVoters;
	TMap<TWeakObjectPtr<ATerminusPlayerState>, bool> RescueVotes;
	FTimerHandle RescueTimer;

	void StartRescueVote(const TArray<ADungeonArea*>& SurvivorAreas, const TArray<ADungeonArea*>& WipedAreas);
	void ResolveRescueVote();
	void CancelRescueVote();
	void ApplyRescue(const TArray<ADungeonArea*>& SurvivorAreas, const TArray<ADungeonArea*>& WipedAreas);
	void ApplyIntervention(const TArray<ADungeonArea*>& SurvivorAreas, ADungeonArea* WipedArea);
	void SplitAreas(TArray<ADungeonArea*>& OutSurvivors, TArray<ADungeonArea*>& OutWiped, bool& bOutStillFighting) const;

	TArray<TWeakObjectPtr<ADungeonArea>> Areas;
};
