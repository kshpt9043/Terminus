#pragma once

#include "CoreMinimal.h"
#include "Data/SkillTypes.h"
#include "PartyCommsTypes.generated.h"

class ATerminusPlayerState;
class UCombatStatsComponent;

// 파티 소통 도구 (채팅 말고): 행동 계획 공유 / 핑 / 퀵챗 / 재촉
//
// 판정과 전달은 전부 서버. 클라는 PC 의 Server_ RPC 로 요청만 보내고, 서버가 받을 사람을 골라 Client_ RPC 로 돌림
//  - 계획 / 핑: 같은 구역 + 같은 편만 (배신 전투에서 상대 편에 새지 않게)
//  - 퀵챗: 채팅과 같이 모두에게
//  - 재촉: 아직 턴을 안 끝낸 같은 구역 사람에게만

// 핑 종류. 적에게는 앞 둘, 아군(나 포함)에게는 뒤 둘
UENUM(BlueprintType)
enum class EPingKind : uint8
{
	Focus     UMETA(DisplayName = "집중 공격"),
	Danger    UMETA(DisplayName = "위험"),
	NeedGuard UMETA(DisplayName = "보호 필요"),
	NeedHeal  UMETA(DisplayName = "회복 필요"),

	MAX       UMETA(Hidden)
};

// 퀵챗 문구. 순서 = 메뉴 번호 (1 부터). 문구는 PartyComms::GetQuickChatText
UENUM(BlueprintType)
enum class EQuickChat : uint8
{
	Good,
	Thanks,
	Wait,
	Sorry,
	Help,
	Go,

	MAX UMETA(Hidden)
};

// 계획한 스킬이 수치(BaseValue)를 어디에 쓰는지. 머리 위 표시 / 예상 피해 합산용
UENUM(BlueprintType)
enum class ECombatPlanEffect : uint8
{
	None,     // 상태이상만 등 (스킬 이름만 표시)
	Damage,
	Shield,
	Heal
};

// 한 사람의 이번 턴 행동 계획. 서버가 수치까지 계산해서 같은 편에게 보냄
// SkillRow 가 None 이면 '계획 없음' (지움)
USTRUCT(BlueprintType)
struct FCombatPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ATerminusPlayerState> Planner;

	// DT_Skill 행 이름. None = 계획 지움
	UPROPERTY(BlueprintReadOnly)
	FName SkillRow;

	UPROPERTY(BlueprintReadOnly)
	ETargetType TargetType = ETargetType::Self;

	// 대상 배틀러. 1명 대상 스킬이면 그 대상(아직 고르는 중이면 nullptr), 자신 스킬이면 시전자, 전체 스킬이면 nullptr
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly)
	ECombatPlanEffect Effect = ECombatPlanEffect::None;

	// 한 대 수치 (훈련소 강화 + 스텟 보정까지 서버가 계산). 대상 상태(급소 등)는 클라가 보고 더 계산
	UPROPERTY(BlueprintReadOnly)
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 HitCount = 1;

	// 어느 사이클의 계획인가. 사이클이 넘어가면 클라가 알아서 안 보여 줌
	UPROPERTY(BlueprintReadOnly)
	int32 Cycle = 0;

	// Shift 로 '예약' 한 계획인가 (false = 대상 고르는 중인 실시간 계획)
	UPROPERTY(BlueprintReadOnly)
	bool bDeclared = false;

	bool IsValid() const { return !SkillRow.IsNone(); }
};

// 핑 한 개
USTRUCT(BlueprintType)
struct FPartyPing
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ATerminusPlayerState> Sender;

	UPROPERTY(BlueprintReadOnly)
	EPingKind Kind = EPingKind::Focus;

	// 핑을 찍은 배틀러 (몬스터 / 플레이어)
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Target;
};

namespace PartyComms
{
	TERMINUS_API FText GetQuickChatText(EQuickChat Kind);
	TERMINUS_API FText GetPingText(EPingKind Kind);
	TERMINUS_API FLinearColor GetPingColor(EPingKind Kind);

	// 적에게 찍는 핑인가 (집중 공격 / 위험)
	TERMINUS_API bool IsEnemyPing(EPingKind Kind);

	// 한 대가 실제로 몇 들어갈지 어림. UCombatStatsComponent::ApplyDamage 의 ① ② 단계와 같은 순서
	// (공격자 공포 -25% -> 대상 급소 +25% -> 불굴 / 강철 1 고정 -> 철벽 감소). 보호막 / 회피는 안 봄
	TERMINUS_API int32 EstimateHit(int32 Amount, const UCombatStatsComponent* Caster, const UCombatStatsComponent* Target);
}
