// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerSlot.generated.h"

class UTextBlock;
class APlayerState;

/**
 * 로비 슬롯 하나. 받은 PlayerState 를 그리기만 하고 아무 판단도 하지 않는다.
 */
UCLASS()
class TERMINUS_API UPlayerSlot : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// WBP에서 바인드해서 생성
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PlayerNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ClassNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget>    ReadyMark;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget>    EmptyMark;

public:
	// PS 가 null 이면 빈 자리로 되돌린다
	void Setup(APlayerState* PS);
	void Clear();

};
