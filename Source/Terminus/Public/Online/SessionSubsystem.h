// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "SessionSubsystem.generated.h"

class FOnlineSessionSearch;
class UNetDriver;

USTRUCT(BlueprintType)
struct FTerminusSessionInfo
{
	// FOnlineSessionSearchResult는 검색 결과인데 BP에 노출 x
	// 그래서 노출 가능한 구조체를 만듦
	
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly) int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString HostName;
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 PingMs = 0;
	UPROPERTY(BlueprintReadOnly) FString RoomName;
	// 비밀번호 걸림 여부
	UPROPERTY(BlueprintReadOnly) bool bLocked = false;
	// 던전 진행 중에 누가 나가서 그 사람의 재합류를 기다리는 방 (이공간). 나갔던 사람만 들어갈 수 있음
	UPROPERTY(BlueprintReadOnly) bool bRejoinWaiting = false;
};

USTRUCT(BlueprintType)
struct FTerminusRoomOptions
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Password;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bListed = true;
};

// dynamic으로 BP 바인딩 + OSS는 비동기이므로 델리게이트 선언

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHostComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFindComplete, bool, bWasSuccessful, const TArray<FTerminusSessionInfo>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaveComplete, bool, bWasSuccessful);

/**
 * 
 */
UCLASS()
class TERMINUS_API USessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void HostSession(int32 MaxPlayers, const FString& MapPath, const FTerminusRoomOptions& Options);
	
	UFUNCTION(BlueprintPure, Category = "Terminus|Session")
	static bool IsValidRoomPassword(const FString& InPassword);
	
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void FindSessions(int32 MaxResults = 200);

	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void JoinSessionByIndex(int32 Index, const FString& Password = TEXT(""));

	bool CheckJoinRequest(const FString& Options, int32 CurrentPlayers, FString& OutError) const;
	
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void LeaveSession();
	
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void LeaveToMenu();

	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnHostComplete  OnHostComplete;
	
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnFindComplete  OnFindComplete;
	
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnJoinComplete  OnJoinComplete;
	
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnLeaveComplete OnLeaveComplete;
	
	// 메뉴 화면이 켜질 때 한 번 꺼내 보는 용도. 꺼내면 비워진다
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	FText ConsumeDisconnectReason();

	// 마지막 참가 실패 사유. OnJoinComplete(false) 를 받은 쪽이 화면에 띄울 때 읽는다
	UFUNCTION(BlueprintPure, Category = "Terminus|Session")
	FText GetLastJoinError() const { return LastJoinError; }

	// 참가 요청부터 주점 도착(맵 로드)까지 기다리는 최대 시간(초)
	// 넘으면 접속을 취소하고 실패로 알린다. 호스트가 응답 없이 멈춰도 "들어가는 중" 에 갇히지 않게
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terminus|Session")
	float JoinTimeoutSeconds = 20.f;
	
	// 호스트가 시작할 때 세션을 진행 중이라고 바꾸는 함수
	void StartRun();

	// [방장] 이공간: 진행 중에 나간 사람이 주점 목록에서 찾아 다시 들어올 수 있게 세션을 목록에 다시 띄움 / 내림
	// 들어오는 사람이 나갔던 사람인지는 던전 게임모드가 확인
	void SetRejoinListing(bool bOpen);
	
	// 스팀 오버레이의 친구 초대 창을 연다. 세션에 들어가 있을 때만 의미 있음
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void ShowInviteUI();

	/**
	 * 검색 시 빌드 태그 필터를 적용할지 여부.
	 * spacewar(480) 은 전 세계 개발자가 공유하는 AppID 라, 켜두면 우리 로비만 잡힌다.
	 * 끄면 남의 480 로비까지 전부 잡히므로 검색 경로 자체가 살아 있는지 확인할 때 쓴다.
	 * 자체 AppID 로 전환하면(M5) 이 스위치와 태그가 함께 필요 없어진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terminus|Session")
	bool bUseBuildFilter = true;

	/**
	 * 현재 NAME_GameSession 의 상태를 로그와 화면에 덤프한다. 디버그 전용.
	 * RegisterPlayer 가 실제로 걸리고 있는지(RegisteredPlayers 수)를 확인하는 용도.
	 * 접속이 끝난 뒤 호스트 쪽에서 호출할 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "Terminus|Session|Debug")
	void DumpSessionState();

private:
	IOnlineSessionPtr GetSessionInterface() const;

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);

	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle;

	TSharedPtr<FOnlineSessionSearch> LastSearch;

	FString PendingHostMap;

	// 이공간으로 목록에 다시 띄운 상태인가 / 띄우기 전 LISTED 값 (닫을 때 되돌림)
	bool bRejoinListed = false;
	FString ListedBeforeRejoin;
	int32   PendingMaxPlayers = 4;
	
	// 검색 결과, 초대 모두 이 함수를 타게 하기
	void JoinSearchResult(const FOnlineSessionSearchResult& Result);
	
	// 세션 파괴는 비동기라서 끝난 후 할 일이 정해져야 함
	enum class EAfterDestroy : uint8
	{
		None, Host, Join, ToMenu
	};
	EAfterDestroy AfterDestroy = EAfterDestroy::None;
	
	// 파괴를 기다리는 동안 참가할 대상
	FOnlineSessionSearchResult PendingJoinResult;
	
	void TravelToMenu();
	
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void CleanupAfterFailure(const FText& Reason);

	FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
	FText PendingDisconnectReason;
	
	void HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId,
		FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	FDelegateHandle InviteHandle;
	
	// 파괴 후 다시 호스트할 때 쓸 옵션
	FTerminusRoomOptions PendingRoomOptions;

	// 호스트만 들고 있는 비밀번호
	FString HostPassword;
	
	int32 HostMaxPlayers = 0;

	FString PendingTravelOptions;

	// --- 참가 실패 / 타임아웃
	// 참가 실패 공통 처리. 사유를 남기고 OnJoinComplete(false)
	// bLeaveSession 이면 들어가 있던 스팀 로비도 나온다 (파괴 실패 경로에선 false -> 무한 반복 방지)
	void FailJoin(const FText& Reason, bool bLeaveSession = true);

	// 타이머는 JoinSession 요청 때 켜고, 주점 도착 / 실패 / 타임아웃 때 끈다
	void StartJoinTimeout();
	void ClearJoinTimeout();
	void HandleJoinTimeout();

	// 맵 로드가 끝남 = 주점 도착. 참가 타이머를 끄는 곳
	void HandlePostLoadMap(UWorld* LoadedWorld);

	FTimerHandle JoinTimeoutTimer;
	FDelegateHandle PostLoadMapHandle;
	FText LastJoinError;
};