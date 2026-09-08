// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TavernWidget.generated.h"

class UPlayerSlot;
class UPanelWidget;
/**
 * 
 */
UCLASS()
class TERMINUS_API UTavernWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> SlotPanel;
	
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UPlayerSlot> SlotClass;
	
	UPROPERTY()
	TArray<TObjectPtr<UPlayerSlot>> Slots;

private:
	void RefreshSlots();
	
	FTimerHandle RefreshTimer;
	int32 LastPlayerCount = -1;
	static constexpr int32 MaxSlots = 4;
};
