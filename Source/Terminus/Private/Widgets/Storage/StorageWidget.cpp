#include "Widgets/Storage/StorageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
		Font.Size = 15;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Button->SetContent(Text);
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

	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		StorageChangedHandle = Profile->OnStorageChanged.AddUObject(this, &UStorageWidget::Refresh);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UStorageWidget::NativeDestruct()
{
	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->OnStorageChanged.Remove(StorageChangedHandle);
	}
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	Super::NativeDestruct();
}

void UStorageWidget::BuildDefaultLayout()
{
	// 화면 전체 어둡게 + 가운데 창
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StorageDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>();
	WindowSize->SetWidthOverride(980.f);
	WindowSize->SetHeightOverride(640.f);
	Dim->SetContent(WindowSize);

	UBorder* Window = WidgetTree->ConstructWidget<UBorder>();
	Window->SetBrushColor(FLinearColor(0.11f, 0.1f, 0.09f, 1.f));
	Window->SetPadding(FMargin(20.f));
	WindowSize->SetContent(Window);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(Column);

	// ---- 위: 제목 / 탭 / 개수 / 닫기
	UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Top))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	}

	UTextBlock* Title = MakeStorageText(WidgetTree, TEXT("TitleText"), TEXT("창고"), 22, FLinearColor::White);
	Title->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(Title))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
	}

	RelicTabButton = MakeStorageButton(WidgetTree, TEXT("RelicTabButton"), TEXT("  유물  "));
	SkillTabButton = MakeStorageButton(WidgetTree, TEXT("SkillTabButton"), TEXT("  스킬  "));
	if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(RelicTabButton)) S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	Top->AddChildToHorizontalBox(SkillTabButton);

	CountText = MakeStorageText(WidgetTree, TEXT("CountText"), TEXT(""), 14, FLinearColor(0.75f, 0.75f, 0.75f));
	CountText->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(CountText))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(16.f, 0.f));
	}

	CloseButton = MakeStorageButton(WidgetTree, TEXT("CloseButton"), TEXT("닫기"));
	Top->AddChildToHorizontalBox(CloseButton);

	// ---- 아래: 격자 (스크롤) | 상세
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Body))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Scroll))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemGrid"));
	ItemGrid->SetSlotPadding(FMargin(4.f));
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
// 열기 / 탭 / 갱신
// =====================================================================

void UStorageWidget::Open(EStorageTab Tab)
{
	SetVisibility(ESlateVisibility::Visible);
	SetTab(Tab);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &UStorageWidget::Close));
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

void UStorageWidget::SetTab(EStorageTab Tab)
{
	CurrentTab = Tab;

	if (RelicTabButton) RelicTabButton->SetBackgroundColor(Tab == EStorageTab::Relic ? ActiveTabColor : InactiveTabColor);
	if (SkillTabButton) SkillTabButton->SetBackgroundColor(Tab == EStorageTab::Skill ? ActiveTabColor : InactiveTabColor);

	Refresh();
}

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

void UStorageWidget::HandleRelicTab() { SetTab(EStorageTab::Relic); }
void UStorageWidget::HandleSkillTab() { SetTab(EStorageTab::Skill); }
void UStorageWidget::HandleClose()    { Close(); }
