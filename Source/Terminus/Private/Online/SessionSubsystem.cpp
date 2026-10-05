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
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Misc/Base64.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusSession, Log, All);

namespace
{
	// 480 로비 오염 필터용 키. 이 값이 일치하는 세션만 검색
	const FName KEY_BUILD_TAG(TEXT("TERMINUSBUILD"));
	const FString VALUE_BUILD_TAG(TEXT("Dev"));
	// 나가거나 끊겼을 때 돌아갈 곳. DefaultEngine.ini 의 GameDefaultMap 과 같아야 함
	const FName MENU_MAP(TEXT("/Game/Maps/Lv_Lobby"));
	
	// 방 목록용 광고 키. 값은 전부 "1"/"0" 문자열 -> 빌드 태그와 같은 방식이라 스팀 필터가 확실히 먹는다
	const FName KEY_ROOM_NAME(TEXT("ROOMNAME"));
	const FName KEY_LOCKED(TEXT("LOCKED"));   // "1" 이면 비밀번호 방
	const FName KEY_LISTED(TEXT("LISTED"));   // "0" 이면 초대 전용. 검색에서 거름
	const FName KEY_INGAME(TEXT("INGAME"));   // "1" 이면 던전 진행 중. 검색에서 거름
	const FName KEY_REJOIN(TEXT("REJOIN"));   // "1" 이면 이공간 (진행 중에 나간 사람의 재합류 대기). 목록에 뜸

	// 스팀 OSS 는 로비 값을 UTF-8 로 쓰고 ANSI 로 읽는다 -> 한글이 깨짐 (영문은 둘이 같아서 멀쩡)
	// 방 이름은 UTF-8 바이트를 Base64 로 감싸서 ASCII 만 오가게 한다
	FString EncodeRoomNameForAd(const FString& InName)
	{
		const FTCHARToUTF8 Utf8(*InName);
		return FBase64::Encode(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	FString DecodeRoomNameFromAd(const FString& InEncoded)
	{
		TArray<uint8> Bytes;
		if (InEncoded.IsEmpty() || !FBase64::Decode(InEncoded, Bytes)) { return FString(); }

		const FUTF8ToTCHAR Wide(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Wide.Length(), Wide.Get());
	}

	// 스팀 로비 참가 결과 -> 화면에 띄울 사유
	FText JoinResultToText(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return FText::FromString(TEXT("방이 가득 찼습니다."));
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return FText::FromString(TEXT("방이 사라졌습니다. 목록을 새로고침해 보세요."));
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return FText::FromString(TEXT("주점 주소를 받지 못했습니다."));
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return FText::FromString(TEXT("이미 다른 방에 들어가 있습니다."));
		default:
			return FText::FromString(TEXT("주점에 들어가지 못했습니다."));
		}
	}
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

	// 참가한 쪽이 주점 맵을 다 불러왔으면 도착한 것 -> 참가 타이머 해제
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &USessionSubsystem::HandlePostLoadMap);

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

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

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

void USessionSubsystem::HostSession(int32 MaxPlayers, const FString& MapPath, const FTerminusRoomOptions& Options)
{
	// 만들어둔 헬퍼 함수로 접근
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		OnHostComplete.Broadcast(false);
		return;
	}

	// 비밀번호는 비어 있거나(잠금 없음) 규칙에 맞아야 한다
	if (!Options.Password.IsEmpty() && !IsValidRoomPassword(Options.Password))
	{
		UE_LOG(LogTerminusSession, Warning, TEXT("HostSession: 비밀번호 규칙 위반 (영문 숫자 1~16자)"));
		OnHostComplete.Broadcast(false);
		return;
	}

	// 이미 세션이 있으면 정리
	if (Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		PendingHostMap = MapPath;
		PendingMaxPlayers = MaxPlayers;
		PendingRoomOptions = Options;
		AfterDestroy = EAfterDestroy::Host;
		LeaveSession();
		return;
	}

	PendingHostMap = MapPath;
	HostPassword = Options.Password;
	HostMaxPlayers = MaxPlayers;
	
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch            = false;
	Settings.NumPublicConnections   = MaxPlayers;
	Settings.NumPrivateConnections  = 0;
	Settings.bShouldAdvertise       = true;
	// 스팀은 로비 인원이 바뀔 때마다 이 값으로 joinable 을 다시 계산한다 (false 면 검색, 초대 둘 다 막힘)
	// 주점에 있는 동안은 열어 두고, 출발할 때 StartRun 에서 닫는다
	Settings.bAllowJoinInProgress   = true;
	Settings.bAllowJoinViaPresence  = true;
	Settings.bUsesPresence          = true;
	Settings.bAllowInvites          = true;
	Settings.bUseLobbiesIfAvailable = true; 
	
	Settings.Set(KEY_BUILD_TAG, VALUE_BUILD_TAG,
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	// 방 목록용 광고. 비밀번호는 넣지 않고 잠김 여부만
	const auto Ad = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
	if (!Options.RoomName.IsEmpty())
	{
		Settings.Set(KEY_ROOM_NAME, EncodeRoomNameForAd(Options.RoomName.Left(24)), Ad);
	}
	Settings.Set(KEY_LOCKED, FString(Options.Password.IsEmpty() ? TEXT("0") : TEXT("1")), Ad);
	Settings.Set(KEY_LISTED, FString(Options.bListed ? TEXT("1") : TEXT("0")), Ad);
	Settings.Set(KEY_INGAME, FString(TEXT("0")), Ad);
	Settings.Set(KEY_REJOIN, FString(TEXT("0")), Ad);
	bRejoinListed = false;
	
	CreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleCreateComplete));

	UE_LOG(LogTerminusSession, Log, TEXT("HostSession: MaxPlayers=%d, MapPath='%s', Room='%s', Locked=%d, Listed=%d"),
	MaxPlayers, *MapPath, *Options.RoomName, Options.Password.IsEmpty() ? 0 : 1, Options.bListed ? 1 : 0);
	
	Session->CreateSession(0, NAME_GameSession, Settings);
}

bool USessionSubsystem::IsValidRoomPassword(const FString& InPassword)
{
	// 접속 URL 옵션(?pw=...)으로 넘어가서 ? = & 공백이 섞이면 잘린다 -> 영문 숫자만
	// FChar::IsAlnum 은 한글도 참이라 범위를 직접 본다
	if (InPassword.Len() < 1 || InPassword.Len() > 16)
	{
		return false;
	}
	for (const TCHAR C : InPassword)
	{
		const bool bDigit = (C >= TEXT('0') && C <= TEXT('9'));
		const bool bLower = (C >= TEXT('a') && C <= TEXT('z'));
		const bool bUpper = (C >= TEXT('A') && C <= TEXT('Z'));
		if (!bDigit && !bLower && !bUpper)
		{
			return false;
		}
	}
	return true;
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
	
	// 검색 중에 또 부르면 무시. 겹치면 완료 델리게이트가 두 번 붙어서 이후 검색마다 결과가 두 번 온다
	if (LastSearch.IsValid() && LastSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		UE_LOG(LogTerminusSession, Log, TEXT("FindSessions: 이미 검색 중이라 무시"));
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
	
	// 초대 전용 방과 던전 진행 중인 방은 스팀이 검색할 때 빼고 돌려준다
	LastSearch->QuerySettings.Set(KEY_LISTED, FString(TEXT("1")), EOnlineComparisonOp::Equals);
	LastSearch->QuerySettings.Set(KEY_INGAME, FString(TEXT("0")), EOnlineComparisonOp::Equals);

	FindHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(
			this, &USessionSubsystem::HandleFindComplete));
	
	UE_LOG(LogTerminusSession, Log, TEXT("FindSessions: MaxResults=%d, bUseBuildFilter=%d"),
		LastSearch->MaxSearchResults, bUseBuildFilter ? 1 : 0);
	Session->FindSessions(0, LastSearch.ToSharedRef());
}

void USessionSubsystem::JoinSessionByIndex(int32 Index, const FString& Password)
{
	// 인덱스는 BP 목록용 창구일 뿐, 실제 참가는 결과 자체로 한다
	if (!LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(Index))
	{
		FailJoin(FText::FromString(TEXT("목록이 바뀌었습니다. 새로고침해 보세요.")), false);
		return;
	}

	PendingTravelOptions = Password.IsEmpty() ? FString() : FString::Printf(TEXT("?pw=%s"), *Password);
	
	JoinSearchResult(LastSearch->SearchResults[Index]);
}

bool USessionSubsystem::CheckJoinRequest(const FString& Options, int32 CurrentPlayers, FString& OutError) const
{
	// 세션 없이 연 주점(솔로, PIE 에서 레벨 바로 열기)은 심사할 게 없다
	if (HostMaxPlayers <= 0)
	{
		return true;
	}

	// 목록이 새로고침 전 정보일 수 있어서 누르는 순간 찼을 수도 있다
	if (CurrentPlayers >= HostMaxPlayers)
	{
		OutError = TEXT("방이 가득 찼습니다.");
		return false;
	}

	if (HostPassword.IsEmpty())
	{
		return true;
	}

	if (UGameplayStatics::HasOption(Options, TEXT("invited")))
	{
		return true;
	}

	if (UGameplayStatics::ParseOption(Options, TEXT("pw")) != HostPassword)
	{
		OutError = TEXT("비밀번호가 틀렸습니다.");
		return false;
	}
	return true;
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
	
	if (FOnlineSessionSettings* Settings = Session->GetSessionSettings(NAME_GameSession))
	{
		Settings->Set(KEY_INGAME, FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		// 던전 중엔 로비도 닫는다. 이후 인원이 바뀌면 스팀이 이 값으로 joinable=false 를 유지
		Settings->bAllowJoinInProgress = false;

		Session->UpdateSession(NAME_GameSession, *Settings, true);
	}
}

void USessionSubsystem::SetRejoinListing(bool bOpen)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	FOnlineSessionSettings* Settings = Session.IsValid() ? Session->GetSessionSettings(NAME_GameSession) : nullptr;
	if (!Settings || bRejoinListed == bOpen)
	{
		return;
	}

	const auto Ad = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
	if (bOpen)
	{
		// 진행 중(INGAME=1)은 검색에서 빠지므로 0 으로, 초대 전용 방도 목록에 뜨게
		Settings->Get(KEY_LISTED, ListedBeforeRejoin);
		Settings->Set(KEY_LISTED, FString(TEXT("1")), Ad);
		Settings->Set(KEY_INGAME, FString(TEXT("0")), Ad);
		Settings->Set(KEY_REJOIN, FString(TEXT("1")), Ad);
		Settings->bAllowJoinInProgress = true;
	}
	else
	{
		Settings->Set(KEY_LISTED, ListedBeforeRejoin.IsEmpty() ? FString(TEXT("1")) : ListedBeforeRejoin, Ad);
		Settings->Set(KEY_INGAME, FString(TEXT("1")), Ad);
		Settings->Set(KEY_REJOIN, FString(TEXT("0")), Ad);
		Settings->bAllowJoinInProgress = false;
	}

	bRejoinListed = bOpen;
	UE_LOG(LogTerminusSession, Log, TEXT("SetRejoinListing: %d"), bOpen ? 1 : 0);
	Session->UpdateSession(NAME_GameSession, *Settings, true);
}

void USessionSubsystem::ShowInviteUI()
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	IOnlineExternalUIPtr UI = OSS ? OSS->GetExternalUIInterface() : nullptr;
	if (!UI.IsValid())
	{
		UE_LOG(LogTerminusSession, Warning, TEXT("ShowInviteUI: 외부 UI 인터페이스 없음"));
		return;
	}

	// 어느 세션으로 초대할지 넘겨야 스팀이 로비 초대 창을 띄운다
	if (!UI->ShowInviteUI(0, NAME_GameSession))
	{
		UE_LOG(LogTerminusSession, Warning, TEXT("ShowInviteUI: 실패 (세션이 없거나 오버레이 꺼짐)"));
	}
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
			FString EncodedName, Locked;
			R.Session.SessionSettings.Get(KEY_ROOM_NAME, EncodedName);
			R.Session.SessionSettings.Get(KEY_LOCKED, Locked);
			const FString RoomName = DecodeRoomNameFromAd(EncodedName);
			Info.RoomName = RoomName.IsEmpty() ? FString::Printf(TEXT("%s의 주점"), *Info.HostName) : RoomName;
			Info.bLocked  = (Locked == TEXT("1"));

			// 이공간: 나갔던 사람만 들어감 (던전 게임모드가 확인) -> 비밀번호는 묻지 않음
			FString Rejoin;
			R.Session.SessionSettings.Get(KEY_REJOIN, Rejoin);
			if (Rejoin == TEXT("1"))
			{
				Info.bRejoinWaiting = true;
				Info.bLocked = false;
				Info.RoomName += TEXT(" (재합류 대기)");
			}

			UE_LOG(LogTerminusSession, Log, TEXT("  [%d] '%s' %d/%d locked=%d ping=%d"),
				i, *Info.RoomName, Info.CurrentPlayers, Info.MaxPlayers, Info.bLocked ? 1 : 0, Info.PingMs);
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

	if (Result != EOnJoinSessionCompleteResult::Success || !Session.IsValid())
	{
		FailJoin(JoinResultToText(Result));
		return;
	}

	// 스팀 로비에는 들어갔다. 이제 호스트 주소로 접속해야 주점에 도착한다
	FString ConnectString;
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!Session->GetResolvedConnectString(NAME_GameSession, ConnectString) || !PC)
	{
		// 예전엔 여기서 조용히 끝나서 목록 화면이 "들어가는 중" 에 그대로 멈춰 있었음
		FailJoin(FText::FromString(TEXT("주점 주소를 받지 못했습니다. 다시 시도해 보세요.")));
		return;
	}

	// 비밀번호가 붙은 옵션은 로그에 남기지 않는다
	UE_LOG(LogTerminusSession, Log, TEXT("JoinComplete: %s 로 접속 시작"), *ConnectString);

	OnJoinComplete.Broadcast(true);
	PC->ClientTravel(ConnectString + PendingTravelOptions, ETravelType::TRAVEL_Absolute);
	PendingTravelOptions.Reset();

	// 타이머는 여기서 끄지 않는다. 접속 자체가 응답 없이 멈출 수 있어서 맵 로드(도착) 때 끈다
}

void USessionSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSessionInterface())
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	OnLeaveComplete.Broadcast(bWasSuccessful);

	HostPassword.Reset();
	HostMaxPlayers = 0;
	
	// 재진입 전에 먼저 내려야 함. 안 그러면 Host -> Leave -> Host ... 무한 루프
	const EAfterDestroy Next = AfterDestroy;
	AfterDestroy = EAfterDestroy::None;

	switch (Next)
	{
	case EAfterDestroy::Host:
		{
			const FString MapPath = PendingHostMap;
			PendingHostMap.Reset();

			if (bWasSuccessful) { HostSession(PendingMaxPlayers, MapPath, PendingRoomOptions); }
			else                { OnHostComplete.Broadcast(false); }
			break;
		}
	case EAfterDestroy::Join:
		// 파괴가 실패했는데 또 참가하면 세션이 그대로라 같은 곳을 무한히 돈다
		if (bWasSuccessful) { JoinSearchResult(PendingJoinResult); }
		else                { FailJoin(FText::FromString(TEXT("이전 방을 정리하지 못했습니다.")), false); }
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
		FailJoin(FText::FromString(TEXT("Steam에 연결되어 있지 않습니다.")), false);
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

	// 여기서부터 주점 도착까지 시간을 잰다 (이전 세션 정리 시간은 빼고)
	StartJoinTimeout();
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

	const FText Reason = (FailureType == ENetworkFailure::PendingConnectionFailure && !ErrorString.IsEmpty())
		? FText::FromString(ErrorString)
		: FText::FromString(TEXT("연결이 끊겼습니다."));

	CleanupAfterFailure(Reason);
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
	// 엔진이 실패를 알려 왔으니 참가 타이머는 더 볼 필요 없음
	ClearJoinTimeout();
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
		FailJoin(FText::FromString(TEXT("초대받은 방에 들어가지 못했습니다.")), false);
		return;
	}
	
	PendingTravelOptions = TEXT("?invited=1");

	// 내 주점을 열어둔 상태여도 JoinSearchResult 가 먼저 정리하고 들어간다
	JoinSearchResult(InviteResult);
}

void USessionSubsystem::FailJoin(const FText& Reason, bool bLeaveSession)
{
	ClearJoinTimeout();
	PendingTravelOptions.Reset();
	LastJoinError = Reason;

	UE_LOG(LogTerminusSession, Warning, TEXT("JoinFailed: %s"), *Reason.ToString());

	// 스팀 로비에는 들어가 있을 수 있음 -> 나와야 다음 참가가 막히지 않는다
	if (bLeaveSession)
	{
		IOnlineSessionPtr Session = GetSessionInterface();
		if (Session.IsValid() && Session->GetNamedSession(NAME_GameSession) != nullptr)
		{
			AfterDestroy = EAfterDestroy::None;
			LeaveSession();
		}
	}

	OnJoinComplete.Broadcast(false);
}

void USessionSubsystem::StartJoinTimeout()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI || JoinTimeoutSeconds <= 0.f) { return; }

	// 게임 인스턴스 타이머라 맵이 바뀌어도 살아 있다. 다시 부르면 처음부터 다시 잰다
	LastJoinError = FText::GetEmpty();
	GI->GetTimerManager().SetTimer(JoinTimeoutTimer, this, &USessionSubsystem::HandleJoinTimeout,
		JoinTimeoutSeconds, false);
}

void USessionSubsystem::ClearJoinTimeout()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(JoinTimeoutTimer);
	}
}

void USessionSubsystem::HandleJoinTimeout()
{
	UE_LOG(LogTerminusSession, Warning, TEXT("JoinTimeout: %.0f초 동안 주점에 도착하지 못함. 접속 취소"), JoinTimeoutSeconds);

	// 스팀 참가 응답이 늦게 오면 그때 ClientTravel 해 버리므로 먼저 끊어 둔다
	if (IOnlineSessionPtr Session = GetSessionInterface())
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	const FText Reason = FText::FromString(TEXT("주점이 응답하지 않습니다. 잠시 후 다시 시도해 보세요."));

	// 메뉴가 다시 뜰 때 팝업으로 보여줄 사유
	PendingDisconnectReason = Reason;

	// 로비에서 나오고 OnJoinComplete(false)
	FailJoin(Reason);

	// ClientTravel 뒤 호스트와 인사 중에 멈춰 있으면 엔진은 꽤 오래 기다린다 -> 우리가 끊는다
	// 메뉴 맵을 다시 열면 엔진이 대기 중인 접속(PendingNetGame)을 알아서 취소한다
	// CancelPending 은 UEngine 의 protected 라 직접 못 부름
	TravelToMenu();
}

void USessionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 전역 이벤트라 PIE 다중 창이면 남의 맵 로드도 옴
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance()) { return; }

	ClearJoinTimeout();
}
