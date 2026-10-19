// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/TerminusBattler.h"
#include "TerminusMonster.generated.h"

/**
 * 몬스터 폰. 배틀러 몸통(3층 구조, 좌우 반전, 전투 컴포넌트)을 그대로 물려받고
 * 초기화만 직업 DT 대신 몬스터 DT 에서 함
 */

UCLASS()
class TERMINUS_API ATerminusMonster : public ATerminusBattler
{
	GENERATED_BODY()
	
public:
	ATerminusMonster();

	// 공격 스킬을 쓸 때 서버가 부름. AttackFlipbooks 중 하나를 골라 모든 화면에서 재생
	void PlayAttackAnim();

protected:
	virtual void BeginPlay() override;

	// 어떤 몬스터인지. DT_Monster 의 행 이름. 레벨에 세울 때 디테일에서 바꿈
	UPROPERTY(EditAnywhere, Category = "Terminus|Monster")
	FName MonsterRow = TEXT("mon_slime");

	void InitAsMonster(FName RowName);

	// 공격 연출 후보. 오른쪽을 보는 그림 기준 (몬스터는 좌우 반전돼서 왼쪽으로 나감). 비어 있으면 연출 없음
	// 임시: 몬스터 종류별 외형이 DT 로 빠지기 전까지 BP 에서 직접 지정
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Visual")
	TArray<TObjectPtr<UPaperFlipbook>> AttackFlipbooks;
};
