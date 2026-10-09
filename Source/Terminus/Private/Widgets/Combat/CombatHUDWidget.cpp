#include "Widgets/Combat/CombatHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/TerminusBattler.h"
#include "Character/TerminusMonster.h"
#include "Combat/CombatStatsComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonArea.h"
#include "Dungeon/DungeonCombatComponent.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Relic/RelicBarWidget.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor PanelColor(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor EnergyColor(0.25f, 0.6f, 1.f);
	const FLinearColor SkillEnergyColor(0.65f, 0.4f, 1.f);
	const FLinearColor PlayerHealthColor(0.3f, 0.85f, 0.35f);   // 시안: 아군은 초록
	const FLinearColor MonsterHealthColor(0.9f, 0.25f, 0.2f);   // 시안: 적은 빨강

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		return Text;
	}

	UBorder* MakePanel(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding = FMargin(12.f, 8.f))
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrushColor(PanelColor);
		Border->SetPadding(Padding);
		Border->SetContent(Content);
		return Border;
	}

	// 캔버스에 붙이고 앵커 / 정렬 지정. 크기는 내용에 맞춤
	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetAutoSize(true);
		return Slot;
	}

	FString IntentLabel(const FMonsterIntent& Intent)
	{
		// TODO: 기획은 아이콘. 아이콘 에셋이 생기면 이미지로
		switch (Intent.Kind)
		{
		case EMonsterIntentKind::Attack:
			return Intent.HitCount > 1
				? FString::Printf(TEXT("공격 %dx%d"), Intent.Amount, Intent.HitCount)
				: FString::Printf(TEXT("공격 %d"), Intent.Amount);
		case EMonsterIntentKind::Defend: return TEXT("방어");
		case EMonsterIntentKind::Buff:   return TEXT("버프");
		default:                          return TEXT("디버프");
		}
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UCombatHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WBP 없이 C++ 클래스로 만들었거나, WBP 에 필요한 위젯이 없으면 기본 배치
	if (!WidgetTree->RootWidget || !TagLayer)
	{
		BuildDefaultLayout();
	}

	// 칸은 강화 칸 수를 알게 되면(내 배틀러 스텟) 다시 만듦. 우선 기본값으로
	BuildSkillSlots(ATerminusPlayerState::NumEnhanceSkills);

	// 보유 유물 줄이 WBP 에 없으면 오른쪽 아래(턴 종료 / 상태 위)에
	if (!RelicBar && TagLayer)
	{
		RelicBar = CreateWidget<URelicBarWidget>(this, URelicBarWidget::StaticClass());
		if (RelicBar)
		{
			AddToCanvas(TagLayer, RelicBar, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -130.f));
		}
	}

	// NativeConstruct 는 여러 번 불릴 수 있어서 버튼 바인딩은 여기서 한 번만 (주점 / 메인 메뉴와 같은 규칙)
	if (EndTurnButton) EndTurnButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::HandleEndTurnClicked);

	SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatHUDWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatRoot"));
	WidgetTree->RootWidget = Root;

	// 따라다니는 표시들 (전체 화면, 맨 뒤)
	TagLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TagLayer"));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(TagLayer))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}

	// ---- 위: 방 / 턴 안내
	RoomText = MakeText(WidgetTree, TEXT(""), 20);
	AddToCanvas(Root, MakePanel(WidgetTree, RoomText), FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(24.f, 20.f));

	PhaseText = MakeText(WidgetTree, TEXT(""), 26);
	AddToCanvas(Root, MakePanel(WidgetTree, PhaseText, FMargin(20.f, 8.f)), FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));

	HintText = MakeText(WidgetTree, TEXT(""), 20, FLinearColor(1.f, 0.9f, 0.4f));
	AddToCanvas(Root, HintText, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -200.f));

	// ---- 왼쪽 아래: 에너지 + 기본 스킬 3개 / 스킬 에너지
	UVerticalBox* LeftBox = WidgetTree->ConstructWidget<UVerticalBox>();

	UHorizontalBox* SkillRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	EnergyText = MakeText(WidgetTree, TEXT("에너지 0/0"), 22, EnergyColor);
	if (UHorizontalBoxSlot* S = SkillRow->AddChildToHorizontalBox(EnergyText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	}

	SkillBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillBox"));
	SkillRow->AddChildToHorizontalBox(SkillBox);

	LeftBox->AddChildToVerticalBox(SkillRow);

	UHorizontalBox* EnhanceRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	SkillEnergyText = MakeText(WidgetTree, TEXT("스킬 에너지 0/0"), 18, SkillEnergyColor);
	if (UHorizontalBoxSlot* S = EnhanceRow->AddChildToHorizontalBox(SkillEnergyText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	}
	EnhanceSkillBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnhanceSkillBox"));
	EnhanceRow->AddChildToHorizontalBox(EnhanceSkillBox);

	if (UVerticalBoxSlot* S = LeftBox->AddChildToVerticalBox(EnhanceRow))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	AddToCanvas(Root, MakePanel(WidgetTree, LeftBox), FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(24.f, -24.f));

	// ---- 오른쪽 아래: 턴 종료 + 상태
	UVerticalBox* RightBox = WidgetTree->ConstructWidget<UVerticalBox>();

	EndTurnButton = WidgetTree->ConstructWidget<UButton>();
	EndTurnButton->SetContent(MakeText(WidgetTree, TEXT("턴 종료"), 22));
	if (UVerticalBoxSlot* S = RightBox->AddChildToVerticalBox(EndTurnButton))
	{
		S->SetHorizontalAlignment(HAlign_Right);
	}

	StatusText = MakeText(WidgetTree, TEXT(""), 18);
	if (UVerticalBoxSlot* S = RightBox->AddChildToVerticalBox(StatusText))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Right);
	}

	AddToCanvas(Root, MakePanel(WidgetTree, RightBox), FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f));
}

void UCombatHUDWidget::HandleSkillSlotClicked(UItemSlotWidget* ClickedSlot)
{
	if (ClickedSlot)
	{
		OnSkillClicked(ClickedSlot->GetSlotIndex());
	}
}

void UCombatHUDWidget::BuildSkillSlots(int32 EnhanceCount)
{
	EnhanceCount = FMath::Clamp(EnhanceCount, 0, ATerminusPlayerState::NumEnhanceSkills);
	if (EnhanceCount == NumEnhanceSlots) return;
	NumEnhanceSlots = EnhanceCount;

	SkillSlots.Reset();
	if (SkillBox) SkillBox->ClearChildren();
	if (EnhanceSkillBox) EnhanceSkillBox->ClearChildren();

	for (int32 i = 0; i < ATerminusPlayerState::NumBasicSkills; ++i)
	{
		SkillSlots.Add(AddSkillSlot(SkillBox, i, i == 0));
	}
	for (int32 i = 0; i < EnhanceCount; ++i)
	{
		SkillSlots.Add(AddSkillSlot(EnhanceSkillBox, ATerminusPlayerState::NumBasicSkills + i, i == 0));
	}
}

UItemSlotWidget* UCombatHUDWidget::AddSkillSlot(UPanelWidget* Box, int32 SlotIndex, bool bFirst)
{
	if (!Box) return nullptr;

	const TSubclassOf<UItemSlotWidget> Class = SkillSlotClass ? SkillSlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	UItemSlotWidget* SkillSlot = CreateWidget<UItemSlotWidget>(this, Class);
	if (!SkillSlot) return nullptr;

	SkillSlot->SetSlotIndex(SlotIndex);
	SkillSlot->OnSlotClicked.BindUObject(this, &UCombatHUDWidget::HandleSkillSlotClicked);
	if (!SkillSlotClass)
	{
		SkillSlot->SetSlotSize(FVector2D(92.f, 84.f));   // C++ 기본 칸이면 전투용 크기
	}

	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Box->AddChild(SkillSlot)))
	{
		HSlot->SetPadding(FMargin(bFirst ? 0.f : SkillSlotSpacing, 0.f, 0.f, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
	return SkillSlot;
}

// =====================================================================
// 구역 / 표시
// =====================================================================

void UCombatHUDWidget::SetArea(ADungeonArea* InArea)
{
	Area = InArea;
	CancelTargeting();
	RebuildTags();

	// 구역에 있는 동안 전투 진행 여부를 계속 보고 켜고 끔 (전투 방이 아니면 안 보임)
	if (UWorld* World = GetWorld())
	{
		if (InArea)
		{
			World->GetTimerManager().SetTimer(VisibilityTimer, this, &UCombatHUDWidget::UpdateVisibility, 0.1f, true, 0.f);
		}
		else
		{
			World->GetTimerManager().ClearTimer(VisibilityTimer);
		}
	}

	UpdateVisibility();
}

void UCombatHUDWidget::UpdateVisibility()
{
	ADungeonArea* CurrentArea = Area.Get();
	const UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;
	const bool bShow = Combat && Combat->IsInCombat();

	// 평소엔 바탕이 클릭을 안 받게(버튼만 받음). 대상 고르는 중에만 바탕도 받아서 우클릭 취소가 오게
	const ESlateVisibility Wanted = !bShow ? ESlateVisibility::Collapsed
		: (PendingSkillIndex != INDEX_NONE ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);

	if (GetVisibility() != Wanted)
	{
		SetVisibility(Wanted);
	}
}

void UCombatHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VisibilityTimer);
	}

	Super::NativeDestruct();
}

void UCombatHUDWidget::RebuildTags()
{
	// 내가 만든 표시만 지움. TagLayer 를 통째로 비우면(ClearChildren) WBP 에서 TagLayer 안에 배치한
	// 방 이름 / 스킬 / 에너지 / 턴 종료 같은 위젯까지 사라짐 (TagLayer 를 루트로 쓰는 경우)
	for (const FBattlerTag& Tag : Tags)
	{
		if (UTextBlock* Intent = Tag.IntentText.Get())   Intent->RemoveFromParent();
		if (UVerticalBox* Box = Tag.HealthBox.Get())       Box->RemoveFromParent();
		if (UButton* Target = Tag.TargetButton.Get())      Target->RemoveFromParent();
	}
	Tags.Reset();
	TaggedBattlers.Reset();

	ADungeonArea* CurrentArea = Area.Get();
	UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;
	if (!TagLayer || !Combat) return;

	auto AddTag = [&](ATerminusBattler* Battler, bool bMonster, int32 Index)
	{
		if (!Battler) return;

		FBattlerTag Tag;
		Tag.Battler = Battler;
		Tag.bMonster = bMonster;
		Tag.Index = Index;

		// 머리 위 행동 예고 (몬스터만)
		if (bMonster)
		{
			UTextBlock* Intent = MakeText(WidgetTree, TEXT(""), 20, FLinearColor(1.f, 0.85f, 0.4f));
			UCanvasPanelSlot* CanvasSlot = TagLayer->AddChildToCanvas(Intent);
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
			Tag.IntentText = Intent;
		}

		// 발밑 체력바 + 숫자
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		Bar->SetFillColorAndOpacity(bMonster ? MonsterHealthColor : PlayerHealthColor);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Bar))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
		UTextBlock* HpText = MakeText(WidgetTree, TEXT(""), 14);
		HpText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(HpText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		UCanvasPanelSlot* BoxSlot = TagLayer->AddChildToCanvas(Box);
		BoxSlot->SetAutoSize(false);
		BoxSlot->SetSize(FVector2D(90.f, 34.f));
		BoxSlot->SetAlignment(FVector2D(0.5f, 0.f));
		Tag.HealthBox = Box;
		Tag.HealthBar = Bar;
		Tag.HealthText = HpText;

		// 대상 선택 버튼 (대상 고르는 중일 때만 보임)
		UButton* Target = WidgetTree->ConstructWidget<UButton>();
		Target->SetBackgroundColor(FLinearColor(1.f, 0.85f, 0.3f, 0.35f));
		Target->SetContent(MakeText(WidgetTree, TEXT("선택"), 16));
		Target->OnClicked.AddDynamic(this, &UCombatHUDWidget::HandleTargetClicked);
		UCanvasPanelSlot* TargetSlot = TagLayer->AddChildToCanvas(Target);
		TargetSlot->SetAutoSize(false);
		TargetSlot->SetSize(TargetButtonSize);
		TargetSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		Target->SetVisibility(ESlateVisibility::Collapsed);
		Tag.TargetButton = Target;

		Tags.Add(Tag);
		TaggedBattlers.Add(Battler);
	};

	const TArray<TObjectPtr<ATerminusMonster>>& Monsters = Combat->GetMonsters();
	for (int32 i = 0; i < Monsters.Num(); ++i)
	{
		AddTag(Monsters[i], true, i);
	}

	const TArray<TObjectPtr<ATerminusPlayerState>>& Occupants = CurrentArea->GetOccupants();
	for (int32 i = 0; i < Occupants.Num(); ++i)
	{
		AddTag(Occupants[i] ? Cast<ATerminusBattler>(Occupants[i]->GetPawn()) : nullptr, false, i);
	}
}

void UCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ADungeonArea* CurrentArea = Area.Get();
	UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;

	// 보이기 / 숨기기는 UpdateVisibility(타이머) 담당. 여긴 보이는 동안의 갱신만
	if (!Combat || !Combat->IsInCombat()) return;

	// 몬스터 / 플레이어 구성이 바뀌었으면(복제 도착, 새 방) 표시 다시 만들기
	TArray<TWeakObjectPtr<ATerminusBattler>> Current;
	for (ATerminusMonster* Monster : Combat->GetMonsters()) Current.Add(Monster);
	for (ATerminusPlayerState* PS : CurrentArea->GetOccupants()) Current.Add(PS ? Cast<ATerminusBattler>(PS->GetPawn()) : nullptr);
	if (Current != TaggedBattlers)
	{
		RebuildTags();
	}

	// 내 턴이 아니면 대상 고르기 취소
	if (PendingSkillIndex != INDEX_NONE && Combat->GetPhase() != ECombatPhase::PlayerTurn)
	{
		CancelTargeting();
	}

	UpdateTags();
	UpdatePanels();
}

void UCombatHUDWidget::UpdateTags()
{
	APlayerController* PC = GetOwningPlayer();
	ADungeonArea* CurrentArea = Area.Get();
	UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;
	if (!PC || !Combat) return;

	// 대상 고르는 중이면 어느 편을 고르는지
	bool bPickEnemy = false;
	bool bPickAlly = false;
	if (PendingSkillIndex != INDEX_NONE)
	{
		const ATerminusPlayerState* LocalPS = PC->GetPlayerState<ATerminusPlayerState>();
		if (const FSkillRow* Pending = LocalPS ? LocalPS->GetCombatSkill(PendingSkillIndex) : nullptr)
		{
			bPickEnemy = Pending->TargetType == ETargetType::SingleEnemy;
			bPickAlly = Pending->TargetType == ETargetType::SingleAlly;
		}
	}

	for (FBattlerTag& Tag : Tags)
	{
		ATerminusBattler* Battler = Tag.Battler.Get();
		const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
		const bool bAlive = Stats && !Stats->IsDead() && !Battler->IsHidden();

		// 캐릭터 발 + Height 높이를 화면 좌표로 바꿔 그 자리에 위젯을 놓음 (DPI 배율까지 반영된 위젯 좌표)
		auto Place = [&](UWidget* Widget, float Height)
		{
			if (!Widget || !Battler) return;

			FVector2D ScreenPos = FVector2D::ZeroVector;
			UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				PC, Battler->GetActorLocation() + FVector(0.f, 0.f, Height), ScreenPos, false);

			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				CanvasSlot->SetPosition(ScreenPos);
			}
		};

		// 머리 위 행동 예고
		if (UTextBlock* Intent = Tag.IntentText.Get())
		{
			const FMonsterIntent* Found = Tag.bMonster ? Combat->FindIntent(Cast<ATerminusMonster>(Battler)) : nullptr;
			const bool bVisible = bAlive && Found;
			Intent->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bVisible)
			{
				Intent->SetText(FText::FromString(IntentLabel(*Found)));
				Place(Intent, HeadHeight);
			}
		}

		// 발밑 체력
		if (UVerticalBox* Box = Tag.HealthBox.Get())
		{
			Box->SetVisibility(bAlive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bAlive)
			{
				const FCombatState& State = Stats->GetCombatState();
				const int32 MaxHealth = FMath::Max(1, Stats->GetStats().MaxHealth);
				if (UProgressBar* Bar = Tag.HealthBar.Get())
				{
					Bar->SetPercent(static_cast<float>(State.Health) / MaxHealth);
				}
				if (UTextBlock* HpText = Tag.HealthText.Get())
				{
					FString Label = FString::Printf(TEXT("%d/%d"), State.Health, MaxHealth);
					if (State.Shield > 0) Label += FString::Printf(TEXT("  +%d"), State.Shield);
					HpText->SetText(FText::FromString(Label));
				}
				Place(Box, -4.f);
			}
		}

		// 대상 선택 버튼. 배신 전투면 적 = 상대 편 플레이어, 아군 = 같은 편
		if (UButton* Target = Tag.TargetButton.Get())
		{
			bool bSelectable = bAlive && ((bPickEnemy && Tag.bMonster) || (bPickAlly && !Tag.bMonster));
			if (Combat->IsBetrayal() && !Tag.bMonster)
			{
				const TArray<TObjectPtr<ATerminusPlayerState>>& Occupants = CurrentArea->GetOccupants();
				const ATerminusPlayerState* TagPS = Occupants.IsValidIndex(Tag.Index) ? Occupants[Tag.Index].Get() : nullptr;
				const bool bSameSide = Combat->IsSameSide(TagPS, PC->GetPlayerState<ATerminusPlayerState>());
				bSelectable = bAlive && ((bPickEnemy && !bSameSide) || (bPickAlly && bSameSide));
			}
			Target->SetVisibility(bSelectable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bSelectable)
			{
				Place(Target, BodyCenterHeight);
			}
		}
	}
}

void UCombatHUDWidget::UpdatePanels()
{
	APlayerController* PC = GetOwningPlayer();
	ADungeonArea* CurrentArea = Area.Get();
	UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!Combat || !LocalPS) return;

	const ATerminusBattler* MyBattler = Cast<ATerminusBattler>(LocalPS->GetPawn());
	const UCombatStatsComponent* MyStats = MyBattler ? MyBattler->GetCombatStats() : nullptr;
	const bool bMeAlive = MyStats && !MyStats->IsDead();

	// 관전 중 (내가 들어 있지 않은 구역을 보는 중): 내 턴 아님, 스킬 못 씀
	const bool bSpectating = !CurrentArea->GetOccupants().ContainsByPredicate(
		[LocalPS](const TObjectPtr<ATerminusPlayerState>& P) { return P.Get() == LocalPS; });

	const ECombatPhase Phase = Combat->GetPhase();
	const bool bMyTurn = !bSpectating && Combat->IsTurnOf(LocalPS) && bMeAlive && !Combat->HasEndedTurn(LocalPS);
	const bool bBetrayal = Combat->IsBetrayal();

	// ---- 위: "n층 n번째 방" (기획 시안)
	if (RoomText)
	{
		if (!CachedMap.IsValid())
		{
			CachedMap = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
		}
		const int32 Floor = CachedMap.IsValid() ? CachedMap->CurrentFloor : 1;
		RoomText->SetText(FText::FromString(bBetrayal
			? FString::Printf(TEXT("%d층 배신 전투  ·  사이클 %d"), Floor, Combat->GetCycle())
			: FString::Printf(TEXT("%d층 %d번째 방  ·  사이클 %d"), Floor, CurrentArea->GetRoom().Row + 1, Combat->GetCycle())));
	}

	if (PhaseText)
	{
		FString Label;
		switch (Phase)
		{
		case ECombatPhase::PlayerTurn:
			if (bBetrayal && !bMyTurn && bMeAlive && !bSpectating && !Combat->IsTurnOf(LocalPS))
			{
				Label = Combat->IsBetrayerTurn() ? TEXT("배신자의 턴") : TEXT("상대 편의 턴");
				break;
			}
			Label = bSpectating ? TEXT("플레이어 턴") : (!bMeAlive ? TEXT("쓰러짐") : (bMyTurn ? TEXT("내 턴") : TEXT("다른 플레이어를 기다리는 중")));
			break;
		case ECombatPhase::MonsterTurn: Label = TEXT("적의 턴"); break;
		case ECombatPhase::Victory:
			// 배신 전투: 내 편이 이겼는지
			Label = !bBetrayal || bSpectating || ((LocalPS == Combat->GetBetrayer()) == Combat->DidBetrayerWin()) ? TEXT("승리!") : TEXT("패배");
			break;
		case ECombatPhase::Defeat:      Label = TEXT("패배"); break;
		default: break;
		}
		if (bSpectating) Label = TEXT("관전 중  ·  ") + Label;
		PhaseText->SetText(FText::FromString(Label));
	}

	if (HintText)
	{
		HintText->SetText(FText::FromString(PendingSkillIndex != INDEX_NONE ? TEXT("대상을 선택하세요  (우클릭: 취소)") : TEXT("")));
	}

	// ---- 왼쪽 아래: 에너지 + 스킬
	if (MyStats)
	{
		const FCombatState& State = MyStats->GetCombatState();
		const FCharacterStats& Stats = MyStats->GetStats();

		if (EnergyText)
		{
			EnergyText->SetText(FText::FromString(FString::Printf(TEXT("에너지 %d/%d"), State.Energy, Stats.MaxEnergy)));
		}
		if (SkillEnergyText)
		{
			SkillEnergyText->SetText(FText::FromString(FString::Printf(TEXT("스킬 에너지 %d/%d"), State.SkillEnergy, Stats.MaxSkillEnergy)));
		}

		// 강화 칸 수가 캐릭터 스텟과 다르면 다시 만듦
		BuildSkillSlots(Stats.EnhanceSlots);

		for (UItemSlotWidget* SkillSlot : SkillSlots)
		{
			if (!SkillSlot) continue;

			const FSkillRow* Skill = LocalPS->GetCombatSkill(SkillSlot->GetSlotIndex());
			SkillSlot->SetSkill(LocalPS->GetCombatSkillRow(SkillSlot->GetSlotIndex()));
			SkillSlot->SetUsable(Skill && bMyTurn
				&& State.Energy >= MyStats->GetEffectiveEnergyCost(Skill->EnergyCost) && State.SkillEnergy >= Skill->SkillEnergyCost);   // 과욕이면 +1
			SkillSlot->SetSelected(PendingSkillIndex == SkillSlot->GetSlotIndex());   // 대상 고르는 중인 칸 강조
		}

		// ---- 오른쪽 아래: 상태. 기획 시안 "체력 [최대/현재] 힘 방어 회피"
		if (StatusText)
		{
			FString Label = FString::Printf(TEXT("체력 [%d/%d]"), Stats.MaxHealth, State.Health);
			if (State.Shield > 0) Label += FString::Printf(TEXT("  보호막 %d"), State.Shield);
			Label += FString::Printf(TEXT("   힘 %d  방어 %d  회피 %d"), Stats.Attack, Stats.Defense, Stats.Evasion);
			StatusText->SetText(FText::FromString(Label));
		}
	}

	if (EndTurnButton)
	{
		EndTurnButton->SetIsEnabled(bMyTurn);
	}
}

// =====================================================================
// 입력
// =====================================================================

void UCombatHUDWidget::OnSkillClicked(int32 SkillIndex)
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PC || !LocalPS) return;

	// 관전 중인 구역에선 못 씀
	if (const ADungeonArea* Viewed = Area.Get(); Viewed && !Viewed->GetOccupants().ContainsByPredicate(
		[LocalPS](const TObjectPtr<ATerminusPlayerState>& P) { return P.Get() == LocalPS; }))
	{
		return;
	}

	// 같은 스킬을 다시 누르면 취소
	if (PendingSkillIndex == SkillIndex)
	{
		CancelTargeting();
		return;
	}

	const FSkillRow* Skill = LocalPS->GetCombatSkill(SkillIndex);
	if (!Skill) return;

	const ETargetType TargetType = Skill->TargetType;
	if (TargetType == ETargetType::SingleEnemy || TargetType == ETargetType::SingleAlly)
	{
		// 대상을 골라야 하는 스킬 -> 대상 고르기 모드 (바탕이 우클릭을 받게 바로 전환)
		PendingSkillIndex = SkillIndex;
		UpdateVisibility();
		return;
	}

	// 대상이 정해져 있는 스킬(자신 / 전체)은 바로 사용
	CancelTargeting();
	PC->Server_UseSkill(SkillIndex, INDEX_NONE);
}

void UCombatHUDWidget::CancelTargeting()
{
	if (PendingSkillIndex == INDEX_NONE) return;

	PendingSkillIndex = INDEX_NONE;
	UpdateVisibility();
}

void UCombatHUDWidget::HandleEndTurnClicked()
{
	CancelTargeting();

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_EndTurn();
	}
}

void UCombatHUDWidget::HandleTargetClicked()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!PC || PendingSkillIndex == INDEX_NONE) return;

	// 버튼 클릭 이벤트엔 누가 눌렸는지가 안 와서, 지금 마우스가 올라가 있는 대상 버튼으로 찾음
	for (const FBattlerTag& Tag : Tags)
	{
		const UButton* Target = Tag.TargetButton.Get();
		if (Target && Target->IsVisible() && Target->IsHovered())
		{
			PC->Server_UseSkill(PendingSkillIndex, Tag.Index);
			CancelTargeting();
			return;
		}
	}
}

FReply UCombatHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 우클릭 = 대상 고르기 취소
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && PendingSkillIndex != INDEX_NONE)
	{
		CancelTargeting();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
