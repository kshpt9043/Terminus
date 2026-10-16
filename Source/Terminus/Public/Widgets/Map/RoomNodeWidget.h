// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "Map/MapManager.h"
#include "RoomNodeWidget.generated.h"

class UHorizontalBox;
class UBorder;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomNodeClicked, int32, RoomId);

class UButton;
class UImage;
class USizeBox;
class UTexture2D;

// 지도 시안의 방 아이콘 상태 4종 + 현재 위치
UENUM(BlueprintType)
enum class ERoomNodeState : uint8
{
	Default,      // 기본: 아직 갈 수 있는 앞쪽 방
	Available,    // 갈 수 있음: 지금 고를 수 있는 방 (등불 빛)
	Visited,      // 지나옴: 먹으로 X
	Current,      // 현재 위치: 먹 동그라미
	Unreachable   // 못 감: 흐리게
};

/**
 * 지도의 방 하나. [아이콘 상자 150x150][동료 칩]
 * 아이콘 상자 가운데에 아이콘(64, 보스 96). 그 위로 등불 빛 / X / 먹 동그라미 / 내 선택(붉은 잉크)
 *
 * WBP 에 둘 것: RoomButton / RoomIconImage / SelectionBorder / ProfileContainer (필수)
 *               GlowImage / VisitedMark / CurrentRing / IconBox (선택)
 */
UCLASS()
class TERMINUS_API URoomNodeWidget : public UTerminusUserWidget
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
	UBorder* SelectionBorder; // 내 선택 시 표시될 붉은 잉크 동그라미

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* ProfileContainer; // 이 방을 고른 동료 칩들

	// 갈 수 있는 방 뒤 등불 빛
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* GlowImage;

	// 지나온 방 위 먹 X
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* VisitedMark;

	// 현재 위치 먹 동그라미
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* CurrentRing;

	// 아이콘 크기 상자 (보스만 크게)
	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* IconBox;

	// 아이콘 크기 (보스 / 그 외)
	UPROPERTY(EditAnywhere, Category = "Room")
	float IconSize = 64.f;

	UPROPERTY(EditAnywhere, Category = "Room")
	float BossIconSize = 96.f;

	// 동료 칩 한 변 (테두리 포함)
	UPROPERTY(EditAnywhere, Category = "Room")
	float ChipSize = 28.f;

	// 아이콘 불투명도: 보통 / 못 감
	UPROPERTY(EditAnywhere, Category = "Room")
	float IconOpacity = 0.94f;

	UPROPERTY(EditAnywhere, Category = "Room")
	float UnreachableOpacity = 0.32f;

	UFUNCTION(BlueprintCallable, Category = "Room")
	void SetupRoomNode(const FRoomNode& InRoomData, const TMap<ERoomType, UTexture2D*>& IconMap);

	// 누를 수 있는 방인가. 못 누르는 방은 호버 효과 / 클릭만 막힘 (모양은 SetVisualState)
	void SetRoomSelectable(bool bSelectable);

	// 지도 시안 상태 (빛 / X / 동그라미 / 흐림)
	void SetVisualState(ERoomNodeState NewState);

	// 모든 PlayerState 수신하여 UI 선택 상태 갱신
	void RefreshSelectionState(const TArray<ATerminusPlayerState*>& AllPlayerStates, ATerminusPlayerState* LocalPS);

protected:

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnButtonClicked();

private:
	// 마지막으로 적용한 상태. 같은 값이 다시 들어오면 아무것도 안 다시 그림
	// (지도 갱신이 올 때마다 모든 방이 초상화를 지웠다 붙이지 않게)
	TOptional<bool> AppliedSelectable;

	TOptional<bool> AppliedSelectedByMe;

	TOptional<ERoomNodeState> AppliedState;

	bool bIsBoss = false;

	// 이 방을 고른 플레이어들 (PlayerArray 순서)
	TArray<TWeakObjectPtr<ATerminusPlayerState>> AppliedSelectors;
};
