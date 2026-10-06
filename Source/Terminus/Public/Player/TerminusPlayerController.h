// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/CharacterTypes.h"
#include "Game/ChatSubsystem.h"
#include "Map/MapManager.h"
#include "TerminusPlayerController.generated.h"

class UTavernWidget;
class UWidget;
class ADungeonArea;
class UCombatHUDWidget;
class UStartSkillPickWidget;
class UMapScreenWidget;
class UConfirmPopupWidget;
class UChatWidget;
class UStartRelicPickWidget;
class UMonsterRewardWidget;
class URiftWidget;
class UFloorTitleWidget;
class UFloorVoteWidget;
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

	// 레벨 이동 직전 (seamless 포함) -> 로딩 화면
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;

	// 마우스로 하는 게임이라 늘 커서를 보이고, 화면을 눌러도 마우스를 잡거나(캡처) 숨기지 않게
	// FocusWidget 이 있으면 키 입력이 그 위젯으로 감 (아무 키나 누르기 화면 등)
	void ApplyUIInputMode(UWidget* FocusWidget = nullptr);
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

	// 하드 모드 켜기 / 끄기 (주점, 방장만 받아들임)
	UFUNCTION(Server, Reliable)
	void Server_SetHardMode(bool bInHardMode);

	// 내 거점 강화(프로필, 이 컴퓨터에 저장)를 서버에 알림. 런 시작 때 서버가 런 스텟에 넣음
	UFUNCTION(Server, Reliable)
	void Server_ReportUpgrades(const TArray<FClassUpgrades>& Upgrades);
	
	// 지도 방 선택 요청. MapManager 는 서버 소유 액터라 클라가 거기에 Server RPC 를 쏘면
	// "No owning connection" 으로 버려짐 -> 클라가 소유한 자기 PC 를 거쳐서 보냄
	// 누가 보냈는지는 서버가 이 PC 로 판단하니 PlayerState 를 인자로 받지 않음
	UFUNCTION(Server, Reliable)
	void Server_RequestSelectRoom(int32 RoomId);
	
	// 방 선택 실패 알림. 같은 이유로 MapManager 가 아니라 PC 의 Client RPC
	UFUNCTION(Client, Reliable)
	void Client_OnRoomSelectFailed(const FString& ReasonMessage);

	// -------------------------------------------------------------
	// [채팅]
	// -------------------------------------------------------------

	// 채팅 보내기. 서버가 다듬어서(공백 / 길이 / 너무 잦은 전송) 모두에게 돌림
	UFUNCTION(Server, Reliable)
	void Server_SendChat(const FString& Message);

	// 채팅 받기. 서버가 접속한 모든 PC 에 보냄 -> 로컬 채팅 기록에 추가
	UFUNCTION(Client, Reliable)
	void Client_ReceiveChat(const FChatMessage& Message);

	// [서버] 모두에게 채팅 한 줄 (시스템 안내에도 씀)
	static void BroadcastChat(UWorld* World, const FChatMessage& Message);

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

	// 몬스터방 보상 화면 띄우기 (서버 -> 이 플레이어). Currency = 이미 받은 던전 재화, SkillOffers = 고를 수 있는 스킬
	UFUNCTION(Client, Reliable)
	void Client_ShowMonsterReward(int32 Currency, const TArray<FName>& SkillOffers);

	// 보상 마침 ('다음으로'). ChosenSkill = None 이면 안 고름, ReplaceSlot = 칸이 꽉 찼을 때 바꿀 강화 칸
	UFUNCTION(Server, Reliable)
	void Server_FinishMonsterReward(FName ChosenSkill, int32 ReplaceSlot);

	// 보상 화면 닫기 (누가 나가서 방이 닫혔을 때)
	UFUNCTION(Client, Reliable)
	void Client_CloseMonsterReward();

	// 다음 층 도착 화면 ("2층" / "표층 · 슬라임 왕국")
	UFUNCTION(Client, Reliable)
	void Client_ShowFloorTitle(const FText& Title, const FText& Subtitle);

	// 테마 끝 층 행선지 투표 (다음 층 / 탈출 / 배신). 서버가 거절하면 Client_FloorVoteRejected
	UFUNCTION(Server, Reliable)
	void Server_CastFloorVote(EFloorChoice Choice);

	UFUNCTION(Client, Reliable)
	void Client_FloorVoteRejected(const FText& Reason);

	// 투표 상태가 바뀜 (MapManager 의 OnRep). 화면을 띄우거나 갱신하거나 닫음
	void UpdateFloorVote(const FFloorVoteState& State);

	// 런이 끝남 (탈출). 안내 후 메인 화면으로
	UFUNCTION(Client, Reliable)
	void Client_RunEnded(const FText& Message);

	// 이공간 (멀티에서 누가 나갔을 때 / 멀티 세이브를 불러와 모이는 중). WaitingFor = 기다리는 사람. 이미 떠 있으면 명단만 갱신
	UFUNCTION(Client, Reliable)
	void Client_EnterRift(const TArray<FString>& WaitingFor, bool bFromSave);

	// 이공간에서 나옴 (전원 돌아옴) -> 지도
	UFUNCTION(Client, Reliable)
	void Client_LeaveRift();

	// [테스트] 콘솔에서 DebugWinCombat -> 내 구역 몬스터 전부 처치
	UFUNCTION(Exec)
	void DebugWinCombat();

	UFUNCTION(Server, Reliable)
	void Server_DebugWinCombat();

	// [테스트] 콘솔에서 DebugClearFloor -> 지금 층 보스를 깬 것처럼 처리
	// 테마 끝 층(2 / 4 / 6층)이면 행선지 투표, 아니면 바로 다음 층. 방을 진행 중이면 그 방을 닫고 함
	UFUNCTION(Exec)
	void DebugClearFloor();

	UFUNCTION(Server, Reliable)
	void Server_DebugClearFloor();

	// -------------------------------------------------------------
	// [런 시작 강화 스킬]
	// -------------------------------------------------------------

	// 고른 강화 스킬 장착 요청. NAME_None = 보유 스킬이 없어 건너뜀
	UFUNCTION(Server, Reliable)
	void Server_ChooseStartSkill(FName SkillRow);

	// 고른 창고 유물 장착 요청 (비어도 됨 = 안 들고 감)
	UFUNCTION(Server, Reliable)
	void Server_ChooseStartRelics(const TArray<FName>& RelicRows);

	// [테스트] 유물 고르기 화면 다시 띄우기
	UFUNCTION(Exec)
	void DebugShowStartRelicPick();

	UFUNCTION(Server, Reliable)
	void Server_DebugResetStartRelics();

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

	// [테스트] 유물. 콘솔에서
	//   DebugGainRelic RLC_Common_005  -> 그 유물 얻기 (다른 직업 / 몬스터 유물은 거절)
	//   DebugGainRelic All             -> 내가 가질 수 있는 유물 전부 (Basic 제외)
	//   DebugRemoveRelic RLC_Common_005
	//   DebugListRelics                -> 보유 유물을 로그로
	UFUNCTION(Exec)
	void DebugGainRelic(FName RelicRow);

	UFUNCTION(Server, Reliable)
	void Server_DebugGainRelic(FName RelicRow);

	UFUNCTION(Exec)
	void DebugRemoveRelic(FName RelicRow);

	UFUNCTION(Server, Reliable)
	void Server_DebugRemoveRelic(FName RelicRow);

	UFUNCTION(Exec)
	void DebugListRelics();

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

	// 런 시작 유물 고르기 화면. 비워 두면 C++ 기본 모양(UStartRelicPickWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UStartRelicPickWidget> StartRelicPickClass;

	UPROPERTY()
	TObjectPtr<UStartRelicPickWidget> StartRelicPick;

	// 유물 고르기를 이미 띄웠거나(또는 고를 게 없어 바로 보냈거나) -> 다시 띄우지 않게
	bool bStartRelicPickOpened = false;

	// 채팅창 클래스. 비워 두면 C++ 기본 모양(UChatWidget). 싱글(혼자 오프라인)이면 안 만듦
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UChatWidget> ChatWidgetClass;

	UPROPERTY()
	TObjectPtr<UChatWidget> ChatWidget;

	// 행선지 투표 화면. 비워 두면 C++ 기본 모양(UFloorVoteWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UFloorVoteWidget> FloorVoteClass;

	UPROPERTY()
	TObjectPtr<UFloorVoteWidget> FloorVote;

	// 층 도착 화면. 비워 두면 C++ 기본 모양(UFloorTitleWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UFloorTitleWidget> FloorTitleClass;

	// 이공간 화면. 비워 두면 C++ 기본 모양(URiftWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<URiftWidget> RiftClass;

	UPROPERTY()
	TObjectPtr<URiftWidget> Rift;

	// 몬스터방 보상 화면. 비워 두면 C++ 기본 모양(UMonsterRewardWidget)
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UMonsterRewardWidget> MonsterRewardClass;

	UPROPERTY()
	TObjectPtr<UMonsterRewardWidget> MonsterReward;

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

	// [서버] 마지막으로 채팅을 받은 시각 (도배 막기)
	double LastChatTime = -1.0;

	// 멀티면 채팅창 만들기
	void CreateChatWidget();

	// 던전에 들어와 내 PS 와 지도(MapManager)가 둘 다 보이면 흐름 시작
	// 게임 시작 -> (아직 안 골랐으면) 시작 스킬 고르기 -> 서버가 고르기 완료를 확인하면 지도 화면
	// 클라에선 둘 다 복제로 늦게 와서 잠깐씩 다시 확인
	FTimerHandle StartSkillCheckTimer;
	int32 StartSkillCheckTries = 0;
	void CheckStartSkillPick();

	// 지도 화면 띄우기 (이미 떠 있으면 무시)
	void OpenMapScreen();

	// 내 RunState 가 바뀜 -> 다음 단계로 (스킬 -> 유물 -> 지도)
	UFUNCTION()
	void HandleLocalRunStateChanged(const FRunState& NewRunState);

	// 런 시작 흐름: 시작 스킬 -> 창고 유물 -> 지도. 지금 상태에 맞는 화면을 띄움
	void ContinueStartFlow(const FRunState& Run);

	// 창고 유물 고르기. 창고가 비어 있으면 화면 없이 '안 고름'으로 보냄
	void OpenStartRelicPick();

	// 보유 스킬에서 최대 3장 뽑아 화면 띄우기. 보유 스킬이 없으면 "없음" 안내 화면 (아무 키 -> 지도)
	void OpenStartSkillPick();
};
