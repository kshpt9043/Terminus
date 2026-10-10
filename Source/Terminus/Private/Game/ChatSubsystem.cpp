#include "Game/ChatSubsystem.h"

#include "Game/TerminusRunSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Chat/ChatWidget.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Game/LoadingScreenSubsystem.h"
#include "Widgets/SWindow.h"

// Slate 가 키를 위젯에 나눠주기 전에 먼저 보는 곳. Enter 만 골라서 서브시스템에 넘김
class FChatInputProcessor : public IInputProcessor
{
public:
	explicit FChatInputProcessor(UChatSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key != EKeys::Enter || InKeyEvent.IsRepeat()) return false;

		UChatSubsystem* Subsystem = Owner.Get();
		return Subsystem && Subsystem->HandleEnter();   // true 면 키를 먹음
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TerminusChat"); }

private:
	TWeakObjectPtr<UChatSubsystem> Owner;
};

void UChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FChatInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
}

void UChatSubsystem::Deinitialize()
{
	if (Processor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	}
	Processor.Reset();

	Super::Deinitialize();
}

UChatSubsystem* UChatSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UChatSubsystem>() : nullptr;
}

void UChatSubsystem::AddMessage(const FChatMessage& InMessage)
{
	// 시간이 안 찍힌 줄은 이 컴퓨터 시계로 (서버가 보낸 줄은 서버 시계로 이미 찍혀 있음)
	FChatMessage Message = InMessage;
	if (Message.Time.IsEmpty())
	{
		Message.Time = FDateTime::Now().ToString(TEXT("%H:%M"));
	}

	History.Add(Message);
	if (History.Num() > MaxHistory)
	{
		History.RemoveAt(0, History.Num() - MaxHistory);
	}

	OnMessageAdded.Broadcast(Message);
}

void UChatSubsystem::BeginServerLog(const FString& RoomName)
{
    // 파일 이름에 못 쓰는 글자는 _ 로
    FString SafeName = RoomName.IsEmpty() ? FString(TEXT("Room")) : RoomName;
    for (const TCHAR Bad : FString(TEXT("\\/:*?\"<>| ")))
    {
        SafeName.ReplaceCharInline(Bad, TEXT('_'));
    }

    const FDateTime Now = FDateTime::Now();
    ServerLogPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChatLogs"),
        FString::Printf(TEXT("Chat_%s_%s.txt"), *Now.ToString(TEXT("%Y%m%d_%H%M%S")), *SafeName));

    // 머리말 (메모장에서 한글이 안 깨지게 BOM 있는 UTF-8 로 시작, 이후 줄은 덧붙이기)
    const FString Header = FString::Printf(TEXT("Terminus 채팅 기록\r\n방: %s\r\n시작: %s\r\n----------------------------------------\r\n"),
        *RoomName, *Now.ToString(TEXT("%Y-%m-%d %H:%M:%S")));
    FFileHelper::SaveStringToFile(Header, *ServerLogPath, FFileHelper::EEncodingOptions::ForceUTF8);
}

void UChatSubsystem::WriteServerLog(const UWorld* World, const FChatMessage& Message)
{
    if (!World) return;

    const ENetMode Mode = World->GetNetMode();
    if (Mode != NM_ListenServer && Mode != NM_DedicatedServer) return;   // 싱글 / 클라는 안 남김

    if (ServerLogPath.IsEmpty())
    {
        const UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr;
        BeginServerLog(Run ? Run->GetRoomName() : FString());
    }

    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    const FString Line = Message.Kind == EChatMessageKind::System
        ? FString::Printf(TEXT("[%s] [안내] %s\r\n"), *Stamp, *Message.Text)
        : FString::Printf(TEXT("[%s] %s: %s\r\n"), *Stamp, *Message.Sender, *Message.Text);

    FFileHelper::SaveStringToFile(Line, *ServerLogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

void UChatSubsystem::AddLocalMessage(const UWorld* World, const FChatMessage& InMessage)
{
    FChatMessage Message = InMessage;
    if (Message.Time.IsEmpty())
    {
        Message.Time = FDateTime::Now().ToString(TEXT("%H:%M"));
    }

    AddMessage(Message);
    WriteServerLog(World, Message);   // 방장 PC 일 때만 실제로 씀
}

void UChatSubsystem::ClearHistory()
{
    History.Reset();
    OnHistoryReplaced.Broadcast();
}

void UChatSubsystem::SetActiveWidget(UChatWidget* InWidget)
{
	ActiveWidget = InWidget;
}

void UChatSubsystem::ClearActiveWidget(UChatWidget* InWidget)
{
	if (ActiveWidget.Get() == InWidget)
	{
		ActiveWidget.Reset();
	}
}

bool UChatSubsystem::HandleEnter()
{
	UChatWidget* Widget = ActiveWidget.Get();
	if (!Widget || !Widget->IsInViewport() || !Widget->IsVisible() || Widget->IsInputOpen()) return false;
	if (!IsOwnWindowActive()) return false;

	// 다른 입력칸(주점 비밀번호 등)에 글을 쓰는 중이면 그쪽 Enter
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (Focused.IsValid())
	{
		const FName Type = Focused->GetType();
		if (Type == TEXT("SEditableText") || Type == TEXT("SMultiLineEditableText"))
		{
			return false;
		}
	}

	// 로딩 화면이 덮고 있으면 채팅 안 엶
	if (const ULoadingScreenSubsystem* Loading = ULoadingScreenSubsystem::Get(Widget); Loading && Loading->IsShowing())
	{
		return false;
	}

	// 팝업 / 창이 떠 있으면 그쪽 Enter (확인 버튼 등)
	if (const UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(Widget); Escape && Escape->HasOpenEntry())
	{
		return false;
	}

	Widget->OpenInput();
	return true;
}

bool UChatSubsystem::IsOwnWindowActive() const
{
	const UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	if (!Window.IsValid()) return true;

	return FSlateApplication::Get().GetActiveTopLevelWindow() == Window;
}
