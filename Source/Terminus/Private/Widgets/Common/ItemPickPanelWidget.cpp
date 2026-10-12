#include "Widgets/Common/ItemPickPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Data/RelicTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

void UItemPickPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !SlotBox)
	{
		BuildDefaultLayout();
	}

	if (CancelButton) CancelButton->OnClicked.AddDynamic(this, &UItemPickPanelWidget::HandleCancel);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UItemPickPanelWidget::BuildDefaultLayout()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PickColumn"));
	WidgetTree->RootWidget = Column;

	GuideText = TerminusUI::MakeWrapText(WidgetTree, TEXT("GuideText"), TEXT(""), 16, TerminusUI::Color::TextSoft);
	TerminusUI::AddRow(Column, GuideText, 0.f);

	SlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotBox"));
	TerminusUI::AddRow(Column, SlotBox, 12.f);

	CancelButton = TerminusUI::MakeButton(WidgetTree, TEXT("CancelButton"), TEXT("  취소  "), 16);
	TerminusUI::AddRow(Column, CancelButton, 12.f);
}

void UItemPickPanelWidget::Begin(const FText& Guide)
{
	if (GuideText) GuideText->SetText(Guide);
	if (SlotBox) SlotBox->ClearChildren();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

UItemSlotWidget* UItemPickPanelWidget::AddSlot()
{
	if (!SlotBox) return nullptr;

	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	UItemSlotWidget* NewSlot = CreateWidget<UItemSlotWidget>(this, Class);
	if (!NewSlot) return nullptr;

	NewSlot->SetShowName(true);
	if (!SlotSize.IsZero()) NewSlot->SetSlotSize(SlotSize);
	NewSlot->OnSlotClicked.BindUObject(this, &UItemPickPanelWidget::HandleSlotClicked);
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(SlotBox->AddChild(NewSlot)))
	{
		S->SetPadding(FMargin(SlotSpacing * 0.5f, 0.f));
	}
	return NewSlot;
}

void UItemPickPanelWidget::ShowEnhanceSkills(const FText& Guide)
{
	Begin(Guide);

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PS) return;

	const TArray<FName>& Equipped = PS->GetRunState().EnhanceSkills;
	for (int32 i = 0; i < Equipped.Num(); ++i)
	{
		if (UItemSlotWidget* SkillSlot = AddSlot())
		{
			SkillSlot->SetSlotIndex(i);
			SkillSlot->SetSkill(Equipped[i]);
		}
	}
}

void UItemPickPanelWidget::ShowRelics(const FText& Guide, TFunctionRef<bool(const FRelicRow&)> Filter)
{
	Begin(Guide);

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PS) return;

	for (const FName& Owned : PS->GetRelics())
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Owned);
		if (!Relic || !Filter(*Relic)) continue;

		if (UItemSlotWidget* RelicSlot = AddSlot())
		{
			RelicSlot->SetRelic(Owned);
		}
	}
}

void UItemPickPanelWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
	if (SlotBox) SlotBox->ClearChildren();
}

void UItemPickPanelWidget::HandleSlotClicked(UItemSlotWidget* ClickedSlot)
{
	if (ClickedSlot)
	{
		OnPicked.ExecuteIfBound(ClickedSlot);
	}
}

void UItemPickPanelWidget::HandleCancel()
{
	OnCancelled.ExecuteIfBound();
}
