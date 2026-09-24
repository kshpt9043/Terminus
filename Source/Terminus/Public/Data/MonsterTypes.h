#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/StatTypes.h"
#include "MonsterTypes.generated.h"

// 몬스터 한 마리를 정의하는 열거형과 행 구조체
// 전부 monster_data.xlsx 의 Monsters 시트 기준
// 프로퍼티 이름 = CSV 컬럼 이름. 하나라도 다르면 그 열은 임포트 때 에러 없이 버려짐

UENUM(BlueprintType)
enum class EMonsterCategory : uint8
{
	Normal   UMETA(DisplayName = "일반"),
	Guardian UMETA(DisplayName = "가디언"),
	Boss     UMETA(DisplayName = "보스")
};

/**
 * 몬스터 한 마리의 정의. Monsters 시트 한 행과 1:1.
 *
 * 컬럼 20개, 프로퍼티 19개. RowName 은 DataTable 의 키라 필드로 안 넣음 (스킬이랑 같음).
 *
 * 스텟이 FCharacterStats 가 아니라 BaseHP/ATK/DEF 로 풀려 있는 건 엑셀이 납작해서.
 * CSV 는 칸 하나에 값 하나라 구조체를 통째로 못 넣음 -> ToStats() 로 바꿔서 씀
 */
USTRUCT(BlueprintType)
struct FMonsterRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- 식별

	// 기획서 추적용 코드. 보스 둘이 일반 슬라임이랑 코드가 겹쳐 있음 (기획 확인 대기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString MonsterID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName_KR;

	// --- 분류

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMonsterCategory MonsterCategory = EMonsterCategory::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Theme_KR;

	// 표층 1층 2층이 섞여 있어서 아직 열거형으로 못 뺌. 층 체계 정해지면 바꿀 것
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString AppearFloor;

	// --- 스텟
	// 스폰 때 체력 -5%~+10%, 공방 +0~1 랜덤이랑 2층 체력 +10% 는 여기 말고 스폰하는 쪽 몫

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseHP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseATK = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseDEF = 0;

	// --- 스킬
	// DT_Skill 의 행 이름. 기획 가이드엔 SkillID 를 참조한다고 돼 있는데
	// 행 이름이 DataTable 의 키라 조회가 공짜 -> 채울 때 행 이름으로 쓰자고 할 것
	// 가중치는 합이 100 일 필요 없음. 고를 때 정규화. 지금은 전부 비어 있음

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Skill1_ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Skill1_Weight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Skill2_ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Skill2_Weight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Skill3_ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Skill3_Weight = 0;

	// --- 특수

	// 세미콜론으로 여러 개. Passive 테이블이 아직 없어서 문자열 그대로 들고 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString PassiveID;

	// 소환 부활처럼 기본 스킬 시스템으로 안 되는 것들 설명. 런타임에선 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FString SpecialMechanic_Notes;

	// --- 연출. 2.5D 라 이름과 달리 스켈레탈 메시는 안 씀. 태그로만 받아둠

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AnimMontageTag;

	// --- 문서

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Concept_KR;

	/** 전투 컴포넌트가 먹는 모양으로 바꿔줌. 몬스터는 에너지를 안 써서 에너지 쪽은 기본값 그대로 */
	FCharacterStats ToStats() const
	{
		FCharacterStats S;
		S.MaxHealth = BaseHP;
		S.Attack = BaseATK;
		S.Defense = BaseDEF;
		return S;
	}
};
