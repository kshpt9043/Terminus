// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/CharacterTypes.h"
#include "TerminusPlayerController.generated.h"

class UTavernWidget;
class ADungeonArea;
class UCombatHUDWidget;
class UStartSkillPickWidget;
class UMapScreenWidget;
class UConfirmPopupWidget;
struct FRunState;

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
	// [팝업]
	// -------------------------------------------------------------

	// 범용 팝업 띄우기 (로컬 화면). CancelLabel 을 비우면 확인 버튼 하나짜리 알림
	// 결과는 돌려받은 위젯의 OnConfirmed / OnCancelled (C++ 은 OnConfirmedNative / OnCancelledNative)
	UFUNCTION(BlueprintCallable, Category = "Terminus|UI")
	UConfirmPopupWidget* ShowPopup(const FText& Title, const FText& Message, const FText& ConfirmLabel, const FText& CancelLabel);

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

	// -------------------------------------------------------------
	// [전투]
	// -------------------------------------------------------------

	// 스킬 사용 요청. SkillIndex = 기본 스킬 0~2 (공격 방어 특수)
	// TargetIndex = 적 1명 스킬이면 몬스터 인덱스, 아군 1명이면 구역 인원 인덱스, 나머지는 무시
	// 판정은 서버에서 내가 들어가 있는 구역의 전투가 함
	UFUNCTION(Server, Reliable)
	void Server_UseSkill(int32 SkillIndex, int32 TargetIndex);

	UFUNCTION(Server, Reliable)
	void Server_EndTurn();

	// [테스트] 콘솔에서 DebugWinCombat -> 내 구역 몬스터 전부 처치
	UFUNCTION(Exec)
	void DebugWinCombat();

	UFUNCTION(Server, Reliable)
	void Server_DebugWinCombat();

	// -------------------------------------------------------------
	// [런 시작 강화 스킬]
	// -------------------------------------------------------------

	// 고른 강화 스킬 장착 요청. NAME_None = 보유 스킬이 없어 건너뜀
	UFUNCTION(Server, Reliable)
	void Server_ChooseStartSkill(FName SkillRow);

	// [테스트] 보유 스킬(영구, 이 컴퓨터 세이브) 조작. 콘솔에서
	//   DebugOwnSkill holy_charge  -> 그 스킬 보유
	//   DebugOwnAllSkills          -> 강화 스킬로 쓸 수 있는 DT_Skill 행 전부 보유
	//   DebugClearOwnedSkills      -> 보유 스킬 전부 삭제 (완전 첫판 상태)
	//   DebugShowStartSkillPick    -> 장착을 비우고 고르기 화면 다시 띄우기
	UFUNCTION(Exec)
	void DebugOwnSkill(FName SkillRow);

	UFUNCTION(Exec)
	void DebugOwnAllSkills();

	UFUNCTION(Exec)
	void DebugClearOwnedSkills();

	UFUNCTION(Exec)
	void DebugShowStartSkillPick();

	UFUNCTION(Server, Reliable)
	void Server_DebugResetStartSkill();

protected:
	// 전투 HUD 클래스. 비워 두면 C++ 기본 배치(UCombatHUDWidget)를 씀. WBP 를 만들면 BP_DungeonPC 에서 지정
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UCombatHUDWidget> CombatHUDClass;

	UPROPERTY()
	TObjectPtr<UCombatHUDWidget> CombatHUD;

	// 런 시작 강화 스킬 고르기 화면. 비워 두면 C++ 기본 모양(UStartSkillPickWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UStartSkillPickWidget> StartSkillPickClass;

	UPROPERTY()
	TObjectPtr<UStartSkillPickWidget> StartSkillPick;

	// 범용 팝업 클래스. 비워 두면 C++ 기본 모양(UConfirmPopupWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UConfirmPopupWidget> PopupClass;

	// 지도 화면 (WBP_MapScreen: 상단바 + 지도). 시작 스킬 고르기가 끝나면 뜸
	// 지도 위젯은 RoomWidgetClass / 아이콘 같은 에셋 설정이 필요해서 C++ 기본 모양이 없음 -> 꼭 지정할 것
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UMapScreenWidget> MapScreenClass;

	UPROPERTY()
	TObjectPtr<UMapScreenWidget> MapScreen;

private:
	TWeakObjectPtr<ADungeonArea> ViewedArea;

	// 던전에 들어와 내 PS 와 지도(MapManager)가 둘 다 보이면 흐름 시작
	// 게임 시작 -> (아직 안 골랐으면) 시작 스킬 고르기 -> 서버가 고르기 완료를 확인하면 지도 화면
	// 클라에선 둘 다 복제로 늦게 와서 잠깐씩 다시 확인
	FTimerHandle StartSkillCheckTimer;
	int32 StartSkillCheckTries = 0;
	void CheckStartSkillPick();

	// 지도 화면 띄우기 (이미 떠 있으면 무시)
	void OpenMapScreen();

	// 내 RunState 가 바뀜 -> 시작 스킬 고르기가 끝났으면 지도 화면
	UFUNCTION()
	void HandleLocalRunStateChanged(const FRunState& NewRunState);

	// 보유 스킬에서 최대 3장 뽑아 화면 띄우기. 보유 스킬이 없으면 "없음" 안내 화면 (아무 키 -> 지도)
	void OpenStartSkillPick();
};
