// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/MapManager.h"
#include "RoomNodeWidget.generated.h"

class UHorizontalBox;
class UBorder;
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
	
	int32 RoomId = -1;
	int32 Row = 0;
	
	// 외부에서 구독할 클릭 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Room Event")
	FOnRoomNodeClicked OnRoomNodeClicked;
	
	UPROPERTY(BlueprintReadWrite)
	TArray<UTexture2D*> RoomTextures;
	
	UPROPERTY(meta = (BindWidget))
	UButton* RoomButton;

	UPROPERTY(meta = (BindWidget))
	UImage* RoomIconImage;
	
	UPROPERTY(meta = (BindWidget))
	UBorder* SelectionBorder; // 내 선택 시 표시될 빨간 테두리

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* ProfileContainer; // 선택한 플레이어 초상화들이 들어가는 컨테이너
	
	UFUNCTION(BlueprintCallable, Category = "Room")
	void SetupRoomNode(const FRoomNode& InRoomData, const TMap<ERoomType, UTexture2D*>& IconMap);
	
	// 버튼 활성화 / 비활성화 (이동 불가능한 방은 어둡게 비활성화)
	void SetRoomSelectable(bool bSelectable);

	// 모든 PlayerState 수신하여 UI 선택 상태 갱신
	void RefreshSelectionState(const TArray<ATerminusPlayerState*>& AllPlayerStates, ATerminusPlayerState* LocalPS);
	
protected:
	
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnButtonClicked();
	
private:
	
};
