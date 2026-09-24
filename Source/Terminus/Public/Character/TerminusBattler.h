// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/CharacterTypes.h"
#include "GameFramework/Pawn.h"
#include "TerminusBattler.generated.h"

// 플립북이랑 ZD 애님 관련 전방선언

class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class UCombatStatsComponent;

// 전투 시 플레이어나 몬스터 다 이 폰을 베이스로 함

UCLASS()
class TERMINUS_API ATerminusBattler : public APawn
{
	GENERATED_BODY()

public:
	ATerminusBattler();
	
	// 테스트용 함수
	UFUNCTION(Exec)
	void DebugDamage(int32 Amount = 20);
	
	UFUNCTION(Exec)
	void DebugSkill(FName RowName);
	
	UFUNCTION(Exec)
	void DebugStatus(FName StatusName, int32 Value = 1, int32 Duration = 1);
	
	UFUNCTION(Exec)
	void DebugTurnEnd();

	UFUNCTION(Exec)
	void DebugCycleEnd();
	
	UFUNCTION(Exec)
	void DebugCast(FName RowName);
	
	UFUNCTION(Exec)
	void DebugCastAt(FName RowName, int32 MonsterIndex = 0);

protected:
	// 테스트용. 체력이랑 상태이상 목록 찍기
	void LogCombatState() const;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<USceneComponent> Root;
	
	// 좌우 반전 전용 층. PaperZD 가 Sprite 스케일을 매 프레임 덮어쓰고,
	// Root 스케일은 레벨 배치값이라 둘 다 쓸 수 없어서 사이에 하나 둔다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<USceneComponent> Visual;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperZDAnimationComponent> Animation;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Component")
	TObjectPtr<UCombatStatsComponent> CombatStats;
	
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	
	// 호스트의 PossessedBy, 클라이언트의 OnRep에서 불릴 비주얼 적용 함수
	// void ApplyClassVisual();
	
	virtual void BeginPlay() override;

	// 클래스 하나로 스텟과 외형을 같이 세팅.
	void InitAsClass(ECharacterClass InClass);

	// 레벨에 직접 배치해서 구도만 볼 때 체크. PlayerState 대신 아래 값을 쓴다
	UPROPERTY(EditAnywhere, Category = "Terminus|Preview")
	bool bUsePreviewClass = false;

	UPROPERTY(EditAnywhere, Category = "Terminus|Preview", meta = (EditCondition = "bUsePreviewClass"))
	ECharacterClass PreviewClass = ECharacterClass::Fighter; 
	
public:
	UFUNCTION(BlueprintCallable, Category = "Terminus|Visual")
	void SetFacingRight(bool bRight);
	
	UFUNCTION(BlueprintPure, Category = "Terminus|Visual")
	bool IsFacingRight() const { return bFacingRight; }
	
	// 밖에서 이 배틀러의 전투 컴포넌트를 꺼낼 때. 턴 진행 쪽이 대상 고를 때 쓸 것
	UFUNCTION(BlueprintPure, Category = "Terminus|Component")
	UCombatStatsComponent* GetCombatStats() const { return CombatStats; }
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	bool bFacingRight = true;
};
