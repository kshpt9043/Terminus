#pragma once

#include "CoreMinimal.h"
#include "Data/EventQuestTypes.h"
#include "Map/MapManager.h"
#include "Data/RunTypes.h"

class ATerminusPlayerState;
class AMapManager;
class UWorld;

/**
 * 퀘스트 규칙 (10-24 기획 엑셀 DT_Quest / DT_QuestRoom + 사용자 결정 2026-10-26)
 *  - 테마가 시작되는 층(1 / 3 / 5층)의 퀘스트 방에서 3개 중 1개 수락 (1개는 내 직업 세력 것), 거절 가능
 *  - 멀티는 사람마다 따로. 같은 방에서 잡은 몬스터는 그 방 사람 모두에게 셈
 *  - 그 테마(2개 층) 동안만 유효. 다음 테마로 넘어가면 못 깬 퀘스트는 사라짐
 *  - 보상 GiveCurrency: 대상 DungeonGold = 던전 재화 즉시, Gold = 정산 때 골드 (데이터가 둘을 나눠 적음)
 *  - 특수 방(bInjectSpecialRoom): 지도의 Depth 줄 방 하나가 특수 방으로 바뀌고, 들어가면 SpawnMonster 가 나옴
 *
 * 상태는 플레이어 RunState(ActiveQuest / 층 방 입장 수 / QuestGold)에 있어서 세이브와 복제가 같이 됨. 판정은 서버만
 */
namespace TerminusQuests
{
	// 퀘스트 방 후보: 지금 테마 이름 또는 계층 이름(표층 / 중층 / 심층) 퀘스트 중 가중치로 Count 개, 1개는 내 세력
	TERMINUS_API TArray<FName> MakeOffers(const ATerminusPlayerState* PS, int32 Floor, const FString& ThemeName, int32 Count = 3);

	// "던전 재화 200", "골드 400 (정산 때)", "유물 [슬라임 킹의 왕관]", "픽업 스킬 2회 강화"
	TERMINUS_API FString DescribeReward(EQuestRewardType Type, FName Target, int32 Value);

	// 카드 설명: 설명 + 세력 + 보상 (+ 수락 보상)
	TERMINUS_API FString DescribeOffer(FName QuestRow);

	// 지도 화면 한 줄: "[퀘스트] 슬라임 학살자 0 / 1". 퀘스트가 없으면 빈 문자열
	TERMINUS_API FString DescribeActive(const FRunState& RunState);

	// 수락 (수락 보상, 지금 층이면 특수 방 넣기, 이미 이룬 조건이면 바로 완료)
	TERMINUS_API void Accept(ATerminusPlayerState* PS, FName QuestRow);

	// 다음 테마로 넘어감: 못 깬 퀘스트는 사라짐
	TERMINUS_API void ExpireForNewTheme(ATerminusPlayerState* PS);

	// ---- 진행 사건 (서버)
	TERMINUS_API void OnMonstersKilled(ATerminusPlayerState* PS, const TArray<FName>& MonsterRows);
	TERMINUS_API void OnRoomEntered(ATerminusPlayerState* PS, ERoomType Type);
	TERMINUS_API void OnRelicGained(ATerminusPlayerState* PS);
	TERMINUS_API void OnCurrencyEarned(ATerminusPlayerState* PS, int32 Amount);
	TERMINUS_API void OnFloorReached(ATerminusPlayerState* PS, int32 Floor);
	TERMINUS_API void OnSpecialRoomCleared(ATerminusPlayerState* PS, FName SpecialRoom);

	// 진행 중 퀘스트들의 특수 방을 지금 층 지도에 넣음 (수락했을 때 / 새 층 지도를 만든 뒤). 바뀐 게 있으면 true
	TERMINUS_API bool InjectSpecialRooms(UWorld* World);

	// 특수 방에서 나올 몬스터 (DT_Monster 행 이름, SpawnValue 마리)
	TERMINUS_API TArray<FName> GetSpecialRoomMonsters(FName SpecialRoom);
}
