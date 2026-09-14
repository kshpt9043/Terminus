#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SkillTypes.generated.h"

// 스킬 한 개를 정의하는 열거형들과 행 구조체
// 전부 skill_data_template.xlsx 기준이고, 열거형 값 이름과 순서도 그 파일 그대로 맞춤
// 값 이름 바꾸면 나중에 CSV 임포트할 때 그 행이 조용히 틀어짐

UENUM(BlueprintType)
enum class ESkillOwner : uint8
{
	Monk     UMETA(DisplayName = "무도가"),
	Engineer UMETA(DisplayName = "마도 공학자"),
	Paladin  UMETA(DisplayName = "성기사"),
	Assassin UMETA(DisplayName = "암살자"),

	// 공용 스킬. ECharacterClass 에 끼우면 MAX 반복문 도는 주점 버튼까지 같이 늘어서 따로 뺌
	Shared   UMETA(DisplayName = "공용")
};

// 스킬을 어떻게 획득하는지
UENUM(BlueprintType)
enum class ESkillCategory : uint8
{
	Basic    UMETA(DisplayName = "기본"),
	Common   UMETA(DisplayName = "공용"),
	Personal UMETA(DisplayName = "개인")
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
	Poison      UMETA(DisplayName = "중독"),
	Mark        UMETA(DisplayName = "급소 지정"),
	EvasionBuff UMETA(DisplayName = "회피 증가"),
	Counter     UMETA(DisplayName = "반격"),

	// 상태효과는 아닌데 엑셀이 여기 넣어둠. 반동 피해는 SelfDamage 컬럼이 따로 있어서 값이 두 군데 들어감
	SelfDamage  UMETA(DisplayName = "반동 피해")
};

// 실제 효과를 실행하는 로직 종류
// 20개인데 6개는 위쪽 값들을 두 개 섞은 것뿐이라, 나중에 스위치는 원자 14개만 구현하면 됨
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

	// 엑셀 Enum_Reference 시트에는 EvasionSelf 로 적혀 있는데, 실제 데이터 행은 EvasionBuff 를 씀
	// EStatusEffect::EvasionBuff 와 이름을 맞춘 것으로 보여서 데이터 행 쪽을 따름
	EvasionBuff,          // 자신 회피 증가. 1사이클만 유지

	PoisonAll,            // 적 전체 중독만 부여
	EvasionBuffPoisonAll, // 조합 -> EvasionBuff + PoisonAll
	GainEnergy            // 즉시 행동 에너지 회복
};

/**
 * 스킬 한 개의 정의. skill_data_template.xlsx 의 Skill_DataTable 시트 한 행과 1:1.
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
