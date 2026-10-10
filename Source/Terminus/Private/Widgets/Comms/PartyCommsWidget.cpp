#include "Widgets/Comms/PartyCommsWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/TerminusBattler.h"
#include "Character/TerminusMonster.h"
#include "Combat/CombatStatsComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonArea.h"
#include "Dungeon/DungeonCombatComponent.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/ChatSubsystem.h"
#include "Game/LoadingScreenSubsystem.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Comms/CommsMenuWidget.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Widgets/SWindow.h"

// Slate 가 키 / 클릭을 위젯에 나눠주기 전에 먼저 보는 곳. 판단은 위젯이 함
class FPartyCommsInputProcessor : public IInputProcessor
{
public:
	explicit FPartyCommsInputProcessor(UPartyCommsWidget* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.IsRepeat()) return false;
		UPartyCommsWidget* Widget = Owner.Get();
		return Widget && Widget->HandleKeyDown(InKeyEvent);
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		UPartyCommsWidget* Widget = Owner.Get();
		return Widget && Widget->HandleMouseDown(MouseEvent);
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TerminusPartyComms"); }

private:
	TWeakObjectPtr<UPartyCommsWidget> Owner;
};

namespace
{
	const FLinearColor CommsPlanColor(0.75f, 0.9f, 1.f);
	const FLinearColor CommsEstimateColor(1.f, 0.85f, 0.5f);
	const FLinearColor CommsLethalColor(1.f, 0.35f, 0.3f);

	// 유니티 빌드에서 다른 파일의 MakeText 와 겹치지 않게 이름을 따로
	UTextBlock* MakeCommsText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}

	// 같은 글자면 다시 안 넣음 (매 틱 SetText 로 레이아웃이 다시 잡히지 않게)
	void SetCommsText(UTextBlock* Text, const FString& Value)
	{
		if (!Text) return;
		if (Text->GetText().ToString() != Value)
		{
			Text->SetText(FText::FromString(Value));
		}
		const ESlateVisibility Wanted = Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
		if (Text->GetVisibility() != Wanted)
		{
			Text->SetVisibility(Wanted);
		}
	}

	const ATerminusPlayerState* GetBattlerPS(const ATerminusBattler* Battler)
	{
		return Battler ? Battler->GetPlayerState<ATerminusPlayerState>() : nullptr;
	}

	bool IsAliveBattler(const ATerminusBattler* Battler)
	{
		const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
		return Stats && !Stats->IsDead() && !Battler->IsHidden();
	}

	bool IsOccupant(const ADungeonArea* Area, const ATerminusPlayerState* PS)
	{
		return Area && PS && Area->GetOccupants().ContainsByPredicate(
			[PS](const TObjectPtr<ATerminusPlayerState>& P) { return P.Get() == PS; });
	}
}

// =====================================================================
// 초기화
// =====================================================================

void UPartyCommsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UPartyCommsWidget::BuildLayout()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CommsRoot"));
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 재촉 / 알림 (위 가운데)
	ToastText = MakeCommsText(WidgetTree, FontSize + 7, FLinearColor(1.f, 0.85f, 0.4f));
	if (UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastText))
	{
		ToastSlot->SetAnchors(FAnchors(0.5f, 0.f));
		ToastSlot->SetAlignment(FVector2D(0.5f, 0.f));
		ToastSlot->SetPosition(FVector2D(0.f, 90.f));
		ToastSlot->SetAutoSize(true);
	}
	ToastText->SetVisibility(ESlateVisibility::Collapsed);

	// 재촉 버튼 (오른쪽 아래)
	NudgeButton = WidgetTree->ConstructWidget<UButton>();
	NudgeButton->SetBackgroundColor(FLinearColor(0.85f, 0.55f, 0.2f, 1.f));
	NudgeLabel = MakeCommsText(WidgetTree, FontSize + 3, FLinearColor::White);
	NudgeButton->SetContent(NudgeLabel);
	NudgeButton->SetToolTipText(NSLOCTEXT("PartyComms", "NudgeTip", "아직 턴을 끝내지 않은 동료에게 알림을 보냅니다"));
	NudgeButton->OnClicked.AddDynamic(this, &UPartyCommsWidget::HandleNudgeClicked);
	if (UCanvasPanelSlot* NudgeSlot = Root->AddChildToCanvas(NudgeButton))
	{
		NudgeSlot->SetAnchors(FAnchors(1.f, 1.f));
		NudgeSlot->SetAlignment(FVector2D(1.f, 1.f));
		NudgeSlot->SetPosition(NudgeButtonPosition);
		NudgeSlot->SetAutoSize(true);
	}
	NudgeButton->SetVisibility(ESlateVisibility::Collapsed);

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UPartyCommsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// NativeConstruct 는 여러 번 불릴 수 있음 -> 한 번만 등록
	if (!Processor.IsValid() && FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FPartyCommsInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
}

void UPartyCommsWidget::NativeDestruct()
{
	if (Processor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	}
	Processor.Reset();

	if (Menu)
	{
		Menu->Close();
	}

	Super::NativeDestruct();
}

UCommsMenuWidget* UPartyCommsWidget::GetMenu()
{
	if (!Menu)
	{
		const TSubclassOf<UCommsMenuWidget> Class = MenuClass ? MenuClass : TSubclassOf<UCommsMenuWidget>(UCommsMenuWidget::StaticClass());
		Menu = CreateWidget<UCommsMenuWidget>(GetOwningPlayer(), Class);
	}
	return Menu;
}

// =====================================================================
// 받은 것
// =====================================================================

void UPartyCommsWidget::HandlePlan(const FCombatPlan& Plan)
{
	if (!Plan.Planner) return;

	if (Plan.IsValid())
	{
		Plans.Add(TWeakObjectPtr<ATerminusPlayerState>(Plan.Planner.Get()), Plan);
	}
	else
	{
		Plans.Remove(TWeakObjectPtr<ATerminusPlayerState>(Plan.Planner.Get()));
	}
}

void UPartyCommsWidget::HandlePing(const FPartyPing& Ping)
{
	if (!Ping.Sender || !Ping.Target) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	// 한 사람당 핑 하나. 새로 찍으면 이전 것은 지움
	Pings.RemoveAll([&Ping](const FActivePing& P) { return P.Ping.Sender.Get() == Ping.Sender.Get(); });

	FActivePing& Added = Pings.AddDefaulted_GetRef();
	Added.Ping = Ping;
	Added.ExpireTime = World->GetTimeSeconds() + PingDuration;

	// 채팅창에도 한 줄 (화면을 놓쳐도 남게)
	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		FChatMessage Line;
		Line.Kind = EChatMessageKind::System;
		Line.Text = FString::Printf(TEXT("[핑] %s -> %s: %s"),
			*Ping.Sender->GetPlayerName(), *DescribeTarget(Ping.Target), *PartyComms::GetPingText(Ping.Kind).ToString());
		Chat->AddMessage(Line);
	}
}

void UPartyCommsWidget::HandleQuickChat(ATerminusPlayerState* Sender, EQuickChat Kind)
{
	if (!Sender) return;

	const FText Text = PartyComms::GetQuickChatText(Kind);
	if (Text.IsEmpty()) return;

	// 채팅창에 보통 말처럼 한 줄
	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		FChatMessage Line;
		Line.Kind = EChatMessageKind::Player;
		Line.Sender = Sender->GetPlayerName();
		Line.Text = Text.ToString();
		Chat->AddMessage(Line);
	}

	// 보낸 사람이 지금 화면에 있으면 머리 위 말풍선
	if (const UWorld* World = GetWorld())
	{
		Bubbles.RemoveAll([Sender](const FBubble& B) { return B.Sender.Get() == Sender; });
		FBubble& Bubble = Bubbles.AddDefaulted_GetRef();
		Bubble.Sender = Sender;
		Bubble.Text = Text;
		Bubble.ExpireTime = World->GetTimeSeconds() + BubbleDuration;
	}
}

void UPartyCommsWidget::HandleNudged(ATerminusPlayerState* Sender)
{
	const FString Name = Sender ? Sender->GetPlayerName() : TEXT("?");
	ShowToast(FText::FromString(FString::Printf(TEXT("%s 님이 기다리고 있어요! 턴을 끝내 주세요"), *Name)));

	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		FChatMessage Line;
		Line.Kind = EChatMessageKind::System;
		Line.Text = FString::Printf(TEXT("%s 님이 재촉합니다."), *Name);
		Chat->AddMessage(Line);
	}
}

void UPartyCommsWidget::ShowToast(const FText& Message)
{
	const UWorld* World = GetWorld();
	if (!ToastText || !World) return;

	ToastText->SetText(Message);
	ToastText->SetVisibility(ESlateVisibility::HitTestInvisible);
	ToastExpireTime = World->GetTimeSeconds() + ToastDuration;
}

// =====================================================================
// 입력
// =====================================================================

bool UPartyCommsWidget::CanTakeKeyboard() const
{
	if (!IsInViewport()) return false;

	// 이 게임 창이 활성 창일 때만 (PIE 창이 여러 개면 남의 창 키는 무시)
	const UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	if (Window.IsValid() && FSlateApplication::Get().GetActiveTopLevelWindow() != Window) return false;

	// 채팅 / 비밀번호 같은 입력칸에 글을 쓰는 중이면 그쪽 키
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (Focused.IsValid())
	{
		const FName Type = Focused->GetType();
		if (Type == TEXT("SEditableText") || Type == TEXT("SMultiLineEditableText"))
		{
			return false;
		}
	}

	if (const ULoadingScreenSubsystem* Loading = ULoadingScreenSubsystem::Get(this); Loading && Loading->IsShowing())
	{
		return false;
	}
	return true;
}

bool UPartyCommsWidget::IsOwnWindowUnder(const FVector2D& ScreenSpacePosition) const
{
	if (!IsInViewport()) return false;

	const UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	if (!Window.IsValid()) return true;

	return FSlateApplication::Get().GetActiveTopLevelWindow() == Window && Window->GetRectInScreen().ContainsPoint(ScreenSpacePosition);
}

bool UPartyCommsWidget::HandleKeyDown(const FKeyEvent& InKeyEvent)
{
	if (!CanTakeKeyboard()) return false;

	const FKey Key = InKeyEvent.GetKey();

	// 메뉴가 떠 있으면 숫자 키로 고르기, 같은 키로 닫기 (ESC 는 ESC 목록이 처리)
	if (Menu && Menu->IsOpen())
	{
		static const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		static const FKey NumPadKeys[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
		for (int32 i = 0; i < UE_ARRAY_COUNT(NumberKeys); ++i)
		{
			if (Key == NumberKeys[i] || Key == NumPadKeys[i])
			{
				return Menu->PickByNumber(i + 1);
			}
		}
		if (Key == QuickChatKey)
		{
			Menu->Close();
			return true;
		}
		return false;
	}

	if (Key != QuickChatKey) return false;

	// 팝업 / 다른 창이 떠 있으면 그쪽 우선
	if (const UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this); Escape && Escape->HasOpenEntry())
	{
		return false;
	}

	OpenQuickChatMenu();
	return true;
}

bool UPartyCommsWidget::HandleMouseDown(const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
	if (!IsOwnWindowUnder(ScreenPos)) return false;

	const FKey Button = InMouseEvent.GetEffectingButton();
	const bool bPingGesture = Button == EKeys::MiddleMouseButton
		|| (bAltClickPing && Button == EKeys::LeftMouseButton && InMouseEvent.IsAltDown());

	// 메뉴 밖을 누르면 메뉴 닫기. 그 클릭은 원래 하려던 일(버튼 등)에 그대로 감
	if (Menu && Menu->IsOpen() && !Menu->IsOverPanel(ScreenPos))
	{
		Menu->Close();
	}

	if (!bPingGesture) return false;

	ATerminusBattler* Target = FindPingTarget(ScreenPos);
	if (!Target) return false;

	OpenPingMenu(Target, GetCachedGeometry().AbsoluteToLocal(ScreenPos));
	return true;
}

ATerminusBattler* UPartyCommsWidget::FindPingTarget(const FVector2D& ScreenSpacePosition) const
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	ADungeonArea* Area = PC ? PC->GetViewedArea() : nullptr;
	UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr;

	// 내가 들어 있는 구역의 싸움 중에만 (관전 중엔 못 찍음)
	if (!Combat || !IsOccupant(Area, LocalPS)) return nullptr;
	const ECombatPhase Phase = Combat->GetPhase();
	if (Phase != ECombatPhase::PlayerTurn && Phase != ECombatPhase::MonsterTurn) return nullptr;

	const FVector2D Local = GetCachedGeometry().AbsoluteToLocal(ScreenSpacePosition);

	TArray<ATerminusBattler*> Battlers;
	GatherBattlers(Area, Combat, Battlers);

	ATerminusBattler* Best = nullptr;
	float BestDistSq = FMath::Square(PickRadius);
	for (ATerminusBattler* Battler : Battlers)
	{
		if (!IsAliveBattler(Battler)) continue;

		FVector2D BodyPos;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Battler->GetActorLocation() + FVector(0.f, 0.f, BodyCenterHeight), BodyPos, false))
		{
			continue;
		}

		const float DistSq = FVector2D::DistSquared(BodyPos, Local);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Battler;
		}
	}
	return Best;
}

void UPartyCommsWidget::OpenPingMenu(ATerminusBattler* Target, const FVector2D& LocalPosition)
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const ADungeonArea* Area = PC ? PC->GetViewedArea() : nullptr;
	const UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr;
	UCommsMenuWidget* PingMenu = GetMenu();
	if (!PC || !Combat || !PingMenu) return;

	// 적이면 집중 공격 / 위험, 아군(나 포함)이면 보호 필요 / 회복 필요
	TArray<EPingKind> Kinds;
	if (IsEnemyOf(Combat, LocalPS, Target))
	{
		Kinds = { EPingKind::Focus, EPingKind::Danger };
	}
	else
	{
		Kinds = { EPingKind::NeedGuard, EPingKind::NeedHeal };
	}

	TArray<FText> Labels;
	for (const EPingKind Kind : Kinds)
	{
		Labels.Add(PartyComms::GetPingText(Kind));
	}

	const TWeakObjectPtr<ATerminusBattler> WeakTarget = Target;
	const TWeakObjectPtr<ATerminusPlayerController> WeakPC = PC;
	PingMenu->Open(FText::FromString(FString::Printf(TEXT("핑: %s"), *DescribeTarget(Target))), Labels, LocalPosition,
		FOnCommsMenuPicked::CreateLambda([WeakPC, WeakTarget, Kinds](int32 Index)
		{
			if (ATerminusPlayerController* OwnerPC = WeakPC.Get(); OwnerPC && WeakTarget.IsValid() && Kinds.IsValidIndex(Index))
			{
				OwnerPC->Server_SendPing(Kinds[Index], WeakTarget.Get());
			}
		}));
}

void UPartyCommsWidget::OpenQuickChatMenu()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	UCommsMenuWidget* QuickMenu = GetMenu();
	if (!PC || !QuickMenu) return;

	TArray<FText> Labels;
	for (int32 i = 0; i < static_cast<int32>(EQuickChat::MAX); ++i)
	{
		Labels.Add(PartyComms::GetQuickChatText(static_cast<EQuickChat>(i)));
	}

	const TWeakObjectPtr<ATerminusPlayerController> WeakPC = PC;
	QuickMenu->Open(NSLOCTEXT("PartyComms", "QuickChatTitle", "퀵챗 (숫자 키)"), Labels, UWidgetLayoutLibrary::GetMousePositionOnViewport(this),
		FOnCommsMenuPicked::CreateLambda([WeakPC](int32 Index)
		{
			if (ATerminusPlayerController* OwnerPC = WeakPC.Get())
			{
				OwnerPC->Server_SendQuickChat(static_cast<EQuickChat>(Index));
			}
		}));
}

void UPartyCommsWidget::HandleNudgeClicked()
{
	const UWorld* World = GetWorld();
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!World || !PC || World->GetTimeSeconds() < NextNudgeTime) return;

	NextNudgeTime = World->GetTimeSeconds() + NudgeCooldown;
	PC->Server_Nudge();
}

// =====================================================================
// 매 틱: 머리 위 표시 / 재촉 버튼 / 알림
// =====================================================================

void UPartyCommsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* World = GetWorld();
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!World || !PC) return;

	const double Now = World->GetTimeSeconds();

	// 시간이 다 된 것 / 주인이 사라진 것 정리
	Pings.RemoveAll([Now](const FActivePing& P) { return P.ExpireTime <= Now || !P.Ping.Sender || !P.Ping.Target; });
	Bubbles.RemoveAll([Now](const FBubble& B) { return B.ExpireTime <= Now || !B.Sender.IsValid(); });
	for (auto It = Plans.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	if (ToastText && ToastExpireTime > 0.0 && Now >= ToastExpireTime)
	{
		ToastExpireTime = 0.0;
		ToastText->SetVisibility(ESlateVisibility::Collapsed);
	}

	ADungeonArea* Area = PC->GetViewedArea();
	UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr;

	// 배틀러 구성이 바뀌면(새 방, 복제 도착) 표시를 다시 만듦
	TArray<ATerminusBattler*> Battlers;
	GatherBattlers(Area, Combat, Battlers);

	bool bChanged = Battlers.Num() != TaggedBattlers.Num();
	for (int32 i = 0; !bChanged && i < Battlers.Num(); ++i)
	{
		bChanged = TaggedBattlers[i].Get() != Battlers[i];
	}
	if (bChanged)
	{
		RebuildTags(Battlers);
	}

	UpdateTags(Area, Combat);
	UpdateNudgeButton(Area, Combat);
}

void UPartyCommsWidget::GatherBattlers(ADungeonArea* Area, UDungeonCombatComponent* Combat, TArray<ATerminusBattler*>& Out) const
{
	if (!Area) return;

	if (Combat && Combat->IsInCombat())
	{
		for (ATerminusMonster* Monster : Combat->GetMonsters())
		{
			if (Monster) Out.Add(Monster);
		}
	}
	for (ATerminusPlayerState* PS : Area->GetOccupants())
	{
		if (ATerminusBattler* Battler = PS ? Cast<ATerminusBattler>(PS->GetPawn()) : nullptr)
		{
			Out.Add(Battler);
		}
	}
}

void UPartyCommsWidget::RebuildTags(const TArray<ATerminusBattler*>& Battlers)
{
	for (const FCommsTag& Tag : Tags)
	{
		if (UVerticalBox* Box = Tag.Box.Get()) Box->RemoveFromParent();
	}
	Tags.Reset();
	TaggedBattlers.Reset();

	if (!Root) return;

	for (ATerminusBattler* Battler : Battlers)
	{
		FCommsTag Tag;
		Tag.Battler = Battler;

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 말풍선 (밝은 바탕 + 어두운 글자)
		UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>();
		Bubble->SetBrushColor(FLinearColor(0.95f, 0.93f, 0.88f, 0.95f));
		Bubble->SetPadding(FMargin(10.f, 4.f));
		UTextBlock* BubbleText = MakeCommsText(WidgetTree, FontSize + 1, FLinearColor(0.08f, 0.07f, 0.06f));
		BubbleText->SetShadowOffset(FVector2D::ZeroVector);
		Bubble->SetContent(BubbleText);
		Bubble->SetVisibility(ESlateVisibility::Collapsed);

		UTextBlock* PingText = MakeCommsText(WidgetTree, FontSize + 1, FLinearColor::White);
		UTextBlock* PlanText = MakeCommsText(WidgetTree, FontSize - 1, CommsPlanColor);
		UTextBlock* EstimateText = MakeCommsText(WidgetTree, FontSize, CommsEstimateColor);

		for (UWidget* Child : TArray<UWidget*>{ Bubble, PingText, PlanText, EstimateText })
		{
			if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Child))
			{
				S->SetHorizontalAlignment(HAlign_Center);
				S->SetPadding(FMargin(0.f, 1.f));
			}
		}
		PingText->SetVisibility(ESlateVisibility::Collapsed);
		PlanText->SetVisibility(ESlateVisibility::Collapsed);
		EstimateText->SetVisibility(ESlateVisibility::Collapsed);

		if (UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box))
		{
			BoxSlot->SetAutoSize(true);
			BoxSlot->SetAlignment(FVector2D(0.5f, 1.f));
		}

		Tag.Box = Box;
		Tag.Bubble = Bubble;
		Tag.BubbleText = BubbleText;
		Tag.PingText = PingText;
		Tag.PlanText = PlanText;
		Tag.EstimateText = EstimateText;
		Tags.Add(Tag);
		TaggedBattlers.Add(Battler);
	}
}

void UPartyCommsWidget::UpdateTags(ADungeonArea* Area, UDungeonCombatComponent* Combat)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	const bool bFighting = Combat && Combat->IsInCombat()
		&& (Combat->GetPhase() == ECombatPhase::PlayerTurn || Combat->GetPhase() == ECombatPhase::MonsterTurn);

	for (FCommsTag& Tag : Tags)
	{
		ATerminusBattler* Battler = Tag.Battler.Get();
		UVerticalBox* Box = Tag.Box.Get();
		if (!Box) continue;

		if (!Battler || Battler->IsHidden())
		{
			Box->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const bool bAlive = IsAliveBattler(Battler);
		const ATerminusPlayerState* BattlerPS = GetBattlerPS(Battler);

		// ---- 말풍선 (플레이어만, 쓰러져도 보임)
		const FBubble* Bubble = BattlerPS ? Bubbles.FindByPredicate([BattlerPS](const FBubble& B) { return B.Sender.Get() == BattlerPS; }) : nullptr;
		if (UBorder* BubbleBorder = Tag.Bubble.Get())
		{
			BubbleBorder->SetVisibility(Bubble ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (Bubble) SetCommsText(Tag.BubbleText.Get(), Bubble->Text.ToString());
		}

		// ---- 핑 (싸우는 중, 살아 있는 대상)
		FString PingLabel;
		FLinearColor PingColor = FLinearColor::White;
		if (bFighting && bAlive)
		{
			for (const FActivePing& Active : Pings)
			{
				if (Active.Ping.Target.Get() != Battler) continue;
				if (!PingLabel.IsEmpty()) PingLabel += TEXT("\n");
				PingLabel += FString::Printf(TEXT("[%s] %s"), *PartyComms::GetPingText(Active.Ping.Kind).ToString(), *Active.Ping.Sender->GetPlayerName());
				PingColor = PartyComms::GetPingColor(Active.Ping.Kind);   // 여러 개면 마지막 핑 색
			}
		}
		if (UTextBlock* PingText = Tag.PingText.Get())
		{
			PingText->SetColorAndOpacity(FSlateColor(PingColor));
			SetCommsText(PingText, PingLabel);
		}

		// ---- 계획 / 예상 피해
		FString PlanLabel;
		int32 EstimatedDamage = 0;
		if (bAlive && Combat && Combat->GetPhase() == ECombatPhase::PlayerTurn)
		{
			const UCombatStatsComponent* TargetStats = Battler->GetCombatStats();
			for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, FCombatPlan>& Pair : Plans)
			{
				const FCombatPlan& Plan = Pair.Value;
				if (!IsPlanShown(Plan, Area, Combat) || !PlanTouches(Plan, Combat, Battler)) continue;

				if (!PlanLabel.IsEmpty()) PlanLabel += TEXT("\n");
				PlanLabel += DescribePlan(Plan);

				if (Plan.Effect == ECombatPlanEffect::Damage && IsEnemyOf(Combat, Plan.Planner, Battler))
				{
					const ATerminusBattler* Caster = Cast<ATerminusBattler>(Plan.Planner->GetPawn());
					EstimatedDamage += PartyComms::EstimateHit(Plan.Amount, Caster ? Caster->GetCombatStats() : nullptr, TargetStats) * FMath::Max(1, Plan.HitCount);
				}
			}

			// 대상을 고르는 중인 사람은 자기 머리 위에 "(대상 고르는 중)"
			for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, FCombatPlan>& Pair : Plans)
			{
				const FCombatPlan& Plan = Pair.Value;
				const bool bChoosing = !Plan.Target && (Plan.TargetType == ETargetType::SingleEnemy || Plan.TargetType == ETargetType::SingleAlly);
				if (!bChoosing || Plan.Planner.Get() != BattlerPS || !IsPlanShown(Plan, Area, Combat)) continue;

				if (!PlanLabel.IsEmpty()) PlanLabel += TEXT("\n");
				PlanLabel += DescribePlan(Plan);
			}
		}
		SetCommsText(Tag.PlanText.Get(), PlanLabel);

		FString EstimateLabel;
		bool bLethal = false;
		if (EstimatedDamage > 0)
		{
			const FCombatState& State = Battler->GetCombatStats()->GetCombatState();
			const int32 Remaining = State.Health + State.Shield;
			bLethal = EstimatedDamage >= Remaining;
			EstimateLabel = State.Shield > 0
				? FString::Printf(TEXT("예상 피해 약 %d / 체력 %d (+%d)"), EstimatedDamage, State.Health, State.Shield)
				: FString::Printf(TEXT("예상 피해 약 %d / 체력 %d"), EstimatedDamage, State.Health);
			if (bLethal) EstimateLabel += TEXT("  처치!");
		}
		if (UTextBlock* EstimateText = Tag.EstimateText.Get())
		{
			EstimateText->SetColorAndOpacity(FSlateColor(bLethal ? CommsLethalColor : CommsEstimateColor));
			SetCommsText(EstimateText, EstimateLabel);
		}

		// ---- 보일지 / 자리
		const bool bAnything = Bubble || !PingLabel.IsEmpty() || !PlanLabel.IsEmpty() || !EstimateLabel.IsEmpty();
		Box->SetVisibility(bAnything ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bAnything) continue;

		FVector2D HeadPos;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Battler->GetActorLocation() + FVector(0.f, 0.f, HeadHeight), HeadPos, false))
		{
			if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(Box->Slot))
			{
				BoxSlot->SetPosition(HeadPos - FVector2D(0.f, HeadScreenOffset));
			}
		}
	}
}

void UPartyCommsWidget::UpdateNudgeButton(ADungeonArea* Area, UDungeonCombatComponent* Combat)
{
	if (!NudgeButton) return;

	const ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 내 구역의 플레이어 턴이고, 나는 할 게 없는데(끝냈거나 상대 편 차례이거나 쓰러짐) 아직 안 끝낸 사람이 있을 때
	bool bShow = false;
	if (Combat && LocalPS && Combat->GetPhase() == ECombatPhase::PlayerTurn && IsOccupant(Area, LocalPS) && LocalPS->GetCurrentArea() == Area)
	{
		const ATerminusBattler* MyBattler = Cast<ATerminusBattler>(LocalPS->GetPawn());
		const bool bMeWaiting = Combat->HasEndedTurn(LocalPS) || !Combat->IsTurnOf(LocalPS) || !IsAliveBattler(MyBattler);

		if (bMeWaiting)
		{
			for (const ATerminusPlayerState* Other : Area->GetOccupants())
			{
				if (!Other || Other == LocalPS) continue;
				if (Combat->IsTurnOf(Other) && !Combat->HasEndedTurn(Other) && IsAliveBattler(Cast<ATerminusBattler>(Other->GetPawn())))
				{
					bShow = true;
					break;
				}
			}
		}
	}

	const ESlateVisibility Wanted = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (NudgeButton->GetVisibility() != Wanted)
	{
		NudgeButton->SetVisibility(Wanted);
	}
	if (!bShow) return;

	const UWorld* World = GetWorld();
	const double Remaining = World ? NextNudgeTime - World->GetTimeSeconds() : 0.0;
	NudgeButton->SetIsEnabled(Remaining <= 0.0);
	SetCommsText(NudgeLabel, Remaining > 0.0
		? FString::Printf(TEXT("재촉 (%d)"), FMath::CeilToInt(Remaining))
		: FString(TEXT("재촉")));
}

// =====================================================================
// 계산 도우미
// =====================================================================

bool UPartyCommsWidget::IsEnemyOf(const UDungeonCombatComponent* Combat, const ATerminusPlayerState* Viewer, const ATerminusBattler* Battler)
{
	if (!Battler) return false;
	if (Battler->IsA<ATerminusMonster>()) return true;

	// 플레이어끼리는 배신 전투에서 편이 갈릴 때만 적
	const ATerminusPlayerState* BattlerPS = GetBattlerPS(Battler);
	return Combat && Combat->IsBetrayal() && BattlerPS && !Combat->IsSameSide(Viewer, BattlerPS);
}

bool UPartyCommsWidget::PlanTouches(const FCombatPlan& Plan, const UDungeonCombatComponent* Combat, const ATerminusBattler* Battler)
{
	switch (Plan.TargetType)
	{
	case ETargetType::AllEnemies:
		return IsEnemyOf(Combat, Plan.Planner, Battler);
	case ETargetType::AllAllies:
		return !Battler->IsA<ATerminusMonster>() && !IsEnemyOf(Combat, Plan.Planner, Battler);
	default:
		// 자신 / 1명 대상: 서버가 넣어 준 대상. 아직 고르는 중(nullptr)이면 여기선 아님
		return Plan.Target && Plan.Target.Get() == Battler;
	}
}

bool UPartyCommsWidget::IsPlanShown(const FCombatPlan& Plan, const ADungeonArea* Area, const UDungeonCombatComponent* Combat) const
{
	return Plan.IsValid() && Plan.Planner && Combat
		&& Combat->GetPhase() == ECombatPhase::PlayerTurn
		&& Plan.Cycle == Combat->GetCycle()
		&& Combat->IsTurnOf(Plan.Planner)
		&& !Combat->HasEndedTurn(Plan.Planner)
		&& IsOccupant(Area, Plan.Planner);
}

FString UPartyCommsWidget::DescribePlan(const FCombatPlan& Plan) const
{
	const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Plan.SkillRow);
	const FString SkillName = Skill ? Skill->DisplayName_KR.ToString() : Plan.SkillRow.ToString();

	FString Value;
	switch (Plan.Effect)
	{
	case ECombatPlanEffect::Damage:
		Value = Plan.HitCount > 1 ? FString::Printf(TEXT(" 피해 %dx%d"), Plan.Amount, Plan.HitCount) : FString::Printf(TEXT(" 피해 %d"), Plan.Amount);
		break;
	case ECombatPlanEffect::Shield: Value = FString::Printf(TEXT(" 보호막 %d"), Plan.Amount); break;
	case ECombatPlanEffect::Heal:   Value = FString::Printf(TEXT(" 회복 %d"), Plan.Amount); break;
	default: break;
	}

	const bool bChoosing = !Plan.Target && (Plan.TargetType == ETargetType::SingleEnemy || Plan.TargetType == ETargetType::SingleAlly);
	return FString::Printf(TEXT("%s%s: %s%s%s"),
		Plan.bDeclared ? TEXT("[예약] ") : TEXT(""),
		*Plan.Planner->GetPlayerName(), *SkillName, *Value,
		bChoosing ? TEXT(" (대상 고르는 중)") : TEXT(""));
}

FString UPartyCommsWidget::DescribeTarget(const AActor* Target) const
{
	const ATerminusBattler* Battler = Cast<ATerminusBattler>(Target);
	if (const ATerminusPlayerState* PS = GetBattlerPS(Battler))
	{
		return PS->GetPlayerName();
	}

	// 몬스터: 지금 보는 구역 몬스터 목록의 순서로 "적 2"
	const ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ADungeonArea* Area = PC ? PC->GetViewedArea() : nullptr;
	const UDungeonCombatComponent* Combat = Area ? Area->GetCombat() : nullptr;
	if (Combat)
	{
		const TArray<TObjectPtr<ATerminusMonster>>& Monsters = Combat->GetMonsters();
		for (int32 i = 0; i < Monsters.Num(); ++i)
		{
			if (Monsters[i].Get() == Target) return FString::Printf(TEXT("적 %d"), i + 1);
		}
	}
	return TEXT("적");
}
