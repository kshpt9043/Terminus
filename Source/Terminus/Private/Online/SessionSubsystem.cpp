// Fill out your copyright notice in the Description page of Project Settings.


#include "Online/SessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"          // GEngine (화면 출력)
#include "Online/OnlineSessionNames.h"   // NAME_GameSession, SEARCH_LOBBIES
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusSession, Log, All);

namespace
{
	// 480 로비 오염 필터용 키. 이 값이 일치하는 세션만 검색
	const FName KEY_BUILD_TAG(TEXT("TERMINUSBUILD"));
	const FString VALUE_BUILD_TAG(TEXT("Dev"));
	// 나가거나 끊겼을 때 돌아갈 곳. DefaultEngine.ini 의 GameDefaultMap 과 같아야 함
	const FName MENU_MAP(TEXT("/Game/Maps/Lv_Lobby"));
}

void USessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this, &USessionSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
			this, &USessionSubsystem::HandleTravelFailure);
	}
	
	// 초대는 게임 시작 직후에도 올 수 있어서 월드 없이 기본 OSS 로 붙인다
	if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
	{
		if (IOnlineSessionPtr Session = OSS->GetSessionInterface())
		{
			InviteHandle = Session->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(
					this, &USessionSubsystem::HandleInviteAccepted));
		}
	}
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

	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	
	if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
	{
		if (IOnlineSessionPtr Session = OSS->GetSessionInterface())
		{
			Session->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
		}
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
		AfterDestroy = EAfterDestroy::Host;
		LeaveSession();
		return;
	}
	
	PendingHostMap = MapPath;
	
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch            = false;
	Settings.NumPublicConnections   = MaxPlayers;
	Settings.NumPrivateConnections  = 0;
	Settings.bShouldAdvertise       = true;
	Settings.bAllowJoinInProgress   = false; // 게임 중인 세션은 참가 x
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
	// 인덱스는 BP 목록용 창구일 뿐, 실제 참가는 결과 자체로 한다
	if (!LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(Index))
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	JoinSearchResult(LastSearch->SearchResults[Index]);
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

void USessionSubsystem::LeaveToMenu()
{
	IOnlineSessionPtr Session = GetSessionInterface();

	// 정리할 세션이 없으면 바로 이동
	if (!Session.IsValid() || Session->GetNamedSession(NAME_GameSession) == nullptr)
	{
		TravelToMenu();
		return;
	}

	AfterDestroy = EAfterDestroy::ToMenu;
	LeaveSession();
}

FText USessionSubsystem::ConsumeDisconnectReason()
{
	FText Out = PendingDisconnectReason;
	PendingDisconnectReason = FText::GetEmpty();
	return Out;
}

void USessionSubsystem::StartRun()
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid() || Session->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return;
	}
	
	UE_LOG(LogTerminusSession, Log, TEXT("StartRun: 세션 진행 중으로 전환"));
	Session->StartSession(NAME_GameSession);
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

	OnLeaveComplete.Broadcast(bWasSuccessful);

	// 재진입 전에 먼저 내려야 함. 안 그러면 Host -> Leave -> Host ... 무한 루프
	const EAfterDestroy Next = AfterDestroy;
	AfterDestroy = EAfterDestroy::None;

	switch (Next)
	{
	case EAfterDestroy::Host:
		{
			const FString MapPath = PendingHostMap;
			PendingHostMap.Reset();

			if (bWasSuccessful) { HostSession(PendingMaxPlayers, MapPath); }
			else                { OnHostComplete.Broadcast(false); }
			break;
		}
	case EAfterDestroy::Join:
		// 파괴가 실패했는데 또 참가하면 세션이 그대로라 같은 곳을 무한히 돈다
		if (bWasSuccessful) { JoinSearchResult(PendingJoinResult); }
		else                { OnJoinComplete.Broadcast(false); }
		break;

	case EAfterDestroy::ToMenu:
		TravelToMenu();
		break;

	default:
		break;
	}
}

void USessionSubsystem::JoinSearchResult(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	// 이전 세션이 남아 있으면 스팀이 참가를 거절함 -> 먼저 부수고 이어서 참가
	if (Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		PendingJoinResult = Result;
		AfterDestroy = EAfterDestroy::Join;
		LeaveSession();
		return;
	}

	JoinHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleJoinComplete));

	Session->JoinSession(0, NAME_GameSession, Result);
}

void USessionSubsystem::TravelToMenu()
{
	// 호스트: 리슨 서버가 닫힘 / 클라: 연결이 끊김
	UGameplayStatics::OpenLevel(GetWorld(), MENU_MAP);
}

void USessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	// 전역 이벤트라 PIE 다중 창이면 남의 인스턴스 소식도 옴
	if (World && World->GetGameInstance() != GetGameInstance()) { return; }

	UE_LOG(LogTerminusSession, Warning, TEXT("NetworkFailure: %s / %s"),
		ENetworkFailure::ToString(FailureType), *ErrorString);

	CleanupAfterFailure(FText::FromString(TEXT("연결이 끊겼습니다.")));
}

void USessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != GetGameInstance()) { return; }

	UE_LOG(LogTerminusSession, Warning, TEXT("TravelFailure: %s / %s"),
		ETravelFailure::ToString(FailureType), *ErrorString);

	CleanupAfterFailure(FText::FromString(TEXT("방에 들어가지 못했습니다.")));
}

void USessionSubsystem::CleanupAfterFailure(const FText& Reason)
{
	PendingDisconnectReason = Reason;

	// 이동은 엔진이 기본 맵으로 해줌. 우리는 남은 세션만 치운다
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid() && Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		AfterDestroy = EAfterDestroy::None;   // 끊긴 마당에 대기 중이던 Host/Join 은 취소
		LeaveSession();
	}
}

void USessionSubsystem::HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId,
	FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	UE_LOG(LogTerminusSession, Log, TEXT("InviteAccepted: ok=%d valid=%d"),
		bWasSuccessful ? 1 : 0, InviteResult.IsValid() ? 1 : 0);

	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	// 내 주점을 열어둔 상태여도 JoinSearchResult 가 먼저 정리하고 들어간다
	JoinSearchResult(InviteResult);
}
