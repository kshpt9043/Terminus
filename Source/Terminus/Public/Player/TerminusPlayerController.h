// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/CharacterTypes.h"
#include "TerminusPlayerController.generated.h"

class UTavernWidget;
class ADungeonArea;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnViewAreaChanged, ADungeonArea*, NewArea);

/**
 * 
 */
UCLASS()
class TERMINUS_API ATerminusPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UTavernWidget> TavernWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UTavernWidget> TavernWidget;
	
	// 시점 결정을 엔진 대신 우리가 함 -> 고정 카메라 레벨은 폰이 아니라 카메라를 봐야해서
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override;
	
	// 그 레벨의 고정 카메라. 없으면 nullptr
	class ACameraActor* FindFixedCamera() const;
	
public:
	UFUNCTION(Server, Reliable)
	void Server_SelectCharacter(ECharacterClass InClass);
	
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);
	
	UFUNCTION(Server, Reliable)
	void Server_StartGame();
	
	// 지도 방 선택 요청. MapManager 는 서버 소유 액터라 클라가 거기에 Server RPC 를 쏘면
	// "No owning connection" 으로 버려짐 -> 클라가 소유한 자기 PC 를 거쳐서 보냄
	// 누가 보냈는지는 서버가 이 PC 로 판단하니 PlayerState 를 인자로 받지 않음
	UFUNCTION(Server, Reliable)
	void Server_RequestSelectRoom(int32 RoomId);
	
	// 방 선택 실패 알림. 같은 이유로 MapManager 가 아니라 PC 의 Client RPC
	UFUNCTION(Client, Reliable)
	void Client_OnRoomSelectFailed(const FString& ReasonMessage);

	// -------------------------------------------------------------
	// [던전 구역]
	// -------------------------------------------------------------

	// 로컬 시점을 이 구역 카메라로. nullptr 이면 지도 고정 카메라로 복귀
	// 지금은 내 PS 의 CurrentArea 가 바뀔 때 불림. 관전도 이걸로 남의 구역을 보면 됨
	void ViewDungeonArea(ADungeonArea* Area);

	UFUNCTION(BlueprintPure, Category = "Dungeon Area")
	ADungeonArea* GetViewedArea() const { return ViewedArea.Get(); }

	// 보고 있는 구역이 바뀜 (nullptr = 지도 화면). 지도 위젯 숨기기 / 전투 HUD 교체를 여기에 걸 것
	UPROPERTY(BlueprintAssignable, Category = "Dungeon Area")
	FOnViewAreaChanged OnViewAreaChanged;

	// [테스트] 콘솔에서 DebugClearArea -> 내 구역 클리어 처리. 전투가 붙기 전까지 흐름 확인용
	//   DebugClearArea 1  -> 진행 중인 모든 구역 클리어 (멀티 PIE 에서 창마다 안 쳐도 되게)
	UFUNCTION(Exec)
	void DebugClearArea(bool bAll = false);

	UFUNCTION(Server, Reliable)
	void Server_DebugClearArea(bool bAll);

private:
	TWeakObjectPtr<ADungeonArea> ViewedArea;
};
