// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TerminusBattler.generated.h"

// 플립북이랑 ZD 애님 관련 전방선언

class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;

// 전투 시 플레이어나 몬스터 다 이 폰을 베이스로 함

UCLASS()
class TERMINUS_API ATerminusBattler : public APawn
{
	GENERATED_BODY()

public:
	ATerminusBattler();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	TObjectPtr<UPaperZDAnimationComponent> Animation;
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "Terminus|Visual")
	void SetFacingRight(bool bRight);
	
	UFUNCTION(BlueprintPure, Category = "Terminus|Visual")
	bool IsFacingRight() const { return bFacingRight; }
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminus|Visual")
	bool bFacingRight = true;
};
