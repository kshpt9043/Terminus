// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DungeonGameMode.generated.h"

/**
 * 
 */
UCLASS()
class TERMINUS_API ADungeonGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	/* 기본 겜모 함수 오버라이드 - PlayerStart 지점 고르는 함수인데, 배틀러 위치
	 * 를 로비 UI에 있던 순서대로 하기 위해서 함 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
protected:
	// 이미 배정한 자리. 같은 자리를 두 번 주지 않기 위함
	UPROPERTY()
	TArray<TObjectPtr<AActor>> AssignedStarts;
};
