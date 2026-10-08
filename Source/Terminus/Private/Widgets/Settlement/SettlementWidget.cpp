#include "Widgets/Settlement/SettlementWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Data/RelicTypes.h"
#include "Data/SkillTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Widgets/Common/ItemSlotWidget.h"

namespace
{
	UTextBlock* MakeSettleText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);
		Text->SetAutoWrapText(true);
		return Text;
	}

	UButton* MakeSettleButton(UWidgetTree* Tree, const FName& Name, const FString& Label, const FLinearColor& Color)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Color);
		Button->SetContent(MakeSettleText(Tree, NAME_None, Label, 15, FLinearColor::White));
		return Button;
	}

	void AddSettleRow(UVerticalBox* Column, UWidget* Widget, float Top, EHorizontalAlignment Align = HAlign_Fill)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Widget))
		{
			S->SetHorizontalAlignment(Align);
			S->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		}
	}

	FString SettleGoldText(int32 Gold)
	{
		return FText::AsNumber(Gold).ToString() + TEXT(" G");
	}

	FString SettleRelicName(FName Row)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		return Relic ? Relic->RelicName.ToString() : Row.ToString();
	}

	const FLinearColor SettleTitleColor(1.f, 0.82f, 0.35f);
	const FLinearColor SettleConfirmColor(0.25f, 0.55f, 0.3f);
}

void USettlementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !RelicList)
	{
		BuildDefaultLayout();
	}

	if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &USettlementWidget::HandleConfirm);
	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &USettlementWidget::HandleClose);

	SetVisibility(ESlateVisibility::Visible);   // 아래 메인 메뉴를 못 누르게

	// 정산 내용
	if (const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Settlement = Profile->GetPendingSettlement();
	}
	Chosen = NAME_None;

	if (SubtitleText)
	{
		FString Sub = Settlement.Reason;
		if (!Settlement.RoomName.IsEmpty() || Settlement.Floor > 0)
		{
			Sub += FString::Printf(TEXT("  (%s%s%d층)"), *Settlement.RoomName, Settlement.RoomName.IsEmpty() ? TEXT("") : TEXT(" · "), Settlement.Floor);
		}
		SubtitleText->SetText(FText::FromString(Sub));
	}

	if (QuestText)
	{
		QuestText->SetText(FText::FromString(TEXT("완료한 퀘스트가 없습니다.")));
	}

	if (SkillText)
	{
		const FSkillRow* Skill = Settlement.KeptSkill.IsNone() ? nullptr : UTerminusDataSettings::FindSkillRow(Settlement.KeptSkill);
		SkillText->SetText(FText::FromString(Settlement.bDeath
			? FString(TEXT("전멸해서 강화 스킬과 던전 재화는 모두 사라졌습니다."))
			: Skill
				? FString::Printf(TEXT("[%s] 을(를) 보유 스킬로 가져갑니다."), *Skill->DisplayName_KR.ToString())
				: FString(TEXT("장착 중인 픽업 스킬을 이미 모두 보유하고 있어 건너뜁니다."))));
	}

	// 사망 정산: 유물은 이미 전부 판매된 걸로 보여 줌
	if (Settlement.bDeath)
	{
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("사망 정산")));
		if (RelicHeaderText) RelicHeaderText->SetText(FText::FromString(TEXT("1. 판매된 유물")));
		if (RelicHelpText) RelicHelpText->SetText(FText::FromString(TEXT("전멸해서 던전에서 얻은 유물은 모두 골드로 판매됩니다.")));
	}

	if (SummaryPanel) SummaryPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (MainPanel) MainPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	RebuildRelics();
	RefreshGuide();
}

void USettlementWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettlementDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.07f, 0.97f));
	Panel->SetPadding(FMargin(30.f, 24.f));
	Dim->SetContent(Panel);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(760.f);
	Panel->SetContent(Width);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	Width->SetContent(Root);

	TitleText = MakeSettleText(WidgetTree, TEXT("TitleText"), TEXT("정산"), 30, SettleTitleColor);
	AddSettleRow(Root, TitleText, 0.f);
	SubtitleText = MakeSettleText(WidgetTree, TEXT("SubtitleText"), TEXT(""), 15, FLinearColor(0.8f, 0.8f, 0.8f));
	AddSettleRow(Root, SubtitleText, 6.f);

	// ---- 정산 목록
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	MainPanel = Main;
	AddSettleRow(Root, Main, 18.f);

	RelicHeaderText = MakeSettleText(WidgetTree, TEXT("RelicHeaderText"), TEXT("1. 보관할 유물"), 19, FLinearColor::White, ETextJustify::Left);
	AddSettleRow(Main, RelicHeaderText, 0.f);
	RelicHelpText = MakeSettleText(WidgetTree, TEXT("RelicHelpText"),
		TEXT("던전에서 얻은 유물 중 하나를 골라 창고에 보관합니다. 나머지 유물은 사라집니다."),
		13, FLinearColor(0.7f, 0.7f, 0.7f), ETextJustify::Left);
	AddSettleRow(Main, RelicHelpText, 4.f);

	USizeBox* ListHeight = WidgetTree->ConstructWidget<USizeBox>();
	ListHeight->SetMaxDesiredHeight(300.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	ListHeight->SetContent(Scroll);
	UWrapBox* Wrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("RelicList"));
	Wrap->SetInnerSlotPadding(FVector2D(8.f, 8.f));
	RelicList = Wrap;
	Scroll->AddChild(Wrap);
	AddSettleRow(Main, ListHeight, 10.f);

	RelicGuideText = MakeSettleText(WidgetTree, TEXT("RelicGuideText"), TEXT(""), 15, SettleTitleColor, ETextJustify::Left);
	AddSettleRow(Main, RelicGuideText, 8.f);

	AddSettleRow(Main, MakeSettleText(WidgetTree, NAME_None, TEXT("2. 완료된 퀘스트"), 19, FLinearColor::White, ETextJustify::Left), 18.f);
	QuestText = MakeSettleText(WidgetTree, TEXT("QuestText"), TEXT(""), 14, FLinearColor(0.85f, 0.85f, 0.85f), ETextJustify::Left);
	AddSettleRow(Main, QuestText, 4.f);

	AddSettleRow(Main, MakeSettleText(WidgetTree, NAME_None, TEXT("3. 가져가는 스킬"), 19, FLinearColor::White, ETextJustify::Left), 18.f);
	SkillText = MakeSettleText(WidgetTree, TEXT("SkillText"), TEXT(""), 14, FLinearColor(0.85f, 0.85f, 0.85f), ETextJustify::Left);
	AddSettleRow(Main, SkillText, 4.f);

	ConfirmButton = MakeSettleButton(WidgetTree, TEXT("ConfirmButton"), TEXT("  정산 완료  "), SettleConfirmColor);
	AddSettleRow(Main, ConfirmButton, 22.f, HAlign_Right);

	// ---- 최종 골드
	UVerticalBox* Summary = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryPanel"));
	SummaryPanel = Summary;
	AddSettleRow(Root, Summary, 18.f);

	AddSettleRow(Summary, MakeSettleText(WidgetTree, NAME_None, TEXT("최종 골드"), 19, FLinearColor::White), 0.f);
	SummaryText = MakeSettleText(WidgetTree, TEXT("SummaryText"), TEXT(""), 17, FLinearColor(0.92f, 0.92f, 0.92f));
	AddSettleRow(Summary, SummaryText, 10.f);
	CloseButton = MakeSettleButton(WidgetTree, TEXT("CloseButton"), TEXT("  확인  "), SettleConfirmColor);
	AddSettleRow(Summary, CloseButton, 18.f, HAlign_Center);
}

bool USettlementWidget::IsAlreadyStored(FName Row) const
{
	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	return Profile && Profile->GetStoredRelics().Contains(Row);
}

void USettlementWidget::RebuildRelics()
{
	if (!RelicList) return;

	RelicList->ClearChildren();

	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	for (int32 i = 0; i < Settlement.Relics.Num(); ++i)
	{
		if (UItemSlotWidget* RelicSlot = CreateWidget<UItemSlotWidget>(this, Class))
		{
			RelicSlot->SetSlotIndex(i);
			RelicSlot->SetSlotSize(RelicSlotSize);
			RelicSlot->SetShowName(true);
			RelicSlot->SetRelic(Settlement.Relics[i]);
			RelicSlot->SetSelected(!Settlement.bDeath && Settlement.Relics[i] == Chosen);
			if (!Settlement.bDeath)
			{
				RelicSlot->OnSlotClicked.BindUObject(this, &USettlementWidget::HandleRelicClicked);
			}
			RelicList->AddChild(RelicSlot);
		}
	}
}

int32 USettlementWidget::DeathGold() const
{
	int64 Total = 0;
	for (const int32 Gold : Settlement.RelicGold) Total += FMath::Max(0, Gold);
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

void USettlementWidget::RefreshGuide()
{
	const bool bHasRelics = Settlement.Relics.Num() > 0;

	if (Settlement.bDeath)
	{
		if (RelicGuideText)
		{
			RelicGuideText->SetText(FText::FromString(bHasRelics
				? FString::Printf(TEXT("유물 판매  +%s"), *SettleGoldText(DeathGold()))
				: FString(TEXT("던전에서 얻은 유물이 없습니다."))));
		}
		if (ConfirmButton) ConfirmButton->SetIsEnabled(true);
		return;
	}

	if (RelicGuideText)
	{
		FString Guide;
		if (!bHasRelics)
		{
			Guide = TEXT("던전에서 얻은 유물이 없습니다.");
		}
		else if (Chosen.IsNone())
		{
			Guide = TEXT("보관할 유물을 하나 고르세요.");
		}
		else
		{
			Guide = FString::Printf(TEXT("고른 유물: %s"), *SettleRelicName(Chosen));
			if (IsAlreadyStored(Chosen)) Guide += TEXT("  (창고에 이미 있어 아무 일도 일어나지 않습니다)");
		}
		RelicGuideText->SetText(FText::FromString(Guide));
	}

	// 고를 유물이 있으면 하나는 골라야 완료
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(!bHasRelics || !Chosen.IsNone());
	}
}

void USettlementWidget::HandleRelicClicked(UItemSlotWidget* ClickedSlot)
{
	if (!ClickedSlot || !Settlement.Relics.IsValidIndex(ClickedSlot->GetSlotIndex())) return;

	Chosen = Settlement.Relics[ClickedSlot->GetSlotIndex()];
	RebuildRelics();
	RefreshGuide();
}

void USettlementWidget::HandleConfirm()
{
	UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	if (!Profile) return;
	if (!Settlement.bDeath && Settlement.Relics.Num() > 0 && Chosen.IsNone()) return;
	if (Settlement.bDeath) Chosen = NAME_None;   // 사망: 보관 없음

	const bool bNewRelic = !Chosen.IsNone() && !IsAlreadyStored(Chosen);
	const bool bNewSkill = !Settlement.KeptSkill.IsNone() && !Profile->GetOwnedSkills().Contains(Settlement.KeptSkill);
	const int32 QuestGold = 0;   // TODO: 퀘스트가 생기면 완료한 퀘스트 보상 (사망이면 없음)
	const int32 RelicGold = Settlement.bDeath ? DeathGold() : 0;

	Profile->FinishSettlement(Chosen, QuestGold + RelicGold);

	if (SummaryText)
	{
		FString Summary = Settlement.bDeath
			? FString::Printf(TEXT("유물 판매  +%s\n\n보유 골드  %s"), *SettleGoldText(RelicGold), *SettleGoldText(Profile->GetGold()))
			: FString::Printf(TEXT("퀘스트 골드  +%s\n\n보유 골드  %s"), *SettleGoldText(QuestGold), *SettleGoldText(Profile->GetGold()));
		if (bNewRelic) Summary += FString::Printf(TEXT("\n\n창고에 보관: %s"), *SettleRelicName(Chosen));
		if (bNewSkill)
		{
			const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Settlement.KeptSkill);
			Summary += FString::Printf(TEXT("\n새 보유 스킬: %s"), Skill ? *Skill->DisplayName_KR.ToString() : *Settlement.KeptSkill.ToString());
		}
		SummaryText->SetText(FText::FromString(Summary));
	}

	if (MainPanel) MainPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (SummaryPanel) SummaryPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USettlementWidget::HandleClose()
{
	RemoveFromParent();
}
