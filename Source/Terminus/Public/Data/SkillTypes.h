#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SkillTypes.generated.h"

// 스킬 한 개를 정의하는 열거형들과 행 구조체
// 전부 skill_data.xlsx 기준이고, 열거형 값 이름과 순서도 그 파일 그대로 맞춤
// 값 이름 바꾸면 나중에 CSV 임포트할 때 그 행이 조용히 틀어짐

UENUM(BlueprintType)
enum class ESkillOwner : uint8
{
	Fighter  UMETA(DisplayName = "무도가"),
	Engineer UMETA(DisplayName = "마도 공학자"),
	Paladin  UMETA(DisplayName = "성기사"),
	Assassin UMETA(DisplayName = "암살자"),

	// 공용 스킬. ECharacterClass 에 끼우면 MAX 반복문 도는 주점 버튼까지 같이 늘어서 따로 뺌
	Shared   UMETA(DisplayName = "공용"),
	// 몬스터 스킬
	Monster  UMETA(DisplayName = "몬스터")
};

// 스킬을 어떻게 획득하는지
UENUM(BlueprintType)
enum class ESkillCategory : uint8
{
	Basic    UMETA(DisplayName = "기본"),
	Common   UMETA(DisplayName = "공용"),
	Personal UMETA(DisplayName = "개인"),
	Event    UMETA(DisplayName = "이벤트")
};

// 주점 UI 의 공격/방어/특수 세 칸에 대응
UENUM(BlueprintType)
enum class ESkillType : uint8
{
	Attack  UMETA(DisplayName = "공격"),
	Defense UMETA(DisplayName = "방어"),
	Special UMETA(DisplayName = "특수")
};

// 획득 가능한 층. 별 색깔과 1:1 -> 표층 회색, 중층 노랑, 심층 빨강
UENUM(BlueprintType)
enum class ESkillTier : uint8
{
	Surface UMETA(DisplayName = "표층"),
	Mid     UMETA(DisplayName = "중층"),
	Deep    UMETA(DisplayName = "심층")
};

UENUM(BlueprintType)
enum class ETargetType : uint8
{
	Self        UMETA(DisplayName = "자신"),
	SingleEnemy UMETA(DisplayName = "적 1명"),
	AllEnemies  UMETA(DisplayName = "적 전체"),
	SingleAlly  UMETA(DisplayName = "아군 1명"),
	AllAllies   UMETA(DisplayName = "아군 전체")
};

// BaseValue 에 더해줄 스텟. 엑셀 순서가 ATK, DEF, None 이라 None 이 마지막
// 그래서 FSkillRow 쪽에서 기본값을 None 으로 직접 박아둠
UENUM(BlueprintType)
enum class EScalingStat : uint8
{
	ATK UMETA(DisplayName = "공격"),
	DEF UMETA(DisplayName = "방어"),
	None
};

UENUM(BlueprintType)
enum class EStatusEffect : uint8
{
	None,
	Counter,         // State_01  반격
	Mark,            // State_02  급소 지정
	Evasion,         // State_03  민첩
	Poison,          // State_04  중독
	Brave,           // State_05  용기
	Protection,      // State_06  가호
	Ice,             // State_07  얼음
	Lava,            // State_08  용암
	Metal,           // State_09  강철
	Acid,            // State_10  산성
	Fear,            // State_11  공포
	Fusion,          // State_12  퓨전
	Vitality,        // State_13  활력
	Reflux,          // State_14  역류
	InternalInjury,  // State_15  내상
	Immortality,     // State_16  불사
	Absorption       // State_17  흡수
};

// 실제 효과를 실행하는 로직 종류. 엑셀 Enum_Reference 시트 그대로 44개
//
// 44갈래 스위치 짜면 안 됨. 상태 종류는 StatusEffect 컬럼이, 대상은 TargetType 이 이미 말해줌
// HitAllEnemies~All 여섯 개가 코드가 전부 같은 이유임 -> 피해 주고 StatusEffect 대로 거는 것뿐
// 실제 원자는 피해 보호막 회복 상태부여 반동 에너지 여섯 개고
// 나중에 이걸 원자 플래그로 번역하는 함수 하나 끼워서 실행부는 여섯 개만 보게 할 것
UENUM(BlueprintType)
enum class EActionKind : uint8
{
	HitEnemy,             // 적 1명 단일 피해
	HitEnemyMulti,        // 적 1명 다단 피해. HitCount 만큼 반복
	HitEnemySingleAoe,    // 조합 -> HitEnemy + HitAllEnemies. 광역분은 SecondaryValue 로 계산
	HitAllEnemies,        // 적 전체 동일 피해
	StatusMark,           // 급소 지정만 부여
	StatusPoison,         // 적 1명 중독만 부여
	HitEnemyMark,         // 조합 -> HitEnemy + StatusMark
	HitEnemyPoison,       // 조합 -> HitEnemy + StatusPoison
	DmgSelf,              // 시전자 반동 피해만
	HitEnemySelfDmg,      // 조합 -> HitEnemy + DmgSelf
	ShieldSelf,           // 자신 보호막
	CounterSelf,          // 자신 반격 부여만
	ShieldSelfCounter,    // 조합 -> ShieldSelf + CounterSelf
	ShieldAll,            // 아군 전체 보호막
	HealAlly,             // 아군 1명 회복
	HealAll,              // 아군 전체 회복
	EvasionSelf,          // 자신 회피 증가. 1사이클만 유지
	StatusPoisonAll,      // 적 전체 중독만 부여
	EvasionSelfPoisonAll, // 조합 -> EvasionSelf + StatusPoisonAll
	GainEnergy,           // 즉시 행동 에너지 회복
	BraveSelf,            // 자신 용기 부여만. 공격 오름
	BraveAll,             // 아군 전체 용기 부여
	HitEnemyProtect,      // 조합 -> HitEnemy + ProtectSelf
	ProtectSelf,          // 자신 가호 부여만. 방어 오름
	HitEnemyShield,       // 조합 -> HitEnemy + ShieldSelf. 보호막은 SecondaryValue 로 계산
	StatusLava,           // 적 1명 용암만 부여. 방어 깎임
	StatusLavaAll,        // 적 전체 용암만 부여
	StatusIce,            // 적 1명 얼음만 부여. 공격 깎임
	StatusIceAll,         // 적 전체 얼음만 부여
	ShieldSelfPoisonAll,  // 조합 -> ShieldSelf + StatusPoisonAll. 독 연막이 회피에서 보호막으로 바뀜
	MetalSelf,            // 자신 강철 부여. 한 사이클 받는 피해가 1로 고정
	HitAllEnemiesLavaAll, // 조합 -> HitAllEnemies + StatusLavaAll
	HitAllEnemiesIceAll,  // 조합 -> HitAllEnemies + StatusIceAll
	HitAllEnemiesPoisonAll, // 조합 -> HitAllEnemies + StatusPoisonAll
	StatusAcid,           // 적 1명 산성만 부여. 에너지 소모량 1 늘어남
	StatusAcidAll,        // 적 전체 산성만 부여
	HitAllEnemiesAcidAll, // 조합 -> HitAllEnemies + StatusAcidAll
	StatusFear,           // 적 1명 공포만 부여. 주는 피해 25% 깎임
	StatusFearAll,        // 적 전체 공포만 부여
	HitAllEnemiesFearAll, // 조합 -> HitAllEnemies + StatusFearAll
	StatusFusion,         // 적 1명 퓨전만 부여. 중독 용암 얼음 중 랜덤
	StatusFusionAll,      // 적 전체 퓨전만 부여
	HitAllEnemiesFusionAll, // 조합 -> HitAllEnemies + StatusFusionAll
	HealAllBraveAll       // 조합 -> HealAll + BraveAll
};

/**
 * 스킬 한 개의 정의. skill_data.xlsx 의 Skill_DataTable 시트 한 행과 1:1.
 *
 * 컬럼은 26개인데 프로퍼티는 25개다. RowName 은 구조체 필드가 아니라
 * DataTable 이 행을 담는 TMap 의 키라서 여기 넣으면 이름이 두 군데로 갈린다.
 *
 * 프로퍼티 이름 = CSV 컬럼 이름. 하나라도 다르면 그 열은 에러 없이 버려진다.
 */
USTRUCT(BlueprintType)
struct FSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- 식별

	// 기획서 추적용 코드. 몬스터 테이블이 이 값으로 스킬을 참조함
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString SkillID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName_KR;

	// --- 분류

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESkillOwner OwnerClass = ESkillOwner::Shared;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESkillCategory SkillCategory = ESkillCategory::Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESkillType SkillType = ESkillType::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESkillTier SkillTier = ESkillTier::Surface;

	// --- 실행

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EActionKind ActionKind = EActionKind::HitEnemy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETargetType TargetType = ETargetType::SingleEnemy;

	// --- 비용

	// 매 턴 최대치까지 회복되는 기본 에너지. 기본 스킬은 보통 1
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 EnergyCost = 1;

	// 던전에서 주운 스킬만 쓰는 자원. 기본 스킬은 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SkillEnergyCost = 0;

	// --- 수치

	// 스텟 보정 전 기본 수치. 피해든 보호막이든 회복이든 다 여기서 시작
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseValue = 0;

	// 엑셀 순서상 None 이 마지막이라 기본값을 직접 지정해야 빈 행이 공격 보정을 안 받음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EScalingStat ScalingStat = EScalingStat::None;

	// 최종값 = BaseValue + 스텟 * ScalingRatio. 지금은 전부 1
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ScalingRatio = 1.f;

	// 타수. 1이면 단타
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 HitCount = 1;

	// 조합형이 쓰는 두 번째 수치. 마도 캐논의 광역 피해 계수 같은 것. 0이면 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SecondaryValue = 0;

	// --- 상태 효과

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EStatusEffect StatusEffect = EStatusEffect::None;

	// 중독 스택 수 같은 상태효과 자체의 수치
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StatusValue = 0;

	// 지속 사이클 수. 턴이 아니라 사이클 기준
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StatusDuration = 0;

	// 시전자가 대가로 받는 피해
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SelfDamage = 0;

	// --- 연출
	// 엑셀이 에셋 경로가 아니라 태그를 담고 있음 -> 나중에 TMap 으로 조회할 거라 FName
	// 실제 에셋 경로가 확정되면 TSoftObjectPtr 로 바꾸면 됨

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName IconID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AnimMontageTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName VFXTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SFXTag;

	// --- 문서

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Description_KR;

	// 기획 의도랑 확정 안 된 것들. 런타임에선 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FString Notes;
};
