// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerSlot.generated.h"

class UTextBlock;
class APlayerState;
/**
 * 
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
	void Setup(APlayerState* PS);
	void Clear();
	
};
