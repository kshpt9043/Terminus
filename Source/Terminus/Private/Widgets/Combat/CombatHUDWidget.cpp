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
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
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
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/TerminusUISettings.h"

namespace
{
	// 팔레트 (스타일 가이드). 월드 위라 판은 살짝 비침
	const FLinearColor HUDPanelColor = TerminusUI::Hex(TEXT("1B1E25"), 0.85f);
	const FLinearColor& EnergyColor = TerminusUI::Color::GoldHover;
	const FLinearColor& SkillEnergyColor = TerminusUI::Color::TextSub;
	const FLinearColor& PlayerHealthColor = TerminusUI::Color::Ready;    // 시안: 아군은 초록
	const FLinearColor& MonsterHealthColor = TerminusUI::Color::Alert;   // 시안: 적은 빨강

	// 대상 선택 버튼: 시안의 대상 강조 (캐릭터 둘레 금 테두리 2px, 마우스 올리면 밝은 금 3px). 속은 비움
	FButtonStyle MakeTargetButtonStyle()
	{
		FButtonStyle Style = TerminusUI::MakeFlatButtonStyle(TerminusUI::Color::Card);
		Style.Normal = TerminusUI::MakeBoxBrush(TerminusUI::Hex(TEXT("FFEDA0"), 0.06f), TerminusUI::Hex(TEXT("FFEDA0")), 0.f, 2.f);
		Style.Hovered = TerminusUI::MakeBoxBrush(TerminusUI::Hex(TEXT("FFF6D0"), 0.14f), TerminusUI::Hex(TEXT("FFF6D0")), 0.f, 3.f);
		Style.Pressed = TerminusUI::MakeBoxBrush(TerminusUI::Hex(TEXT("FFF6D0"), 0.2f), TerminusUI::Hex(TEXT("FFF6D0")), 0.f, 3.f);
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		return Style;
	}

	FLinearColor TargetButtonTint(bool bDragHover)
	{
		return FLinearColor(1.f, 1.f, 1.f, bDragHover ? 1.f : 0.55f);
	}

	// 배경 없이 월드 위에 뜨는 글자라 그림자
	UTextBlock* MakeHUDText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Text = TerminusUI::MakeText(Tree, NAME_None, Initial, Size, Color, ETextJustify::Left);
		TerminusUI::AddShadow(Text, 0.85f);
		return Text;
	}

	UBorder* MakePanel(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding = FMargin(12.f, 8.f))
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrush(TerminusUI::MakeBoxBrush(HUDPanelColor, TerminusUI::Color::Line));
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

	// 행동 예고: 아이콘 번호 (공격 / 방어 / 버프 / 디버프) + 숫자 (공격만, 연타는 "4x2")
	int32 IntentIconIndex(const FMonsterIntent& Intent)
	{
		switch (Intent.Kind)
		{
		case EMonsterIntentKind::Attack: return 0;
		case EMonsterIntentKind::Defend: return 1;
		case EMonsterIntentKind::Buff:   return 2;
		default:                          return 3;
		}
	}

	FString IntentNumber(const FMonsterIntent& Intent)
	{
		if (Intent.Kind != EMonsterIntentKind::Attack) return FString();
		return Intent.HitCount > 1 ? FString::Printf(TEXT("%dx%d"), Intent.Amount, Intent.HitCount) : FString::FromInt(Intent.Amount);
	}

	// 상태이상 글자 칸 (전투 시안): 이로움 금 / 청, 해로움 적 / 보라
	struct FStatusLook { const TCHAR* Letter; const TCHAR* Hex; };
	FStatusLook GetStatusLook(EStatusEffect Type)
	{
		switch (Type)
		{
		case EStatusEffect::Counter:        return { TEXT("반"), TEXT("FFD978") };
		case EStatusEffect::Mark:           return { TEXT("급"), TEXT("FFD978") };
		case EStatusEffect::Evasion:        return { TEXT("회"), TEXT("7FB8E8") };
		case EStatusEffect::Poison:         return { TEXT("중"), TEXT("B07CD8") };
		case EStatusEffect::Brave:          return { TEXT("용"), TEXT("FFD978") };
		case EStatusEffect::Protection:     return { TEXT("가"), TEXT("7FB8E8") };
		case EStatusEffect::Ice:            return { TEXT("얼"), TEXT("B07CD8") };
		case EStatusEffect::Lava:           return { TEXT("용"), TEXT("FF5A4A") };
		case EStatusEffect::Metal:          return { TEXT("강"), TEXT("7FB8E8") };
		case EStatusEffect::Acid:           return { TEXT("산"), TEXT("FF5A4A") };
		case EStatusEffect::Fear:           return { TEXT("공"), TEXT("B07CD8") };
		case EStatusEffect::Fusion:         return { TEXT("퓨"), TEXT("FFD978") };
		case EStatusEffect::Vitality:       return { TEXT("활"), TEXT("FFD978") };
		case EStatusEffect::Reflux:         return { TEXT("역"), TEXT("FF5A4A") };
		case EStatusEffect::InternalInjury: return { TEXT("내"), TEXT("FF5A4A") };
		case EStatusEffect::Immortality:    return { TEXT("불"), TEXT("7FB8E8") };
		case EStatusEffect::Absorption:     return { TEXT("흡"), TEXT("FFD978") };
		case EStatusEffect::Solid:          return { TEXT("견"), TEXT("7FB8E8") };
		case EStatusEffect::Indomitable:    return { TEXT("불"), TEXT("FFD978") };
		case EStatusEffect::IronWall:       return { TEXT("철"), TEXT("7FB8E8") };
		// 같은 그룹이라도 테마 이름 첫 글자로 (색은 그룹과 같게)
		case EStatusEffect::Dark:           return { TEXT("어"), TEXT("FF5A4A") };
		case EStatusEffect::Flooding:       return { TEXT("침"), TEXT("B07CD8") };
		case EStatusEffect::Contrarywind:   return { TEXT("풍"), TEXT("B07CD8") };
		case EStatusEffect::Spirit:         return { TEXT("정"), TEXT("FFD978") };
		case EStatusEffect::Blindness:      return { TEXT("실"), TEXT("FFD978") };
		default:                           return { TEXT("?"), TEXT("A89F8C") };
		}
	}

	// 턴 띠 단계별 색 (글자 / 띠 바탕 / 띠 진하기 / 위아래 선)
	struct FPhaseLook { const TCHAR* Text; const TCHAR* Band; float BandAlpha; const TCHAR* Line; };
	enum EPhaseStyle : int32 { Phase_Mine, Phase_Waiting, Phase_Enemy, Phase_Defeat, Phase_Betrayer, Phase_Victory };
	const FPhaseLook PhaseLooks[] =
	{
		{ TEXT("FFEDA0"), TEXT("403212"), 0.88f, TEXT("FFD978") },   // 내 턴
		{ TEXT("A89F8C"), TEXT("1C1A22"), 0.72f, TEXT("5A5562") },   // 다른 플레이어를 기다리는 중
		{ TEXT("FFC2B8"), TEXT("4A1012"), 0.88f, TEXT("FF4A3A") },   // 적의 턴
		{ TEXT("8C8796"), TEXT("0E0D10"), 0.88f, TEXT("6A6470") },   // 패배
		{ TEXT("FFD2CC"), TEXT("5A0A0E"), 0.92f, TEXT("FF3A3A") },   // 배신자의 턴
		{ TEXT("FFD978"), TEXT("403212"), 0.88f, TEXT("FFD978") },   // 승리
	};

	UTexture2D* LoadCombatTexture(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/Textures/Combat/%s.%s"), Name, Name);
		return LoadObject<UTexture2D>(nullptr, *Path);
	}

	// 굵은 숫자 + 검은 외곽선 (행동 예고)
	void SetOutlinedFont(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = TerminusUI::GetFont(Size, true);
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Font);
	}

	USizeBox* MakeSized(UWidgetTree* Tree, UWidget* Content, float Width, float Height)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		if (Width > 0.f) Box->SetWidthOverride(Width);
		if (Height > 0.f) Box->SetHeightOverride(Height);
		Box->SetContent(Content);
		return Box;
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

	LoadCombatTextures();

	// 칸은 강화 칸 수를 알게 되면(내 배틀러 스텟) 다시 만듦. 우선 기본값으로
	BuildSkillSlots(ATerminusPlayerState::NumEnhanceSkills);

	// 보유 유물 줄이 WBP 에 없으면 오른쪽 아래(턴 종료 / 상태 위)에
	if (!RelicBar && TagLayer)
	{
		RelicBar = CreateWidget<URelicBarWidget>(this, UTerminusUISettings::Pick(RelicBarClass, &UTerminusUISettings::RelicBar));
		if (RelicBar)
		{
			AddToCanvas(TagLayer, RelicBar, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-28.f, -176.f));
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
	RoomText = MakeHUDText(WidgetTree, TEXT(""), TerminusUI::FontSize::Subtitle, TerminusUI::Color::TextSub);
	AddToCanvas(Root, MakePanel(WidgetTree, RoomText), FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(24.f, 20.f));

	PhaseText = MakeHUDText(WidgetTree, TEXT(""), TerminusUI::FontSize::Section, TerminusUI::Color::Gold);
	TerminusUI::SetBold(PhaseText);
	AddToCanvas(Root, MakePanel(WidgetTree, PhaseText, FMargin(20.f, 8.f)), FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));

	HintText = MakeHUDText(WidgetTree, TEXT(""), 20, TerminusUI::Color::GoldHover);
	AddToCanvas(Root, HintText, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -200.f));

	// ---- 왼쪽 아래: 에너지 + 기본 스킬 3개 / 스킬 에너지
	UVerticalBox* LeftBox = WidgetTree->ConstructWidget<UVerticalBox>();

	UHorizontalBox* SkillRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	EnergyText = MakeHUDText(WidgetTree, TEXT("에너지 0/0"), TerminusUI::FontSize::Section, EnergyColor);
	TerminusUI::SetBold(EnergyText);
	if (UHorizontalBoxSlot* S = SkillRow->AddChildToHorizontalBox(EnergyText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	}

	SkillBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillBox"));
	SkillRow->AddChildToHorizontalBox(SkillBox);

	LeftBox->AddChildToVerticalBox(SkillRow);

	UHorizontalBox* EnhanceRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	SkillEnergyText = MakeHUDText(WidgetTree, TEXT("스킬 에너지 0/0"), TerminusUI::FontSize::Subtitle, SkillEnergyColor);
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

	// 턴 종료 = 주 행동 육각 버튼
	EndTurnButton = WidgetTree->ConstructWidget<UButton>();
	TerminusUI::StyleHexButton(EndTurnButton, TerminusUI::EHexButton::Primary);
	EndTurnLabel = MakeHUDText(WidgetTree, TEXT("턴 종료"), 20, TerminusUI::Color::TextBody);
	TerminusUI::SetBold(EndTurnLabel);
	TerminusUI::SetLetterSpacing(EndTurnLabel, 120);
	EndTurnButton->SetContent(EndTurnLabel);
	if (UVerticalBoxSlot* S = RightBox->AddChildToVerticalBox(EndTurnButton))
	{
		S->SetHorizontalAlignment(HAlign_Right);
	}

	StatusText = MakeHUDText(WidgetTree, TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::TextBody);
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
	SkillDecos.Reset();
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

	const TSubclassOf<UItemSlotWidget> Class = UTerminusUISettings::Pick(SkillSlotClass, &UTerminusUISettings::ItemSlot);
	UItemSlotWidget* SkillSlot = CreateWidget<UItemSlotWidget>(this, Class);
	if (!SkillSlot) return nullptr;

	SkillSlot->SetSlotIndex(SlotIndex);
	SkillSlot->OnSlotClicked.BindUObject(this, &UCombatHUDWidget::HandleSkillSlotClicked);
	SkillSlot->SetSlotSize(FVector2D(92.f, 84.f));   // 전투 시안 칸 92x84 (WBP 칸도 SlotSizeBox 가 있으면 맞춤)

	// 칸을 겹침 판으로 감싸고 그 위에 시안 장식: 왼쪽 위 층 마름모 / 오른쪽 위 비용 원 / 오른쪽 강화 +N
	UOverlay* Holder = WidgetTree->ConstructWidget<UOverlay>();
	if (UOverlaySlot* S = Holder->AddChildToOverlay(SkillSlot))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	FSkillDeco& Deco = SkillDecos.AddDefaulted_GetRef();
	Deco.Holder = Holder;

	UHorizontalBox* Pips = WidgetTree->ConstructWidget<UHorizontalBox>();
	Pips->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* S = Holder->AddChildToOverlay(Pips))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(8.f, 8.f, 0.f, 0.f));
	}
	Deco.TierPips = Pips;

	UBorder* Cost = WidgetTree->ConstructWidget<UBorder>();
	Cost->SetPadding(FMargin(0.f));
	Cost->SetHorizontalAlignment(HAlign_Center);
	Cost->SetVerticalAlignment(VAlign_Center);
	Cost->SetVisibility(ESlateVisibility::HitTestInvisible);
	UTextBlock* CostLabel = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 13, FLinearColor::White);
	TerminusUI::SetBold(CostLabel);
	CostLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Cost->SetContent(CostLabel);
	if (UOverlaySlot* S = Holder->AddChildToOverlay(MakeSized(WidgetTree, Cost, 24.f, 24.f)))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(0.f, -8.f, -8.f, 0.f));
	}
	Deco.CostBadge = Cost;
	Deco.CostText = CostLabel;

	UBorder* Upgrade = WidgetTree->ConstructWidget<UBorder>();
	Upgrade->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::Color::GoldBase, TerminusUI::Color::GoldBase, 0.f, 0.f));
	Upgrade->SetPadding(FMargin(4.f, 0.f));
	Upgrade->SetVisibility(ESlateVisibility::Collapsed);
	UTextBlock* UpgradeLabel = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 11, TerminusUI::Hex(TEXT("1A1410")));
	TerminusUI::SetBold(UpgradeLabel);
	Upgrade->SetContent(UpgradeLabel);
	if (UOverlaySlot* S = Holder->AddChildToOverlay(Upgrade))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(0.f, 44.f, 5.f, 0.f));
	}
	Deco.UpgradeBadge = Upgrade;
	Deco.UpgradeText = UpgradeLabel;

	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Box->AddChild(Holder)))
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
	ClearDeclaredPlan();
	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->ClearCombatPlan();
	}
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
		if (UBorder* Intent = Tag.IntentBox.Get())         Intent->RemoveFromParent();
		if (UBorder* Ready = Tag.ReadyBadge.Get())         Ready->RemoveFromParent();
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

		// 머리 위 행동 예고 (몬스터만): 어두운 판 + 픽셀 아이콘 + 굵은 숫자
		if (bMonster)
		{
			UBorder* IntentBox = WidgetTree->ConstructWidget<UBorder>();
			IntentBox->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::Hex(TEXT("0A090E"), 0.75f), TerminusUI::Hex(TEXT("E8DFCB"), 0.18f), 0.f, 1.f));
			IntentBox->SetPadding(FMargin(9.f, 4.f));
			IntentBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			UHorizontalBox* IntentRow = WidgetTree->ConstructWidget<UHorizontalBox>();
			IntentBox->SetContent(IntentRow);

			UImage* Icon = WidgetTree->ConstructWidget<UImage>();
			Icon->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
			IntentRow->AddChildToHorizontalBox(Icon)->SetVerticalAlignment(VAlign_Center);

			UTextBlock* Number = WidgetTree->ConstructWidget<UTextBlock>();
			SetOutlinedFont(Number, 20);
			if (UHorizontalBoxSlot* S = IntentRow->AddChildToHorizontalBox(Number))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}

			UCanvasPanelSlot* CanvasSlot = TagLayer->AddChildToCanvas(IntentBox);
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
			Tag.IntentBox = IntentBox;
			Tag.IntentIcon = Icon;
			Tag.IntentText = Number;
		}
		else
		{
			// 플레이어: 이번 턴을 끝내면 머리 위 "대기"
			UBorder* Ready = WidgetTree->ConstructWidget<UBorder>();
			Ready->SetBrush(TerminusUI::MakeBoxBrush(FLinearColor(0.008f, 0.021f, 0.008f, 0.85f), TerminusUI::Hex(TEXT("5FC45A")), 0.f, 1.f));
			Ready->SetPadding(FMargin(8.f, 2.f));
			UTextBlock* ReadyText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT("대기"), 12, TerminusUI::Hex(TEXT("BFF0B8")));
			TerminusUI::SetBold(ReadyText);
			Ready->SetContent(ReadyText);
			Ready->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* CanvasSlot = TagLayer->AddChildToCanvas(Ready);
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
			Tag.ReadyBadge = Ready;
		}

		// 발밑: 보호막 줄 4px / 체력 줄 7px / 숫자 / 상태이상 칸
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);

		UHorizontalBox* ShieldTrack = WidgetTree->ConstructWidget<UHorizontalBox>();
		UImage* ShieldFill = WidgetTree->ConstructWidget<UImage>();
		ShieldFill->SetColorAndOpacity(TerminusUI::Hex(TEXT("8FA4B8")));
		ShieldFill->SetDesiredSizeOverride(FVector2D(0.f, 4.f));
		ShieldTrack->AddChildToHorizontalBox(ShieldFill);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(MakeSized(WidgetTree, ShieldTrack, 90.f, 4.f)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}

		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
		BarStyle.BackgroundImage = TerminusUI::MakeBoxBrush(TerminusUI::Hex(TEXT("120F16")), FLinearColor::Black, 0.f, 1.f);
		BarStyle.FillImage = FSlateBrush();
		Bar->SetWidgetStyle(BarStyle);
		Bar->SetFillColorAndOpacity(TerminusUI::Hex(bMonster ? TEXT("E8323A") : TEXT("5FC45A")));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(MakeSized(WidgetTree, Bar, 90.f, 7.f)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}

		UHorizontalBox* NumberRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* HpText = MakeHUDText(WidgetTree, TEXT(""), 13, TerminusUI::MapColor::Text);
		TerminusUI::SetBold(HpText);
		NumberRow->AddChildToHorizontalBox(HpText);
		UTextBlock* ShieldText = MakeHUDText(WidgetTree, TEXT(""), 13, TerminusUI::Hex(TEXT("A9C0D6")));
		TerminusUI::SetBold(ShieldText);
		NumberRow->AddChildToHorizontalBox(ShieldText)->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(NumberRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}

		UWrapBox* StatusWrap = WidgetTree->ConstructWidget<UWrapBox>();
		StatusWrap->SetInnerSlotPadding(FVector2D(6.f, 4.f));
		StatusWrap->SetHorizontalAlignment(HAlign_Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(MakeSized(WidgetTree, StatusWrap, 150.f, 0.f)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		UCanvasPanelSlot* BoxSlot = TagLayer->AddChildToCanvas(Box);
		BoxSlot->SetAutoSize(true);
		BoxSlot->SetAlignment(FVector2D(0.5f, 0.f));
		Tag.HealthBox = Box;
		Tag.ShieldFill = ShieldFill;
		Tag.HealthBar = Bar;
		Tag.HealthText = HpText;
		Tag.ShieldText = ShieldText;
		Tag.StatusWrap = StatusWrap;

		// 대상 선택 버튼 (대상 고르는 중일 때만 보임)
		UButton* Target = WidgetTree->ConstructWidget<UButton>();
		Target->SetStyle(MakeTargetButtonStyle());
		Target->SetBackgroundColor(TargetButtonTint(false));
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

	UpdateSkillDrag();
	UpdateTags();
	UpdatePanels();
	UpdateSharedPlan();
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

	// 드래그 중이면 커서 아래 대상을 진하게
	const int32 DragHoverTag = bDraggingSkill ? FindTargetTagUnderCursor() : INDEX_NONE;

	for (int32 TagIndex = 0; TagIndex < Tags.Num(); ++TagIndex)
	{
		FBattlerTag& Tag = Tags[TagIndex];
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

		// 머리 위 행동 예고 (아이콘 + 숫자)
		if (UBorder* IntentBox = Tag.IntentBox.Get())
		{
			const FMonsterIntent* Found = Tag.bMonster ? Combat->FindIntent(Cast<ATerminusMonster>(Battler)) : nullptr;
			const bool bVisible = bAlive && Found;
			IntentBox->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bVisible)
			{
				const int32 IconIndex = IntentIconIndex(*Found);
				if (UImage* Icon = Tag.IntentIcon.Get(); Icon && IntentTextures.IsValidIndex(IconIndex) && Icon->GetBrush().GetResourceObject() != IntentTextures[IconIndex])
				{
					Icon->SetBrushFromTexture(IntentTextures[IconIndex]);
					Icon->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
				}
				if (UTextBlock* Number = Tag.IntentText.Get())
				{
					const FString Label = IntentNumber(*Found);
					TerminusUI::SetTextIfChanged(Number, Label);
					Number->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
					Number->SetColorAndOpacity(FSlateColor(TerminusUI::Hex(Found->Kind == EMonsterIntentKind::Attack ? TEXT("FFE3DE") : TEXT("F0E8D6"))));
				}
				Place(IntentBox, HeadHeight + 8.f);
			}
		}

		// 플레이어가 이번 턴을 끝냈으면 머리 위 "대기"
		if (UBorder* Ready = Tag.ReadyBadge.Get())
		{
			const TArray<TObjectPtr<ATerminusPlayerState>>& Occupants = CurrentArea->GetOccupants();
			const ATerminusPlayerState* TagPS = Occupants.IsValidIndex(Tag.Index) ? Occupants[Tag.Index].Get() : nullptr;
			const bool bDone = bAlive && TagPS && Combat->GetPhase() == ECombatPhase::PlayerTurn && Combat->HasEndedTurn(TagPS);
			Ready->SetVisibility(bDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bDone)
			{
				Place(Ready, HeadHeight + 8.f);
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
					TerminusUI::SetTextIfChanged(HpText, FString::Printf(TEXT("%d/%d"), State.Health, MaxHealth));
				}
				// 보호막: 체력 줄 위 4px 회청색 줄 (최대 체력 대비) + 숫자 옆 "+6"
				if (UTextBlock* ShieldText = Tag.ShieldText.Get())
				{
					TerminusUI::SetTextIfChanged(ShieldText, State.Shield > 0 ? FString::Printf(TEXT("+%d"), State.Shield) : FString());
				}
				if (UImage* ShieldFill = Tag.ShieldFill.Get())
				{
					const float Width = State.Shield > 0 ? FMath::Clamp(90.f * State.Shield / MaxHealth, 2.f, 90.f) : 0.f;
					ShieldFill->SetDesiredSizeOverride(FVector2D(Width, 4.f));
				}
				RefreshStatusIcons(Tag, State.Statuses);
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

				const FLinearColor TargetColor = TargetButtonTint(TagIndex == DragHoverTag);
				if (!Target->GetBackgroundColor().Equals(TargetColor))
				{
					Target->SetBackgroundColor(TargetColor);
				}
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
		TerminusUI::SetTextIfChanged(RoomText, bBetrayal
			? FString::Printf(TEXT("%d층 배신 전투  ·  사이클 %d"), Floor, Combat->GetCycle())
			: FString::Printf(TEXT("%d층 %d번째 방  ·  사이클 %d"), Floor, CurrentArea->GetRoom().Row + 1, Combat->GetCycle()));
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
		if (bSpectating) Label = TEXT("관전 중 · ") + Label;
		TerminusUI::SetTextIfChanged(PhaseText, Label);

		// 띠 색: 승리 / 내 턴 금, 기다림 회색, 적의 턴 붉음, 배신자 진홍, 패배 어두움
		int32 Style = Phase_Mine;
		switch (Phase)
		{
		case ECombatPhase::PlayerTurn:
			Style = Label.Contains(TEXT("배신자")) || Label.Contains(TEXT("상대 편")) ? Phase_Betrayer
				: (bMyTurn || bSpectating) ? Phase_Mine : Phase_Waiting;
			break;
		case ECombatPhase::MonsterTurn: Style = Phase_Enemy; break;
		case ECombatPhase::Victory:     Style = Label.Contains(TEXT("패배")) ? Phase_Defeat : Phase_Victory; break;
		case ECombatPhase::Defeat:      Style = Phase_Defeat; break;
		default: break;
		}
		ApplyPhaseStyle(Style);
	}

	if (HintText)
	{
		TerminusUI::SetTextIfChanged(HintText, bDraggingSkill ? TEXT("대상 위에서 놓으면 사용  (대상 밖에서 놓으면 취소 · Shift: 쓰지 않고 예약만 공유)")
			: PendingSkillIndex != INDEX_NONE ? TEXT("대상을 선택하세요  (우클릭: 취소 · Shift+클릭: 쓰지 않고 예약만 공유)")
			: DeclaredSkillIndex != INDEX_NONE ? TEXT("예약을 파티에 공유 중  (같은 스킬 Shift+클릭: 예약 취소)")
			: TEXT(""));

		// 안내가 없으면 판째 숨김
		if (HintPlate)
		{
			HintPlate->SetVisibility(HintText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}

	// ---- 왼쪽 아래: 에너지 + 스킬
	if (MyStats)
	{
		const FCombatState& State = MyStats->GetCombatState();
		const FCharacterStats& Stats = MyStats->GetStats();

		// 구슬 상자가 있으면 (시안) 숫자만, 없으면 "에너지 2/3"
		if (EnergyText)
		{
			TerminusUI::SetTextIfChanged(EnergyText, EnergyOrbs ? FString::Printf(TEXT("%d/%d"), State.Energy, Stats.MaxEnergy)
				: FString::Printf(TEXT("에너지 %d/%d"), State.Energy, Stats.MaxEnergy));
		}
		if (SkillEnergyText)
		{
			TerminusUI::SetTextIfChanged(SkillEnergyText, SkillEnergyOrbs ? FString::Printf(TEXT("%d/%d"), State.SkillEnergy, Stats.MaxSkillEnergy)
				: FString::Printf(TEXT("스킬 에너지 %d/%d"), State.SkillEnergy, Stats.MaxSkillEnergy));
		}
		RefreshOrbs(EnergyOrbs, ShownEnergy, State.Energy, Stats.MaxEnergy, OrbEnergyTexture, OrbEnergyEmptyTexture);
		RefreshOrbs(SkillEnergyOrbs, ShownSkillEnergy, State.SkillEnergy, Stats.MaxSkillEnergy, OrbSkillTexture, OrbSkillEmptyTexture);

		// 강화 칸 수가 캐릭터 스텟과 다르면 다시 만듦
		BuildSkillSlots(Stats.EnhanceSlots);

		for (UItemSlotWidget* SkillSlot : SkillSlots)
		{
			if (!SkillSlot) continue;

			const FSkillRow* Skill = LocalPS->GetCombatSkill(SkillSlot->GetSlotIndex());
			const bool bAffordable = Skill && State.Energy >= MyStats->GetEffectiveEnergyCost(Skill->EnergyCost) && State.SkillEnergy >= Skill->SkillEnergyCost;   // 과욕이면 +1
			SkillSlot->SetSkill(LocalPS->GetCombatSkillRow(SkillSlot->GetSlotIndex()));
			SkillSlot->SetUsable(Skill && bMyTurn && bAffordable);
			const bool bTargeting = PendingSkillIndex == SkillSlot->GetSlotIndex();
			SkillSlot->SetSelected(bTargeting);   // 대상 고르는 중인 칸 강조

			// 시안 장식: 비용 원 / 강화 / 층 마름모, 칸 띄우기 (호버 4px / 대상 고르는 중 10px), 내 턴 아니면 흐림
			const int32 DecoIndex = SkillSlots.IndexOfByKey(SkillSlot);
			if (SkillDecos.IsValidIndex(DecoIndex))
			{
				FSkillDeco& Deco = SkillDecos[DecoIndex];
				RefreshSkillDeco(Deco, Skill, SkillSlot->GetSlotIndex(), bAffordable);
				if (UOverlay* Holder = Deco.Holder.Get())
				{
					const float Lift = bTargeting ? -10.f : (Skill && bMyTurn && SkillSlot->IsHovered()) ? -4.f : 0.f;
					if (Holder->GetRenderTransform().Translation.Y != Lift)
					{
						Holder->SetRenderTranslation(FVector2D(0.f, Lift));
					}
					Holder->SetRenderOpacity(Skill && !bMyTurn ? 0.42f : 1.f);
				}
			}
		}

		// ---- 오른쪽 아래: 상태. 기획 시안 "체력 [최대/현재] 힘 방어 회피"
		if (StatusRow)
		{
			// 시안: "체력 21/30  보호막 6  힘 5  방어 3  회피 1" 이름은 흐리게, 값은 굵게
			TArray<TPair<FString, FString>> Pairs;
			Pairs.Emplace(TEXT("체력"), FString::Printf(TEXT("%d/%d"), State.Health, Stats.MaxHealth));
			if (State.Shield > 0) Pairs.Emplace(TEXT("보호막"), FString::FromInt(State.Shield));
			Pairs.Emplace(TEXT("힘"), FString::FromInt(Stats.Attack));
			Pairs.Emplace(TEXT("방어"), FString::FromInt(Stats.Defense));
			Pairs.Emplace(TEXT("회피"), FString::FromInt(Stats.Evasion));

			FString Key;
			for (const TPair<FString, FString>& Pair : Pairs) Key += Pair.Key + Pair.Value + TEXT("|");
			if (Key != ShownStatusLine)
			{
				ShownStatusLine = Key;
				StatusRow->ClearChildren();
				for (int32 i = 0; i < Pairs.Num(); ++i)
				{
					UTextBlock* Name = TerminusUI::MakeText(WidgetTree, NAME_None, Pairs[i].Key, 14, TerminusUI::MapColor::TextSub, ETextJustify::Left);
					const bool bShield = Pairs[i].Key == TEXT("보호막");
					UTextBlock* Value = TerminusUI::MakeText(WidgetTree, NAME_None, Pairs[i].Value, 14,
						bShield ? TerminusUI::Hex(TEXT("A9C0D6")) : TerminusUI::MapColor::Text, ETextJustify::Left);
					TerminusUI::SetBold(Value);
					if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(StatusRow->AddChild(Name)))
					{
						S->SetPadding(FMargin(i == 0 ? 0.f : 14.f, 0.f, 4.f, 0.f));
					}
					StatusRow->AddChild(Value);
				}
			}
		}
		else if (StatusText)
		{
			FString Label = FString::Printf(TEXT("체력 %d/%d"), State.Health, Stats.MaxHealth);
			if (State.Shield > 0) Label += FString::Printf(TEXT("  보호막 %d"), State.Shield);
			Label += FString::Printf(TEXT("  힘 %d  방어 %d  회피 %d"), Stats.Attack, Stats.Defense, Stats.Evasion);
			TerminusUI::SetTextIfChanged(StatusText, Label);
		}
	}

	if (EndTurnButton)
	{
		EndTurnButton->SetIsEnabled(bMyTurn);
	}

	// 턴 종료 판: 내 턴이면 붉은 봉랍 + "턴 종료", 아니면 꺼진 봉랍 + "대기 중…"
	if (EndTurnLabel)
	{
		TerminusUI::SetTextIfChanged(EndTurnLabel, bMyTurn ? TEXT("턴 종료") : TEXT("대기 중…"));
		const bool bHover = bMyTurn && EndTurnButton && EndTurnButton->IsHovered();
		EndTurnLabel->SetColorAndOpacity(FSlateColor(TerminusUI::Hex(!bMyTurn ? TEXT("6A6470") : bHover ? TEXT("FFF6D0") : TEXT("F0E8D6"))));
	}
	if (EndTurnSeal)
	{
		UTexture2D* Wanted = bMyTurn ? SealTexture : SealOffTexture;
		if (Wanted && EndTurnSeal->GetBrush().GetResourceObject() != Wanted)
		{
			EndTurnSeal->SetBrushFromTexture(Wanted);
			EndTurnSeal->SetDesiredSizeOverride(FVector2D(46.f, 46.f));
		}
	}
}

// =====================================================================
// 시안 부품
// =====================================================================

void UCombatHUDWidget::LoadCombatTextures()
{
	OrbEnergyTexture = LoadCombatTexture(TEXT("T_Orb_Energy"));
	OrbEnergyEmptyTexture = LoadCombatTexture(TEXT("T_Orb_EnergyEmpty"));
	OrbSkillTexture = LoadCombatTexture(TEXT("T_Orb_Skill"));
	OrbSkillEmptyTexture = LoadCombatTexture(TEXT("T_Orb_SkillEmpty"));
	SealTexture = LoadCombatTexture(TEXT("T_EndTurn_Seal"));
	SealOffTexture = LoadCombatTexture(TEXT("T_EndTurn_SealOff"));

	IntentTextures.Reset();
	for (const TCHAR* Name : { TEXT("T_Intent_Attack"), TEXT("T_Intent_Defense"), TEXT("T_Intent_Buff"), TEXT("T_Intent_Debuff") })
	{
		IntentTextures.Add(LoadCombatTexture(Name));
	}
}

void UCombatHUDWidget::ApplyPhaseStyle(int32 PhaseStyle)
{
	if (PhaseStyle == ShownPhaseStyle || PhaseStyle < 0 || PhaseStyle >= UE_ARRAY_COUNT(PhaseLooks)) return;
	ShownPhaseStyle = PhaseStyle;

	const FPhaseLook& Look = PhaseLooks[PhaseStyle];
	if (PhaseText)
	{
		PhaseText->SetColorAndOpacity(FSlateColor(TerminusUI::Hex(Look.Text)));
	}
	if (PhaseBandFill)
	{
		PhaseBandFill->SetColorAndOpacity(TerminusUI::Hex(Look.Band, Look.BandAlpha));
	}
	for (UImage* Line : { PhaseLineTop.Get(), PhaseLineBottom.Get() })
	{
		if (Line) Line->SetColorAndOpacity(TerminusUI::Hex(Look.Line));
	}
}

void UCombatHUDWidget::RefreshOrbs(UPanelWidget* Box, FIntPoint& Shown, int32 Current, int32 Max, UTexture2D* Full, UTexture2D* Empty)
{
	if (!Box) return;
	const FIntPoint Wanted(Current, Max);
	if (Wanted == Shown) return;
	Shown = Wanted;

	// 구슬 18px (그림은 빛 여백까지 27px). 찬 것 / 빈 것
	Box->ClearChildren();
	for (int32 i = 0; i < Max; ++i)
	{
		UImage* Orb = WidgetTree->ConstructWidget<UImage>();
		if (UTexture2D* Texture = i < Current ? Full : Empty)
		{
			Orb->SetBrushFromTexture(Texture);
		}
		Orb->SetDesiredSizeOverride(FVector2D(27.f, 27.f));
		if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Box->AddChild(Orb)))
		{
			S->SetPadding(FMargin(-4.5f));
		}
		else
		{
			Box->AddChild(Orb);
		}
	}
}

void UCombatHUDWidget::RefreshStatusIcons(FBattlerTag& Tag, const TArray<FStatusInstance>& Statuses)
{
	UWrapBox* Wrap = Tag.StatusWrap.Get();
	if (!Wrap) return;

	// 종류 + 수치가 그대로면 다시 안 그림
	TArray<uint32> Key;
	for (const FStatusInstance& Status : Statuses)
	{
		if (Status.Type == EStatusEffect::None) continue;
		Key.Add((static_cast<uint32>(Status.Type) << 24) | (static_cast<uint32>(FMath::Clamp(Status.Value, 0, 0xFFF)) << 12) | static_cast<uint32>(FMath::Clamp(Status.Duration, 0, 0xFFF)));
	}
	if (Key == Tag.ShownStatuses) return;
	Tag.ShownStatuses = Key;

	Wrap->ClearChildren();
	for (const FStatusInstance& Status : Statuses)
	{
		if (Status.Type == EStatusEffect::None) continue;
		const FStatusLook Look = GetStatusLook(Status.Type);
		const FLinearColor Color = TerminusUI::Hex(Look.Hex);

		// [글자 칸 16px] 수치
		UHorizontalBox* Item = WidgetTree->ConstructWidget<UHorizontalBox>();
		UBorder* Cell = WidgetTree->ConstructWidget<UBorder>();
		Cell->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::Hex(TEXT("16131C")), Color, 0.f, 1.f));
		Cell->SetPadding(FMargin(0.f));
		Cell->SetHorizontalAlignment(HAlign_Center);
		Cell->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Letter = TerminusUI::MakeText(WidgetTree, NAME_None, Look.Letter, 8, Color);
		TerminusUI::SetBold(Letter);
		Cell->SetContent(Letter);
		Item->AddChildToHorizontalBox(MakeSized(WidgetTree, Cell, 16.f, 16.f))->SetVerticalAlignment(VAlign_Center);

		// 수치가 없는 상태(불사 / 불굴 등)는 남은 턴
		const int32 Shown = Status.Value > 0 ? Status.Value : Status.Duration;
		UTextBlock* Count = MakeHUDText(WidgetTree, Shown > 0 ? FString::FromInt(Shown) : FString(), 11, TerminusUI::MapColor::Text);
		TerminusUI::SetBold(Count);
		if (UHorizontalBoxSlot* S = Item->AddChildToHorizontalBox(Count))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(2.f, 0.f, 0.f, 0.f));
		}
		Wrap->AddChild(Item);
	}
}

void UCombatHUDWidget::RefreshSkillDeco(FSkillDeco& Deco, const FSkillRow* Skill, int32 SlotIndex, bool bAffordable)
{
	UBorder* Cost = Deco.CostBadge.Get();
	UTextBlock* CostText = Deco.CostText.Get();
	UBorder* Upgrade = Deco.UpgradeBadge.Get();
	UPanelWidget* Pips = Deco.TierPips.Get();

	if (!Skill)
	{
		if (Cost) Cost->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
		if (Upgrade) Upgrade->SetVisibility(ESlateVisibility::Collapsed);
		if (Pips) Pips->ClearChildren();
		Deco.ShownSkill = NAME_None;
		return;
	}

	// 비용 원: 스킬 에너지가 드는 스킬은 보라, 아니면 에너지 파랑. 모자라면 어두운 원 + 붉은 숫자
	if (Cost && CostText)
	{
		const bool bSkillEnergy = Skill->SkillEnergyCost > 0;
		const FLinearColor Fill = !bAffordable ? TerminusUI::Hex(TEXT("1A1820")) : TerminusUI::Hex(bSkillEnergy ? TEXT("7C45C8") : TEXT("2F6BD0"));
		Cost->SetBrush(TerminusUI::MakeBoxBrush(Fill, TerminusUI::Hex(TEXT("0E0D12")), 12.f, 2.f));
		TerminusUI::SetTextIfChanged(CostText, FString::FromInt(bSkillEnergy ? Skill->SkillEnergyCost : Skill->EnergyCost));
		CostText->SetColorAndOpacity(FSlateColor(bAffordable ? FLinearColor::White : TerminusUI::Hex(TEXT("FF4A3A"))));
		Cost->GetParent()->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	const FName Row = GetOwningPlayer() && GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>()
		? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>()->GetCombatSkillRow(SlotIndex) : NAME_None;
	if (Row == Deco.ShownSkill) return;
	Deco.ShownSkill = Row;

	// 강화 +N: 기본 칸은 훈련소 단계, 강화 칸은 픽업 스킬 단계
	if (Upgrade && Deco.UpgradeText.IsValid())
	{
		const ATerminusPlayerState* PS = GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>();
		int32 Level = 0;
		if (SlotIndex < ATerminusPlayerState::NumBasicSkills)
		{
			const TArray<int32>& Levels = PS->GetRunState().BasicSkillLevels;
			Level = Levels.IsValidIndex(SlotIndex) ? Levels[SlotIndex] : 0;
		}
		else
		{
			Level = PS->GetPickupSkillLevel(Row);
		}
		Deco.UpgradeText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Level)));
		Upgrade->SetVisibility(Level > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 층 마름모: 표층 1 흐림 / 중층 2 금 / 심층 3 적
	if (Pips)
	{
		Pips->ClearChildren();
		const int32 Count = static_cast<int32>(Skill->SkillTier) + 1;
		const FLinearColor PipColor = TerminusUI::Hex(Skill->SkillTier == ESkillTier::Deep ? TEXT("FF4A3A") : Skill->SkillTier == ESkillTier::Mid ? TEXT("FFD978") : TEXT("A89F8C"));
		for (int32 i = 0; i < Count; ++i)
		{
			UImage* Pip = WidgetTree->ConstructWidget<UImage>();
			Pip->SetColorAndOpacity(PipColor);
			Pip->SetDesiredSizeOverride(FVector2D(6.f, 6.f));
			Pip->SetRenderTransformAngle(45.f);
			if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(Pips->AddChild(Pip)))
			{
				S->SetPadding(FMargin(i == 0 ? 0.f : 3.f, 0.f, 0.f, 0.f));
			}
		}
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

	const bool bShift = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();

	// 예약한 스킬을 Shift + 클릭 = 예약 취소
	if (bShift && DeclaredSkillIndex == SkillIndex)
	{
		ClearDeclaredPlan();
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

	CancelTargeting();

	// Shift: 쓰지 않고 예약만 공유
	if (bShift)
	{
		DeclarePlan(SkillIndex, nullptr);
		return;
	}

	// 대상이 정해져 있는 스킬(자신 / 전체)은 바로 사용
	PC->Server_UseSkill(SkillIndex, INDEX_NONE);
	NotifySkillUsed();
}

void UCombatHUDWidget::CancelTargeting()
{
	// 드래그 중이었으면 그것도 끝냄 (칸 버튼은 아직 눌려 있을 수 있음 -> 손을 뗄 때까지 같은 누름으로 다시 시작하지 않게)
	if (bDraggingSkill)
	{
		bDraggingSkill = false;
		bSkillPressConsumed = true;
		if (UTextBlock* Ghost = DragGhost.Get())
		{
			Ghost->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (PendingSkillIndex == INDEX_NONE) return;

	PendingSkillIndex = INDEX_NONE;
	UpdateVisibility();
}

// =====================================================================
// 스킬 드래그: 칸을 누른 채 끌어서 대상 위에서 놓기
// =====================================================================

void UCombatHUDWidget::UpdateSkillDrag()
{
	if (!FSlateApplication::IsInitialized()) return;
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	if (bDraggingSkill)
	{
		// 놓았으면(칸 버튼이 더는 안 눌림) 그 자리 대상에게
		const TObjectPtr<UItemSlotWidget>* Held = SkillSlots.FindByPredicate([this](const TObjectPtr<UItemSlotWidget>& S)
		{
			return S && S->GetSlotIndex() == PressedSkillIndex;
		});
		if (!Held || !*Held || !(*Held)->IsPressed())
		{
			FinishSkillDrag();
			return;
		}

		// 커서를 따라다니는 스킬 이름
		if (UTextBlock* Ghost = DragGhost.Get())
		{
			if (UCanvasPanelSlot* GhostSlot = Cast<UCanvasPanelSlot>(Ghost->Slot))
			{
				GhostSlot->SetPosition(UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + FVector2D(18.f, 10.f));
			}
		}
		return;
	}

	// 누르고 있는 칸 찾기 (비용 부족 / 내 턴 아님이면 칸 버튼이 꺼져 있어 안 눌림)
	const UItemSlotWidget* Pressed = nullptr;
	for (const UItemSlotWidget* SkillSlot : SkillSlots)
	{
		if (SkillSlot && SkillSlot->IsPressed())
		{
			Pressed = SkillSlot;
			break;
		}
	}

	if (!Pressed)
	{
		PressedSkillIndex = INDEX_NONE;
		bSkillPressConsumed = false;
		return;
	}

	// 새로 누르기 시작함 -> 시작 위치 기억
	if (Pressed->GetSlotIndex() != PressedSkillIndex)
	{
		PressedSkillIndex = Pressed->GetSlotIndex();
		PressStartPos = CursorPos;
		bSkillPressConsumed = false;
		return;
	}

	if (bSkillPressConsumed) return;

	// 조금 움직인 건 클릭으로 둠. 드래그로 쓸 수 있는 건 대상을 고르는 스킬(적 1명 / 아군 1명)만
	if (FVector2D::Distance(CursorPos, PressStartPos) < SkillDragStartDistance) return;

	const ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const FSkillRow* Skill = LocalPS ? LocalPS->GetCombatSkill(PressedSkillIndex) : nullptr;
	if (!Skill || (Skill->TargetType != ETargetType::SingleEnemy && Skill->TargetType != ETargetType::SingleAlly)) return;

	// 관전 중인 구역에선 못 씀 (클릭과 같은 규칙)
	if (const ADungeonArea* Viewed = Area.Get(); !Viewed || !Viewed->GetOccupants().ContainsByPredicate(
		[LocalPS](const TObjectPtr<ATerminusPlayerState>& P) { return P.Get() == LocalPS; }))
	{
		return;
	}

	// 대상 고르기 모드로 들어감 -> 대상 버튼이 보이고 동료 화면에도 계획이 보임
	bDraggingSkill = true;
	PendingSkillIndex = PressedSkillIndex;
	UpdateVisibility();

	if (!DragGhost.IsValid() && TagLayer)
	{
		UTextBlock* Ghost = MakeHUDText(WidgetTree, TEXT(""), TerminusUI::FontSize::Subtitle, TerminusUI::Color::Gold);
		TerminusUI::AddShadow(Ghost);
		if (UCanvasPanelSlot* GhostSlot = TagLayer->AddChildToCanvas(Ghost))
		{
			GhostSlot->SetAutoSize(true);
			GhostSlot->SetZOrder(100);
		}
		DragGhost = Ghost;
	}
	if (UTextBlock* Ghost = DragGhost.Get())
	{
		Ghost->SetText(Skill->DisplayName_KR);
		Ghost->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UCombatHUDWidget::FinishSkillDrag()
{
	const int32 TagIndex = FindTargetTagUnderCursor();
	const int32 SkillIndex = PendingSkillIndex;
	const bool bShift = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();

	PressedSkillIndex = INDEX_NONE;
	CancelTargeting();   // 드래그 / 대상 고르기 끝 (대상 밖에서 놓았으면 여기서 그냥 취소)

	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!PC || SkillIndex == INDEX_NONE || !Tags.IsValidIndex(TagIndex)) return;

	// Shift: 쓰지 않고 이 대상으로 예약만 공유 (Shift + 클릭과 같음)
	if (bShift)
	{
		DeclarePlan(SkillIndex, Tags[TagIndex].Battler.Get());
		return;
	}

	PC->Server_UseSkill(SkillIndex, Tags[TagIndex].Index);
	NotifySkillUsed();
}

int32 UCombatHUDWidget::FindTargetTagUnderCursor() const
{
	if (!FSlateApplication::IsInitialized()) return INDEX_NONE;
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	for (int32 i = 0; i < Tags.Num(); ++i)
	{
		const UButton* Target = Tags[i].TargetButton.Get();
		if (Target && Target->IsVisible() && Target->GetCachedGeometry().IsUnderLocation(CursorPos))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UCombatHUDWidget::HandleEndTurnClicked()
{
	CancelTargeting();
	ClearDeclaredPlan();

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->ClearCombatPlan();
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
			// Shift: 쓰지 않고 이 대상으로 예약만 공유
			if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown())
			{
				const int32 SkillIndex = PendingSkillIndex;
				CancelTargeting();
				DeclarePlan(SkillIndex, Tag.Battler.Get());
				return;
			}

			PC->Server_UseSkill(PendingSkillIndex, Tag.Index);
			CancelTargeting();
			NotifySkillUsed();
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

// =====================================================================
// 행동 계획 공유 (표시는 UPartyCommsWidget)
// =====================================================================

void UCombatHUDWidget::DeclarePlan(int32 SkillIndex, AActor* Target)
{
	const ADungeonArea* CurrentArea = Area.Get();
	const UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;
	if (!Combat) return;

	DeclaredSkillIndex = SkillIndex;
	DeclaredTarget = Target;
	DeclaredCycle = Combat->GetCycle();
}

void UCombatHUDWidget::ClearDeclaredPlan()
{
	DeclaredSkillIndex = INDEX_NONE;
	DeclaredTarget.Reset();
	DeclaredCycle = INDEX_NONE;
}

void UCombatHUDWidget::NotifySkillUsed()
{
	ClearDeclaredPlan();
	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->ClearCombatPlan();
	}
}

ATerminusBattler* UCombatHUDWidget::GetHoveredTarget() const
{
	// 드래그 중엔 칸 버튼이 마우스를 잡고 있어 hover 가 안 바뀜 -> 화면 영역으로 판정
	if (bDraggingSkill)
	{
		const int32 TagIndex = FindTargetTagUnderCursor();
		return Tags.IsValidIndex(TagIndex) ? Tags[TagIndex].Battler.Get() : nullptr;
	}

	for (const FBattlerTag& Tag : Tags)
	{
		const UButton* Target = Tag.TargetButton.Get();
		if (Target && Target->IsVisible() && Target->IsHovered())
		{
			return Tag.Battler.Get();
		}
	}
	return nullptr;
}

void UCombatHUDWidget::UpdateSharedPlan()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* LocalPS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const ADungeonArea* CurrentArea = Area.Get();
	const UDungeonCombatComponent* Combat = CurrentArea ? CurrentArea->GetCombat() : nullptr;
	if (!PC || !LocalPS || !Combat) return;

	const bool bInArea = CurrentArea->GetOccupants().ContainsByPredicate(
		[LocalPS](const TObjectPtr<ATerminusPlayerState>& P) { return P.Get() == LocalPS; });
	const ATerminusBattler* MyBattler = Cast<ATerminusBattler>(LocalPS->GetPawn());
	const UCombatStatsComponent* MyStats = MyBattler ? MyBattler->GetCombatStats() : nullptr;

	// 계획은 내가 행동할 수 있을 때만 (관전 / 남의 차례 / 턴 끝냄 / 쓰러짐이면 지움)
	const bool bCanPlan = bInArea && MyStats && !MyStats->IsDead() && Combat->IsTurnOf(LocalPS) && !Combat->HasEndedTurn(LocalPS);

	if (!bCanPlan || DeclaredCycle != Combat->GetCycle())
	{
		ClearDeclaredPlan();
	}

	if (!bCanPlan)
	{
		PC->UpdateCombatPlan(INDEX_NONE, nullptr, false);
	}
	else if (PendingSkillIndex != INDEX_NONE)
	{
		PC->UpdateCombatPlan(PendingSkillIndex, GetHoveredTarget(), false);
	}
	else if (DeclaredSkillIndex != INDEX_NONE)
	{
		PC->UpdateCombatPlan(DeclaredSkillIndex, DeclaredTarget.Get(), true);
	}
	else
	{
		PC->UpdateCombatPlan(INDEX_NONE, nullptr, false);
	}
}
