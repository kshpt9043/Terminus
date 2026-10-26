#pragma once

#include "CoreMinimal.h"
#include "Map/MapManager.h"
#include "Data/EventQuestTypes.h"
#include "RewardTypes.generated.h"

// 이벤트 선택지 하나 (DT_EventChoice 한 행을 이 사람 기준으로 풀어 둔 것). 서버가 만들어 보내고 화면은 그대로 보여줌
// 무엇을 줄지(스킬 / 유물 / 재화)는 서버가 미리 정해 둠 -> 카드에 그 스킬 / 유물이 보이고, 고르면 그대로 받음
USTRUCT(BlueprintType)
struct FEventOption
{
	GENERATED_BODY()

	// DT_EventChoice 행 이름
	UPROPERTY(BlueprintReadOnly) FName ChoiceRow;
	UPROPERTY(BlueprintReadOnly) EEventOutcomeType Outcome = EEventOutcomeType::Leave;

	// 버튼 문구 / 대가와 결과 안내 (ChoiceTitle_KR / ChoiceTooltip_KR)
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Description;

	// 받을 스킬 / 유물 (미리 정해 둔 것). 없으면 None
	UPROPERTY(BlueprintReadOnly) FName SkillRow;
	UPROPERTY(BlueprintReadOnly) FName RelicRow;

	// 재화 / 회복 수치 (범위는 미리 굴려 둠)
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;

	// 대가 (툴팁 대괄호 안 문구 기준. 문구가 없으면 CostType / CostValue)
	UPROPERTY(BlueprintReadOnly) int32 CostHealth = 0;
	UPROPERTY(BlueprintReadOnly) int32 CostMaxHealth = 0;
	UPROPERTY(BlueprintReadOnly) int32 CostCurrency = 0;
	UPROPERTY(BlueprintReadOnly) int32 CostSkills = 0;     // 장착 픽업 스킬 무작위로 잃음
	UPROPERTY(BlueprintReadOnly) int32 CostRelics = 0;     // 유물 무작위로 잃음 (유물 변화면 바뀌는 그 유물)
	UPROPERTY(BlueprintReadOnly) FName CostRelic;          // 대가로 받는 디버프 유물 (GainDebuffRelic)

	// 툴팁에만 적힌 추가 결과 ("체력을 8 회복하고 ... +1강 강화", "승리 시 용암 지대 유물 및 던전 재화 55 획득")
	UPROPERTY(BlueprintReadOnly) int32 BonusHeal = 0;
	UPROPERTY(BlueprintReadOnly) int32 BonusUpgrades = 0;
	UPROPERTY(BlueprintReadOnly) int32 BonusWinCurrency = 0;     // 쉬는 가디언을 이기면
	UPROPERTY(BlueprintReadOnly) bool bBonusWinRelic = false;    // 쉬는 가디언을 이기면 테마 유물

	// 고를 수 있나. 못 고르면 이유 ("던전 재화가 부족합니다")
	UPROPERTY(BlueprintReadOnly) bool bAvailable = true;
	UPROPERTY(BlueprintReadOnly) FText UnavailableReason;

	// 스킬을 받는 선택지 (강화 칸이 꽉 찼으면 바꿀 칸부터 고름)
	bool GivesSkill() const { return !SkillRow.IsNone(); }

	// 유물 칸을 하나 차지하는 선택지 (꽉 찼으면 버릴 유물부터 고름)
	bool GivesRelic() const
	{
		return !RelicRow.IsNone() && Outcome != EEventOutcomeType::SwapRelicSameTierRandom && Outcome != EEventOutcomeType::SwapRelicHigherTierRandom;
	}
};

/**
 * 상점 (서버 -> 플레이어 한 명). 사용자 결정 2026-10-09: 진열은 아직 미정이라 도감(데이터 전체)에서 아무거나
 *  - 유물 구매 (DT 상점 구매가) / 픽업 스킬 구매 (임시 고정가) / 장착 픽업 스킬 강화 / 보유 유물 판매 (DT 상점 판매가)
 * 진열은 사람마다 따로. 사고 나면 Bought 가 true
 */
USTRUCT(BlueprintType)
struct FShopState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TArray<FName> RelicOffers;
	UPROPERTY(BlueprintReadOnly) TArray<int32> RelicPrices;
	UPROPERTY(BlueprintReadOnly) TArray<bool> RelicBought;

	UPROPERTY(BlueprintReadOnly) TArray<FName> SkillOffers;
	UPROPERTY(BlueprintReadOnly) TArray<int32> SkillPrices;
	UPROPERTY(BlueprintReadOnly) TArray<bool> SkillBought;

	// 픽업 스킬 강화 비용 = UpgradeCostPerLevel x (다음 단계)
	UPROPERTY(BlueprintReadOnly) int32 UpgradeCostPerLevel = 20;

	// 마지막 행동 결과 ("구매: 은빛 방패" / "던전 재화가 부족합니다")
	UPROPERTY(BlueprintReadOnly) FText Message;
};

/**
 * 방 클리어 보상 (서버 -> 플레이어 한 명). 사용자 결정 2026-10-06
 *  - 몬스터방: 던전 재화 + 픽업 스킬(스킬 에너지를 쓰는 스킬) 후보 중 하나
 *  - 가디언방: 유물 하나 (공용이거나 내 직업, 지금 계층 등급)
 *  - 보스방: 유물 하나 (같은 조건) + 픽업 스킬 하나
 * 스킬 / 유물은 각각 받거나 안 받을 수 있음. 칸이 꽉 차면 바꿀 칸을 고름
 */
USTRUCT(BlueprintType)
struct FRoomRewardOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ERoomType RoomType = ERoomType::MONSTER;

	// 이미 받은 던전 재화 (보여주기만)
	UPROPERTY(BlueprintReadOnly)
	int32 Currency = 0;

	// 픽업 스킬 후보 (하나만 고름)
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> SkillOffers;

	// 유물 후보 (하나만 고름)
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> RelicOffers;
};
