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