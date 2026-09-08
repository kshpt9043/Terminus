// Fill out your copyright notice in the Description page of Project Settings.


#include "Online/SessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"          // GEngine (화면 출력)
#include "Online/OnlineSessionNames.h"   // NAME_GameSession, SEARCH_LOBBIES

DEFINE_LOG_CATEGORY_STATIC(LogTerminusSession, Log, All);

namespace
{
	// 480 로비 오염 필터용 키. 이 값이 일치하는 세션만 검색
	const FName KEY_BUILD_TAG(TEXT("TERMINUSBUILD"));
	const FString VALUE_BUILD_TAG(TEXT("Dev"));
}

void USessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USessionSubsystem::Deinitialize()
{
	// 진행 중이던 요청의 핸들이 남아 있을 수 있으므로 전부 정리한다.
	if (IOnlineSessionPtr Session = GetSessionInterface())
	{
		Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Session->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	Super::Deinitialize();
}

IOnlineSessionPtr USessionSubsystem::GetSessionInterface() const
{
	// PIE 테스트에선 컨텍스트별로 서브시스템 인스턴스가 갈려서, 월드를 넘겨야
	// 맞추기가 쉬움
	if (const UWorld* World = GetWorld())
	{
		return Online::GetSessionInterface(World);
	}
	return nullptr;
}

void USessionSubsystem::HostSession(int32 MaxPlayers, const FString& MapPath)
{
	// 만들어둔 헬퍼 함수로 접근
	IOnlineSessionPtr Session = GetSessionInterface();
	
	if (!Session.IsValid())
	{
		// 세션이 없으면 false로 완료 신호 보냄
		OnHostComplete.Broadcast(false);
		return;
	}
	
	// 이미 세션이 있으면 정리
	if (Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		PendingHostMap = MapPath;
		PendingMaxPlayers = MaxPlayers;
		bHostAfterDestroy = true;
		LeaveSession();
		return;
	}
	
	PendingHostMap = MapPath;
	
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch            = false;
	Settings.NumPublicConnections   = MaxPlayers;
	Settings.NumPrivateConnections  = 0;
	Settings.bShouldAdvertise       = true;
	Settings.bAllowJoinInProgress   = true;
	Settings.bAllowJoinViaPresence  = true;
	Settings.bUsesPresence          = true;
	Settings.bAllowInvites          = true;
	Settings.bUseLobbiesIfAvailable = true; 
	
	Settings.Set(KEY_BUILD_TAG, VALUE_BUILD_TAG,
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	CreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleCreateComplete));

	UE_LOG(LogTerminusSession, Log, TEXT("HostSession: MaxPlayers=%d, MapPath='%s'"), MaxPlayers, *MapPath);
	Session->CreateSession(0, NAME_GameSession, Settings);
}

void USessionSubsystem::FindSessions(int32 MaxResults)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		// 세션을 못 찾았으면 빈 배열과 false 반환
		OnFindComplete.Broadcast(false, {});
		return;
	}
	
	LastSearch = MakeShared<FOnlineSessionSearch>();
	// 스팀은 상한만큼 가져온 뒤 필터를 적용하기 때문에, 200 정도로 크게 잡아오기
	LastSearch->MaxSearchResults = FMath::Max(MaxResults, 200);
	LastSearch->bIsLanQuery = false;
	LastSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	// 빌드 태그 필터. bUseBuildFilter 를 끄면 남의 480 로비까지 전부 잡힌다.
	if (bUseBuildFilter)
	{
		LastSearch->QuerySettings.Set(KEY_BUILD_TAG, VALUE_BUILD_TAG, EOnlineComparisonOp::Equals);
	}

	FindHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleFindComplete));
	
	UE_LOG(LogTerminusSession, Log, TEXT("FindSessions: MaxResults=%d, bUseBuildFilter=%d"),
		LastSearch->MaxSearchResults, bUseBuildFilter ? 1 : 0);
	Session->FindSessions(0, LastSearch.ToSharedRef());
}

void USessionSubsystem::JoinSessionByIndex(int32 Index)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid() || !LastSearch.IsValid()
		|| !LastSearch->SearchResults.IsValidIndex(Index))
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	JoinHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleJoinComplete));

	Session->JoinSession(0, NAME_GameSession, LastSearch->SearchResults[Index]);
}

void USessionSubsystem::LeaveSession()
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid()) { OnLeaveComplete.Broadcast(false); return; }

	DestroyHandle = Session->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleDestroyComplete));

	Session->DestroySession(NAME_GameSession);
}

void USessionSubsystem::DumpSessionState()
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		UE_LOG(LogTerminusSession, Warning, TEXT("DumpSessionState: 세션 인터페이스 없음"));
		return;
	}

	const FNamedOnlineSession* Named = Session->GetNamedSession(NAME_GameSession);
	if (Named == nullptr)
	{
		UE_LOG(LogTerminusSession, Warning, TEXT("DumpSessionState: GameSession 이 존재하지 않음 (호스트/참가 전)"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange, TEXT("Session: none"));
		}
		return;
	}

	const int32 Registered = Named->RegisteredPlayers.Num();

	UE_LOG(LogTerminusSession, Log, TEXT("---- Session dump ----"));
	UE_LOG(LogTerminusSession, Log, TEXT("  State             : %s"), EOnlineSessionState::ToString(Named->SessionState));
	UE_LOG(LogTerminusSession, Log, TEXT("  bHosting          : %d"), Named->bHosting ? 1 : 0);
	UE_LOG(LogTerminusSession, Log, TEXT("  RegisteredPlayers : %d"), Registered);
	UE_LOG(LogTerminusSession, Log, TEXT("  PublicConnections : %d (open %d)"),
		Named->SessionSettings.NumPublicConnections, Named->NumOpenPublicConnections);

	for (int32 i = 0; i < Registered; ++i)
	{
		UE_LOG(LogTerminusSession, Log, TEXT("    [%d] %s"), i, *Named->RegisteredPlayers[i]->ToString());
	}

	// 두 번째 PC 에서는 로그를 보기 어려우므로 화면에도 요약을 띄운다.
	if (GEngine)
	{
		const FString Msg = FString::Printf(
			TEXT("Session[%s] hosting=%d  registered=%d  open=%d/%d"),
			EOnlineSessionState::ToString(Named->SessionState),
			Named->bHosting ? 1 : 0,
			Registered,
			Named->NumOpenPublicConnections,
			Named->SessionSettings.NumPublicConnections);

		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Msg);
	}

	// 엔진 기본 덤프 (LogOnlineSession 으로 전체 상세 출력)
	Session->DumpSessionState();
}

void USessionSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSessionInterface())
	{
		// clear핸들을 붙여야 델리게이트가 안 쌓임->콜백이 나중엔 2번 이상 발생할 수 있어서
		Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	
	OnHostComplete.Broadcast(bWasSuccessful);
	
	// true로 받고 맵이 있다면
	if (bWasSuccessful && !PendingHostMap.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			// 맵 이동
			World->ServerTravel(FString::Printf(TEXT("%s?listen"), *PendingHostMap));
		}
	}
	PendingHostMap.Reset();
}

void USessionSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSessionInterface())
	{
		Session->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	TArray<FTerminusSessionInfo> Out;
	int32 SkippedInvalid = 0;

	UE_LOG(LogTerminusSession, Log, TEXT("FindComplete: bWasSuccessful=%d, SearchValid=%d, RawResults=%d"),
		bWasSuccessful ? 1 : 0,
		LastSearch.IsValid() ? 1 : 0,
		LastSearch.IsValid() ? LastSearch->SearchResults.Num() : -1);

	if (bWasSuccessful && LastSearch.IsValid())
	{
		for (int32 i = 0; i < LastSearch->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& R = LastSearch->SearchResults[i];
			if (!R.IsValid())
			{
				// 여기서 전부 걸리면 IsValid() 조건이 원인이다.
				++SkippedInvalid;
				UE_LOG(LogTerminusSession, Warning, TEXT("  [%d] skipped: IsValid()==false, Owner=%s"),
					i, *R.Session.OwningUserName);
				continue;
			}

			FTerminusSessionInfo Info;
			Info.Index          = i;
			Info.MaxPlayers     = R.Session.SessionSettings.NumPublicConnections;
			Info.CurrentPlayers = Info.MaxPlayers - R.Session.NumOpenPublicConnections;
			Info.PingMs         = R.PingInMs;
			Info.HostName       = R.Session.OwningUserName;
			Out.Add(Info);
		}
	}
	UE_LOG(LogTerminusSession, Log, TEXT("FindComplete: broadcasting %d entries (skipped %d invalid)"),
		Out.Num(), SkippedInvalid);

	OnFindComplete.Broadcast(bWasSuccessful, Out);
}

void USessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid())
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	const bool bOk = (Result == EOnJoinSessionCompleteResult::Success);
	OnJoinComplete.Broadcast(bOk);
	if (!bOk || !Session.IsValid()) return;

	FString ConnectString;
	if (Session->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
		{
			PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
		}
	}
}

void USessionSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSessionInterface())
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	OnLeaveComplete.Broadcast(bWasSuccessful);

	// "나갔다 다시 호스트" 경로.
	// HostSession 은 기존 세션을 발견하면 LeaveSession() 만 부르고 리턴하므로,
	// 파괴가 끝난 지금 여기서 다시 호스트해야 흐름이 이어진다.
	if (bHostAfterDestroy)
	{
		bHostAfterDestroy = false;              // 재진입 전에 내려야 무한 루프를 피한다

		const FString MapPath    = PendingHostMap;
		const int32   MaxPlayers = PendingMaxPlayers;
		PendingHostMap.Reset();

		if (bWasSuccessful)
		{
			HostSession(MaxPlayers, MapPath);
		}
		else
		{
			OnHostComplete.Broadcast(false);
		}
	}
}
