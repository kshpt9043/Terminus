#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/SkillTypes.h"
#include "RelicTypes.generated.h"

class UTexture2D;

// 유물 하나를 정의하는 열거형과 행 구조체
// 전부 skill_data.xlsx 의 Relic_DataTable 시트 기준 (열 이름에서 _KR 은 뺌)
// 프로퍼티 이름 = CSV 컬럼 이름, 열거형 값 이름 = 엑셀 값. 하나라도 다르면 임포트 때 에러 없이 기본값이 들어감

// 유물 등급 (노션 유물 페이지)
UENUM(BlueprintType)
enum class ERelicTier : uint8
{
	Basic   UMETA(DisplayName = "기본"),       // 직업 / 몬스터가 처음부터 가진 패시브. 추가 획득 불가 (RLC_<직업>_001)
	Upgrade UMETA(DisplayName = "업그레이드"), // Basic 의 강화판. 얻으면 베이스 Basic 유물은 사라짐
	Surface UMETA(DisplayName = "표층"),       // 1~2층에서 획득 [Common]
	Mid     UMETA(DisplayName = "중층"),       // 3~4층 [Rare]
	Deep    UMETA(DisplayName = "심층")        // 5~6층 [Epic]
};

// 유물 효과가 발동하는 시점 (Enum_TriggerTiming 시트)
UENUM(BlueprintType)
enum class ERelicTrigger : uint8
{
	OnBattleStart,          // 전투 시작 (1번)
	OnTurnStart,            // 자기 턴 시작. 에너지가 최대치로 회복된 뒤
	OnHitEnemy,             // 적의 체력을 깎았을 때
	OnTakeDamage,           // 내 체력이 깎였을 때
	OnGainShield,           // 보호막이 늘었을 때마다
	OnSkillUsed,            // OnSkill 에 적힌 스킬을 쓴 직후
	OnAllyTakeDamage,       // 아군 체력이 깎였을 때
	OnBattleEnd,            // 전투 종료 (1번)
	OnTurnEnd,              // 자기 턴 끝
	OnDead,                 // 사망 시 (BaseValue 만큼)
	OnUseEnergy,            // 에너지를 쓸 때마다
	OnUseSkillEnergy,       // 스킬 에너지를 쓸 때마다
	OnGainRelic,            // 이 유물을 얻었을 때 (팔아도 효과 유지)
	OnRestStart,            // 휴식터에 들어갈 때
	OnGuardianBattleStart,  // 가디언 전투 시작
	OnBossBattleStart,      // 보스 전투 시작
	OnHealHP                // 내 체력이 회복될 때마다
};

/**
 * 유물 한 개의 정의. Relic_DataTable 시트 한 행과 1:1. 행 이름 = RLC_Common_001 같은 코드
 *
 * 효과는 스킬과 같은 구조 (ActionKind / TargetType / BaseValue / Status...) + 발동 시점(TriggerTiming)
 * 가격 -1: 구매가면 상점에서 살 수 없음, 판매가면 팔 수 없음. 판매가 0 은 0원에 팔 수 있음 (의도)
 */
USTRUCT(BlueprintType)
struct FRelicRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText RelicName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText RelicDesc;

	// 아이콘. 엑셀에 없는 열이라 에디터에서 직접 지정 (CSV 다시 임포트해도 Icon 열이 비어 있으면 지워짐 주의)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	// --- 분류
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ERelicTier RelicTier = ERelicTier::Surface;

	// 이 유물을 가질 수 있는 쪽 (Shared = 누구나, 직업 = 그 직업만, Monster = 몬스터 패시브)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESkillOwner OwnerClass = ESkillOwner::Shared;

	// --- 효과
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ERelicTrigger TriggerTiming = ERelicTrigger::OnBattleStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EActionKind ActionKind = EActionKind::HitEnemy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETargetType TargetType = ETargetType::Self;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EScalingStat ScalingStat = EScalingStat::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ScalingRatio = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EStatusEffect StatusEffect = EStatusEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StatusValue = 0;

	// 지속 사이클 수. -1 = 전투 끝까지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StatusDuration = 0;

	// TriggerTiming 이 OnSkillUsed 일 때 대상 스킬 (DT_Skill 행 이름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName OnSkill;

	// --- 가격
	// 상점 구매가 (던전 재화). -1 = 상점에서 살 수 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BuyPrice_Dungeon = -1;

	// 상점 판매가 (던전 재화). -1 = 팔 수 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SellPrice_Dungeon = -1;

	// 정산 판매 기본가 (골드). 세력별로 여기에 -5%~+5% 가 붙음. -1 = 팔 수 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SellPrice_Gold = -1;

	// 상점에 나올 수 있는가 (Basic / Upgrade 는 가격과 상관없이 안 나옴)
	bool CanBuyInShop() const
	{
		return BuyPrice_Dungeon >= 0 && RelicTier != ERelicTier::Basic && RelicTier != ERelicTier::Upgrade;
	}

	bool CanSellInShop() const { return SellPrice_Dungeon >= 0; }
	bool CanSellAtSettlement() const { return SellPrice_Gold >= 0; }
};
