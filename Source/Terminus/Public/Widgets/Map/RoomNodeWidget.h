// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/MapGenerator.h"
#include "RoomNodeWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomNodeClicked, int32, RoomId);

class UButton;
class UImage;
class UTexture2D;
/**
 * 
 */
UCLASS()
class TERMINUS_API URoomNodeWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	// 외부에서 구독할 클릭 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Room Event")
	FOnRoomNodeClicked OnRoomNodeClicked;
	
	UPROPERTY(BlueprintReadWrite)
	TArray<UTexture2D*> RoomTextures;
	
	UPROPERTY(meta = (BindWidget))
	UButton* RoomButton;

	UPROPERTY(meta = (BindWidget))
	UImage* RoomIconImage;
	
	UFUNCTION(BlueprintCallable, Category = "Room")
	void SetupRoomNode(const FRoomNode& InRoomData, const TMap<ERoomType, UTexture2D*>& IconMap);
	
protected:
	
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnButtonClicked();
	
private:
	int32 RoomId;
};
