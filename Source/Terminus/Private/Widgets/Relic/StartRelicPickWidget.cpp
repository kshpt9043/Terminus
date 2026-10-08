#include "Widgets/Relic/StartRelicPickWidget.h"

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
#include "Data/TerminusDataSettings.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/ItemSlotWidget.h"

namespace
{
	UTextBlock* MakeRelicPickText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}
}

void UStartRelicPickWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !ItemGrid)
	{
		BuildDefaultLayout();
	}

	if (StartButton)
	{
		StartButton->OnClicked.AddDynamic(this, &UStartRelicPickWidget::HandleStartClicked);
	}

	// 아래 지도를 못 누르게 화면 전체가 클릭을 받음
	SetVisibility(ESlateVisibility::Visible);
}

void UStartRelicPickWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RelicPickDim"));
	Dim->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>();
	WindowSize->SetWidthOverride(640.f);
	WindowSize->SetHeightOverride(600.f);
	Dim->SetContent(WindowSize);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	WindowSize->SetContent(Column);

	TitleText = MakeRelicPickText(WidgetTree, TEXT("TitleText"), 24, FLinearColor::White);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	ModeText = MakeRelicPickText(WidgetTree, TEXT("ModeText"), 13, FLinearColor(0.75f, 0.75f, 0.75f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ModeText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	CountText = MakeRelicPickText(WidgetTree, TEXT("CountText"), 16, FLinearColor(1.f, 0.85f, 0.35f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CountText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Scroll))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Center);
	}
	ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemGrid"));
	ItemGrid->SetSlotPadding(FMargin(4.f));
	Scroll->AddChild(ItemGrid);

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
	UTextBlock* StartLabel = MakeRelicPickText(WidgetTree, NAME_None, 18, FLinearColor::Black);
	StartLabel->SetText(FText::FromString(TEXT("    시작    ")));
	StartButton->SetContent(StartLabel);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StartButton))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}
}

void UStartRelicPickWidget::SetCandidates(const TArray<FName>& StoredRelics, ECharacterClass InClass, bool bInHardMode)
{
	PlayerClass = InClass;
	bHardMode = bInHardMode;
	SelectedIndices.Reset();
	MaxSelect = FMath::Clamp(MaxSelect, 1, ATerminusPlayerState::MaxStartRelics);

	if (ModeText)
	{
		// 사용자 결정 10-08: 들고 가도 창고(도감)에 남음. 하드 모드에서 죽으면 그때 창고에서 지워짐
		ModeText->SetText(FText::FromString(bHardMode
			? TEXT("하드 모드: 죽으면 들고 간 유물이 창고에서 사라집니다")
			: TEXT("들고 간 유물은 창고에 그대로 남습니다")));
		ModeText->SetColorAndOpacity(FSlateColor(bHardMode ? FLinearColor(1.f, 0.4f, 0.35f) : FLinearColor(0.75f, 0.75f, 0.75f)));
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(MaxSelect == 1
			? FString(TEXT("던전에 들고 갈 유물을 1개 고르세요 (안 골라도 됩니다)"))
			: FString::Printf(TEXT("던전에 들고 갈 유물을 고르세요 (최대 %d개)"), MaxSelect)));
	}

	if (!ItemGrid) return;
	ItemGrid->ClearChildren();

	const int32 Cols = FMath::Max(1, Columns);
	const int32 Total = FMath::DivideAndRoundUp(FMath::Max(MinSlots, StoredRelics.Num()), Cols) * Cols;
	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());

	Slots.Reset();
	for (int32 i = 0; i < Total; ++i)
	{
		UItemSlotWidget* NewSlot = CreateWidget<UItemSlotWidget>(this, Class);
		if (!NewSlot) continue;

		NewSlot->SetSlotIndex(i);
		NewSlot->OnSlotClicked.BindUObject(this, &UStartRelicPickWidget::HandleSlotClicked);
		if (StoredRelics.IsValidIndex(i)) NewSlot->SetRelic(StoredRelics[i]);
		else                              NewSlot->SetEmpty();
		Slots.Add(NewSlot);

		if (UUniformGridSlot* GridSlot = ItemGrid->AddChildToUniformGrid(NewSlot, i / Cols, i % Cols))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	RefreshStates();
}

bool UStartRelicPickWidget::IsRowSelected(FName Row) const
{
	for (const int32 Index : SelectedIndices)
	{
		if (Slots.IsValidIndex(Index) && Slots[Index]->GetItemRow() == Row) return true;
	}
	return false;
}

bool UStartRelicPickWidget::CanEquip(FName Row) const
{
	const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
	return Relic && ATerminusPlayerState::CanClassHoldRelic(PlayerClass, *Relic);
}

void UStartRelicPickWidget::RefreshStates()
{
	const bool bFull = SelectedIndices.Num() >= MaxSelect;

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		UItemSlotWidget* ItemSlot = Slots[i];
		if (!ItemSlot || ItemSlot->GetKind() != EItemSlotKind::Relic) continue;

		const bool bSelected = SelectedIndices.Contains(i);
		const FName Row = ItemSlot->GetItemRow();

		// 고른 칸은 언제나 눌러서 해제 가능. 안 고른 칸은: 장착 가능 + 같은 유물을 아직 안 골랐음 + 자리가 남음 (1개만 고르면 바꾸기 가능)
		const bool bUsable = bSelected || (CanEquip(Row) && !IsRowSelected(Row) && (!bFull || MaxSelect == 1));
		ItemSlot->SetSelected(bSelected);
		ItemSlot->SetUsable(bUsable);
	}

	if (CountText)
	{
		CountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), SelectedIndices.Num(), MaxSelect)));
	}
}

void UStartRelicPickWidget::HandleSlotClicked(UItemSlotWidget* ClickedSlot)
{
	if (bStarted || !ClickedSlot) return;

	const int32 Index = ClickedSlot->GetSlotIndex();
	if (SelectedIndices.Contains(Index))
	{
		SelectedIndices.Remove(Index);
	}
	else if (CanEquip(ClickedSlot->GetItemRow()) && !IsRowSelected(ClickedSlot->GetItemRow()))
	{
		// 1개만 고르면 바꾸기, 여러 개면 자리가 남을 때만
		if (MaxSelect == 1)
		{
			SelectedIndices.Reset();
			SelectedIndices.Add(Index);
		}
		else if (SelectedIndices.Num() < MaxSelect)
		{
			SelectedIndices.Add(Index);
		}
	}

	RefreshStates();
}

void UStartRelicPickWidget::HandleStartClicked()
{
	if (bStarted) return;
	bStarted = true;

	TArray<FName> Chosen;
	for (const int32 Index : SelectedIndices)
	{
		if (Slots.IsValidIndex(Index)) Chosen.Add(Slots[Index]->GetItemRow());
	}

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ChooseStartRelics(Chosen);
	}

	// 창고는 도감이라 들고 가도 그대로 둠. 하드 모드에서 죽으면 사망 정산 때 지움 (사용자 결정 10-08)

	RemoveFromParent();
}
