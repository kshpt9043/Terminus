#include "Widgets/Common/EscapeStackSubsystem.h"

#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

// Slate 가 키를 위젯에 나눠주기 전에 먼저 보는 곳. ESC 만 골라서 서브시스템에 넘김
class FEscapeInputProcessor : public IInputProcessor
{
public:
	explicit FEscapeInputProcessor(UEscapeStackSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() != EKeys::Escape || InKeyEvent.IsRepeat()) return false;

		UEscapeStackSubsystem* Subsystem = Owner.Get();
		return Subsystem && Subsystem->HandleEscape();   // true 면 키를 먹음
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TerminusEscapeStack"); }

private:
	TWeakObjectPtr<UEscapeStackSubsystem> Owner;
};

void UEscapeStackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FEscapeInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
}

void UEscapeStackSubsystem::Deinitialize()
{
	if (Processor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	}
	Processor.Reset();
	Stack.Reset();

	Super::Deinitialize();
}

UEscapeStackSubsystem* UEscapeStackSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEscapeStackSubsystem>() : nullptr;
}

void UEscapeStackSubsystem::Push(UWidget* Owner, FSimpleDelegate OnEscape)
{
	if (!Owner) return;

	Remove(Owner);
	Stack.Add({ Owner, MoveTemp(OnEscape) });
}

void UEscapeStackSubsystem::Remove(UWidget* Owner)
{
	Stack.RemoveAll([Owner](const FEntry& Entry) { return !Entry.Owner.IsValid() || Entry.Owner.Get() == Owner; });
}

bool UEscapeStackSubsystem::HandleEscape()
{
	// 사라졌거나 숨겨진 창은 건너뜀 (닫을 때 Remove 를 빼먹었어도 안전하게)
	Stack.RemoveAll([](const FEntry& Entry) { return !Entry.Owner.IsValid(); });

	for (int32 i = Stack.Num() - 1; i >= 0; --i)
	{
		const UWidget* Owner = Stack[i].Owner.Get();
		if (!Owner->IsVisible() || !Owner->GetCachedWidget().IsValid()) continue;

		if (!IsOwnWindowActive()) return false;

		// 부르는 쪽에서 Remove 할 수 있으니 복사해서 부름
		const FSimpleDelegate OnEscape = Stack[i].OnEscape;
		OnEscape.ExecuteIfBound();
		return true;
	}
	return false;
}

bool UEscapeStackSubsystem::HasOpenEntry() const
{
	for (const FEntry& Entry : Stack)
	{
		const UWidget* Owner = Entry.Owner.Get();
		if (Owner && Owner->IsVisible() && Owner->GetCachedWidget().IsValid()) return true;
	}
	return false;
}

bool UEscapeStackSubsystem::IsOwnWindowActive() const
{
	const UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	if (!Window.IsValid()) return true;

	return FSlateApplication::Get().GetActiveTopLevelWindow() == Window;
}
