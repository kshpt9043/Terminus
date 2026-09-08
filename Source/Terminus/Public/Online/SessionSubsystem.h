// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/OnlineSessionNames.h"
#include "SessionSubsystem.generated.h"

class FOnlineSessionSearch;

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
	void HostSession(int32 MaxPlayers, const FString& MapPath);

	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void FindSessions(int32 MaxResults = 200);

	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void JoinSessionByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Terminus|Session")
	void LeaveSession();

	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnHostComplete  OnHostComplete;
	
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnFindComplete  OnFindComplete;
	
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnJoinComplete  OnJoinComplete;
	
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Session")
	FOnLeaveComplete OnLeaveComplete;

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
	int32   PendingMaxPlayers = 4;
	bool    bHostAfterDestroy = false;
};