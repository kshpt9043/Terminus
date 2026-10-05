#include "Widgets/Storage/StorageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/RelicTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Widgets/Common/ItemSlotWidget.h"

namespace
{
	UTextBlock* MakeStorageText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	}

	UButton* MakeStorageButton(UWidgetTree* Tree, const FName& Name, const FString& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Label));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 16;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);
		Button->SetBackgroundColor(FLinearColor(0.18f, 0.18f, 0.18f));
		return Button;
	}

	// 큰 카드 버튼 (시안의 회색 상자, 아래쪽에 이름)
	UButton* MakeStorageCard(UWidgetTree* Tree, const FName& Name, const FString& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(FLinearColor(0.35f, 0.35f, 0.35f));

		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(220.f);
		Size->SetHeightOverride(320.f);
		Button->SetContent(Size);

		UOverlay* Inner = Tree->ConstructWidget<UOverlay>();
		Size->SetContent(Inner);

		UTextBlock* Text = MakeStorageText(Tree, NAME_None, Label, 24, FLinearColor::White);
		Text->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* S = Inner->AddChildToOverlay(Text))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}
		return Button;
	}

	FString StorageTierLabel(ERelicTier Tier)
	{
		switch (Tier)
		{
		case ERelicTier::Basic:   return TEXT("기본");
		case ERelicTier::Upgrade: return TEXT("업그레이드");
		case ERelicTier::Mid:     return TEXT("중층");
		case ERelicTier::Deep:    return TEXT("심층");
		default:                  return TEXT("표층");
		}
	}

	FString StorageSkillTypeLabel(ESkillType Type)
	{
		switch (Type)
		{
		case ESkillType::Attack:  return TEXT("공격");
		case ESkillType::Defense: return TEXT("방어");
		default:                  return TEXT("특수");
		}
	}

	FString StorageOwnerLabel(ESkillOwner Owner)
	{
		switch (Owner)
		{
		case ESkillOwner::Fighter:  return TEXT("무도가");
		case ESkillOwner::Engineer: return TEXT("마도 공학자");
		case ESkillOwner::Paladin:  return TEXT("성기사");
		case ESkillOwner::Assassin: return TEXT("암살자");
		default:                    return TEXT("공용");
		}
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UStorageWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !ItemGrid)
	{
		BuildDefaultLayout();
	}

	if (RelicTabButton) RelicTabButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleRelicTab);
	if (SkillTabButton) SkillTabButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleSkillTab);
	if (CloseButton)    CloseButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleClose);
	if (BackButton)     BackButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleBack);
	if (MainMenuButton) MainMenuButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleClose);

	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		StorageChangedHandle = Profile->OnStorageChanged.AddUObject(this, &UStorageWidget::Refresh);
		Profile->OnGoldChanged.AddUniqueDynamic(this, &UStorageWidget::HandleGoldChanged);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UStorageWidget::NativeDestruct()
{
	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->OnStorageChanged.Remove(StorageChangedHandle);
		Profile->OnGoldChanged.RemoveDynamic(this, &UStorageWidget::HandleGoldChanged);
	}
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	Super::NativeDestruct();
}

void UStorageWidget::BuildDefaultLayout()
{
	// 화면 전체 (시안: 회색 바탕, 가운데 제목, 우상단 골드 / 버튼)
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StorageBackground"));
	Background->SetBrushColor(FLinearColor(0.07f, 0.065f, 0.06f, 0.97f));
	Background->SetPadding(FMargin(48.f, 32.f));
	WidgetTree->RootWidget = Background;

	UOverlay* Stage = WidgetTree->ConstructWidget<UOverlay>();
	Background->SetContent(Stage);

	// ---- 우상단: 보유 골드 + 버튼
	UVerticalBox* Corner = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* S = Stage->AddChildToOverlay(Corner))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Top);
	}

	GoldText = MakeStorageText(WidgetTree, TEXT("GoldText"), TEXT(""), 14, FLinearColor(1.f, 0.85f, 0.35f));
	GoldText->SetAutoWrapText(false);
	if (UVerticalBoxSlot* S = Corner->AddChildToVerticalBox(GoldText)) S->SetHorizontalAlignment(HAlign_Left);

	auto AddCornerButton = [&](const FName& Name, const FString& Label) -> UButton*
	{
		UButton* Button = MakeStorageButton(WidgetTree, Name, Label);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(170.f);
		Size->SetHeightOverride(46.f);
		Size->SetContent(Button);
		if (UVerticalBoxSlot* S = Corner->AddChildToVerticalBox(Size)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		return Button;
	};
	CloseButton    = AddCornerButton(TEXT("CloseButton"), TEXT("돌아가기"));
	BackButton     = AddCornerButton(TEXT("BackButton"), TEXT("돌아가기"));
	MainMenuButton = AddCornerButton(TEXT("MainMenuButton"), TEXT("메인화면"));

	// ---- 첫 화면: 제목 + 카드 두 장
	UVerticalBox* Hub = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HubPanel"));
	HubPanel = Hub;
	if (UOverlaySlot* S = Stage->AddChildToOverlay(Hub))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UTextBlock* HubTitle = MakeStorageText(WidgetTree, NAME_None, TEXT("창고"), 30, FLinearColor::White);
	HubTitle->SetAutoWrapText(false);
	if (UVerticalBoxSlot* S = Hub->AddChildToVerticalBox(HubTitle)) S->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Hub->AddChildToVerticalBox(Cards))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	RelicTabButton = MakeStorageCard(WidgetTree, TEXT("RelicTabButton"), TEXT("유물 창고"));
	SkillTabButton = MakeStorageCard(WidgetTree, TEXT("SkillTabButton"), TEXT("스킬 창고"));
	if (UHorizontalBoxSlot* S = Cards->AddChildToHorizontalBox(RelicTabButton)) S->SetPadding(FMargin(0.f, 0.f, 40.f, 0.f));
	Cards->AddChildToHorizontalBox(SkillTabButton);

	// ---- 목록 화면: 제목 / 개수 + 격자 | 상세
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ListPanel"));
	ListPanel = List;
	if (UOverlaySlot* S = Stage->AddChildToOverlay(List))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	ListTitleText = MakeStorageText(WidgetTree, TEXT("ListTitleText"), TEXT(""), 30, FLinearColor::White);
	ListTitleText->SetAutoWrapText(false);
	if (UVerticalBoxSlot* S = List->AddChildToVerticalBox(ListTitleText)) S->SetHorizontalAlignment(HAlign_Center);

	CountText = MakeStorageText(WidgetTree, TEXT("CountText"), TEXT(""), 14, FLinearColor(0.75f, 0.75f, 0.75f));
	CountText->SetAutoWrapText(false);
	if (UVerticalBoxSlot* S = List->AddChildToVerticalBox(CountText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 18.f));
	}

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = List->AddChildToVerticalBox(Body))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
	}

	UBorder* GridBox = WidgetTree->ConstructWidget<UBorder>();
	GridBox->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.3f, 1.f));
	GridBox->SetPadding(FMargin(16.f));
	Body->AddChildToHorizontalBox(GridBox);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetAlwaysShowScrollbar(true);
	GridBox->SetContent(Scroll);
	ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemGrid"));
	ItemGrid->SetSlotPadding(FMargin(6.f));
	Scroll->AddChild(ItemGrid);

	USizeBox* DetailSize = WidgetTree->ConstructWidget<USizeBox>();
	DetailSize->SetWidthOverride(280.f);
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(DetailSize))
	{
		S->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	}
	UBorder* DetailBox = WidgetTree->ConstructWidget<UBorder>();
	DetailBox->SetBrushColor(FLinearColor(0.06f, 0.05f, 0.05f, 1.f));
	DetailBox->SetPadding(FMargin(14.f));
	DetailSize->SetContent(DetailBox);

	UVerticalBox* Detail = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailBox->SetContent(Detail);
	DetailName = MakeStorageText(WidgetTree, TEXT("DetailName"), TEXT(""), 18, FLinearColor::White);
	DetailInfo = MakeStorageText(WidgetTree, TEXT("DetailInfo"), TEXT(""), 12, FLinearColor(0.75f, 0.75f, 0.75f));
	DetailDesc = MakeStorageText(WidgetTree, TEXT("DetailDesc"), TEXT(""), 14, FLinearColor(1.f, 0.95f, 0.85f));
	Detail->AddChildToVerticalBox(DetailName);
	if (UVerticalBoxSlot* S = Detail->AddChildToVerticalBox(DetailInfo)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	if (UVerticalBoxSlot* S = Detail->AddChildToVerticalBox(DetailDesc)) S->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
}

// =====================================================================
// 열기 / 화면 전환
// =====================================================================

void UStorageWidget::Open(EStorageTab Tab)
{
	SetVisibility(ESlateVisibility::Visible);
	RefreshGold();

	if (UsesHub()) ShowHub();
	else           ShowList(Tab);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &UStorageWidget::HandleEscape));
	}
}

void UStorageWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}
}

void UStorageWidget::HandleEscape()
{
	// 목록이면 첫 화면으로, 첫 화면이면 닫기 (스택 항목은 Close 에서 빠짐)
	if (bInList && UsesHub())
	{
		ShowHub();
		return;
	}
	Close();
}

void UStorageWidget::ShowHub()
{
	bInList = false;

	if (HubPanel)       HubPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (ListPanel)      ListPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (CloseButton)    CloseButton->SetVisibility(ESlateVisibility::Visible);
	if (BackButton)     BackButton->SetVisibility(ESlateVisibility::Collapsed);
	if (MainMenuButton) MainMenuButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UStorageWidget::ShowList(EStorageTab Tab)
{
	bInList = true;
	CurrentTab = Tab;

	if (UsesHub())
	{
		HubPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (CloseButton)    CloseButton->SetVisibility(ESlateVisibility::Collapsed);
		if (BackButton)     BackButton->SetVisibility(ESlateVisibility::Visible);
		if (MainMenuButton) MainMenuButton->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// 예전 탭 방식: 고른 탭에 색
		if (RelicTabButton) RelicTabButton->SetBackgroundColor(Tab == EStorageTab::Relic ? ActiveTabColor : InactiveTabColor);
		if (SkillTabButton) SkillTabButton->SetBackgroundColor(Tab == EStorageTab::Skill ? ActiveTabColor : InactiveTabColor);
	}

	if (ListPanel)     ListPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (ListTitleText) ListTitleText->SetText(FText::FromString(Tab == EStorageTab::Relic ? TEXT("유물 창고") : TEXT("스킬 창고")));

	Refresh();
}

void UStorageWidget::HandleRelicTab() { ShowList(EStorageTab::Relic); }
void UStorageWidget::HandleSkillTab() { ShowList(EStorageTab::Skill); }
void UStorageWidget::HandleClose()    { Close(); }
void UStorageWidget::HandleBack()     { if (UsesHub()) ShowHub(); else Close(); }

void UStorageWidget::HandleGoldChanged(int32 NewGold, int32 Delta)
{
	RefreshGold();
}

void UStorageWidget::RefreshGold()
{
	if (!GoldText) return;

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	GoldText->SetText(FText::Format(FText::FromString(TEXT("보유 골드: {0}")), FText::AsNumber(Profile ? Profile->GetGold() : 0)));
}

// =====================================================================
// 목록 갱신 / 상세
// =====================================================================

void UStorageWidget::Refresh()
{
	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	const TArray<FName> Items = !Profile ? TArray<FName>()
		: (CurrentTab == EStorageTab::Relic ? Profile->GetStoredRelics() : Profile->GetOwnedSkills());

	if (CountText)
	{
		CountText->SetText(FText::FromString(FString::Printf(TEXT("%s %d개"),
			CurrentTab == EStorageTab::Relic ? TEXT("보관 유물") : TEXT("보유 스킬"), Items.Num())));
	}

	if (!ItemGrid) return;

	// 칸 수: 최소 칸 수 이상, 줄 단위로 맞춤 (빈 칸도 보여서 인벤토리처럼)
	const int32 Cols = FMath::Max(1, Columns);
	const int32 Needed = FMath::Max(MinSlots, Items.Num());
	const int32 Total = FMath::DivideAndRoundUp(Needed, Cols) * Cols;

	// 칸 위젯은 모자랄 때만 더 만들고 재사용
	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	while (Slots.Num() < Total)
	{
		UItemSlotWidget* NewSlot = CreateWidget<UItemSlotWidget>(this, Class);
		if (!NewSlot) break;
		NewSlot->OnSlotClicked.BindUObject(this, &UStorageWidget::HandleSlotClicked);
		Slots.Add(NewSlot);
	}

	ItemGrid->ClearChildren();
	for (int32 i = 0; i < Total && Slots.IsValidIndex(i); ++i)
	{
		UItemSlotWidget* ItemSlot = Slots[i];
		if (!Items.IsValidIndex(i))                ItemSlot->SetEmpty();
		else if (CurrentTab == EStorageTab::Relic) ItemSlot->SetRelic(Items[i]);
		else                                        ItemSlot->SetSkill(Items[i]);

		if (UUniformGridSlot* GridSlot = ItemGrid->AddChildToUniformGrid(ItemSlot, i / Cols, i % Cols))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 첫 칸을 골라 둠 (비어 있으면 상세도 비움)
	SelectedSlot.Reset();
	HandleSlotClicked(Slots.IsValidIndex(0) && Items.Num() > 0 ? Slots[0].Get() : nullptr);
}

void UStorageWidget::HandleSlotClicked(UItemSlotWidget* InSlot)
{
	if (UItemSlotWidget* Prev = SelectedSlot.Get())
	{
		Prev->SetSelected(false);
	}

	SelectedSlot = InSlot;
	if (InSlot)
	{
		InSlot->SetSelected(true);
	}

	ShowDetail(InSlot);
}

void UStorageWidget::ShowDetail(const UItemSlotWidget* InSlot)
{
	FString Name, Info, Desc;

	if (InSlot && InSlot->GetKind() == EItemSlotKind::Relic)
	{
		if (const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(InSlot->GetItemRow()))
		{
			Name = Relic->RelicName.ToString();
			Info = FString::Printf(TEXT("유물 · %s · %s"), *StorageTierLabel(Relic->RelicTier), *StorageOwnerLabel(Relic->OwnerClass));
			if (Relic->CanSellAtSettlement())
			{
				Info += FString::Printf(TEXT("\n정산 판매가 %d 골드"), Relic->SellPrice_Gold);
			}
			Desc = Relic->RelicDesc.ToString();
		}
	}
	else if (InSlot && InSlot->GetKind() == EItemSlotKind::Skill)
	{
		if (const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(InSlot->GetItemRow()))
		{
			Name = Skill->DisplayName_KR.ToString();
			Info = FString::Printf(TEXT("스킬 · %s · %s · 강화 에너지 %d"),
				*StorageSkillTypeLabel(Skill->SkillType), *StorageOwnerLabel(Skill->OwnerClass), Skill->SkillEnergyCost);
			Desc = Skill->Description_KR.ToString().Replace(TEXT("\\n"), TEXT("\n"));
		}
	}
	else
	{
		Name = CurrentTab == EStorageTab::Relic ? TEXT("보관한 유물이 없습니다") : TEXT("보유한 스킬이 없습니다");
	}

	if (DetailName) DetailName->SetText(FText::FromString(Name));
	if (DetailInfo) DetailInfo->SetText(FText::FromString(Info));
	if (DetailDesc) DetailDesc->SetText(FText::FromString(Desc));
}
