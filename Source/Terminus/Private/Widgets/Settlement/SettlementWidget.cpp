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
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/Common/TerminusUIText.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/TerminusUISettings.h"

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
		if (Settlement.BonusGold > 0)
		{
			Sub += FString::Printf(TEXT("\n%s  +%s"), *Settlement.BonusReason, *TerminusUI::GoldText(Settlement.BonusGold));
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
		FString Help = TEXT("전멸해서 던전에서 얻은 유물은 모두 골드로 판매됩니다.");
		if (Settlement.LostStoredRelics.Num() > 0)
		{
			TArray<FString> Names;
			for (const FName& Row : Settlement.LostStoredRelics) Names.Add(UTerminusDataSettings::GetRelicName(Row));
			Help += FString::Printf(TEXT("\n하드 모드: 들고 간 [%s] 이(가) 창고에서 사라집니다."), *FString::Join(Names, TEXT(", ")));
		}
		if (RelicHelpText) RelicHelpText->SetText(FText::FromString(Help));
	}

	if (SummaryPanel) SummaryPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (MainPanel) MainPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	RebuildRelics();
	RefreshGuide();
}

void USettlementWidget::BuildDefaultLayout()
{
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("SettlementDim"), TerminusUI::Color::DimStrong);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(TerminusUI::Color::Panel);
	Panel->SetPadding(FMargin(30.f, 24.f));
	Dim->SetContent(Panel);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(760.f);
	Panel->SetContent(Width);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	Width->SetContent(Root);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT("정산"), 30, TerminusUI::Color::Gold);
	TerminusUI::AddRow(Root, TitleText, 0.f, HAlign_Fill);
	SubtitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SubtitleText"), TEXT(""), 15, TerminusUI::Color::TextMuted);
	TerminusUI::AddRow(Root, SubtitleText, 6.f, HAlign_Fill);

	// ---- 정산 목록
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	MainPanel = Main;
	TerminusUI::AddRow(Root, Main, 18.f, HAlign_Fill);

	RelicHeaderText = TerminusUI::MakeWrapText(WidgetTree, TEXT("RelicHeaderText"), TEXT("1. 보관할 유물"), 19, FLinearColor::White, ETextJustify::Left);
	TerminusUI::AddRow(Main, RelicHeaderText, 0.f, HAlign_Fill);
	RelicHelpText = TerminusUI::MakeWrapText(WidgetTree, TEXT("RelicHelpText"), TEXT("던전에서 얻은 유물 중 하나를 골라 창고에 보관합니다. 나머지 유물은 사라집니다."), 13, TerminusUI::Color::TextDim, ETextJustify::Left);
	TerminusUI::AddRow(Main, RelicHelpText, 4.f, HAlign_Fill);

	USizeBox* ListHeight = WidgetTree->ConstructWidget<USizeBox>();
	ListHeight->SetMaxDesiredHeight(300.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	ListHeight->SetContent(Scroll);
	UWrapBox* Wrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("RelicList"));
	Wrap->SetInnerSlotPadding(FVector2D(8.f, 8.f));
	RelicList = Wrap;
	Scroll->AddChild(Wrap);
	TerminusUI::AddRow(Main, ListHeight, 10.f, HAlign_Fill);

	RelicGuideText = TerminusUI::MakeWrapText(WidgetTree, TEXT("RelicGuideText"), TEXT(""), 15, TerminusUI::Color::Gold, ETextJustify::Left);
	TerminusUI::AddRow(Main, RelicGuideText, 8.f, HAlign_Fill);

	TerminusUI::AddRow(Main, TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("2. 완료된 퀘스트"), 19, FLinearColor::White, ETextJustify::Left), 18.f, HAlign_Fill);
	QuestText = TerminusUI::MakeWrapText(WidgetTree, TEXT("QuestText"), TEXT(""), 14, TerminusUI::Color::TextSoft, ETextJustify::Left);
	TerminusUI::AddRow(Main, QuestText, 4.f, HAlign_Fill);

	TerminusUI::AddRow(Main, TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("3. 가져가는 스킬"), 19, FLinearColor::White, ETextJustify::Left), 18.f, HAlign_Fill);
	SkillText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SkillText"), TEXT(""), 14, TerminusUI::Color::TextSoft, ETextJustify::Left);
	TerminusUI::AddRow(Main, SkillText, 4.f, HAlign_Fill);

	ConfirmButton = TerminusUI::MakeButton(WidgetTree, TEXT("ConfirmButton"), TEXT("  정산 완료  "), 15, FLinearColor::White, TerminusUI::Color::ButtonConfirm);
	TerminusUI::AddRow(Main, ConfirmButton, 22.f, HAlign_Right);

	// ---- 최종 골드
	UVerticalBox* Summary = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryPanel"));
	SummaryPanel = Summary;
	TerminusUI::AddRow(Root, Summary, 18.f, HAlign_Fill);

	TerminusUI::AddRow(Summary, TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("최종 골드"), 19, FLinearColor::White), 0.f, HAlign_Fill);
	SummaryText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SummaryText"), TEXT(""), 17, FLinearColor(0.92f, 0.92f, 0.92f));
	TerminusUI::AddRow(Summary, SummaryText, 10.f, HAlign_Fill);
	CloseButton = TerminusUI::MakeButton(WidgetTree, TEXT("CloseButton"), TEXT("  확인  "), 15, FLinearColor::White, TerminusUI::Color::ButtonConfirm);
	TerminusUI::AddRow(Summary, CloseButton, 18.f, HAlign_Center);
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

	const TSubclassOf<UItemSlotWidget> Class = UTerminusUISettings::Pick(SlotClass, &UTerminusUISettings::ItemSlot);
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
				? FString::Printf(TEXT("유물 판매  +%s"), *TerminusUI::GoldText(DeathGold()))
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
			Guide = FString::Printf(TEXT("고른 유물: %s"), *UTerminusDataSettings::GetRelicName(Chosen));
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

	const int32 BonusGold = FMath::Max(0, Settlement.BonusGold);

	Profile->FinishSettlement(Chosen, QuestGold + RelicGold + BonusGold);

	if (SummaryText)
	{
		FString Summary = Settlement.bDeath
			? FString::Printf(TEXT("유물 판매  +%s\n\n보유 골드  %s"), *TerminusUI::GoldText(RelicGold), *TerminusUI::GoldText(Profile->GetGold()))
			: FString::Printf(TEXT("퀘스트 골드  +%s\n\n보유 골드  %s"), *TerminusUI::GoldText(QuestGold), *TerminusUI::GoldText(Profile->GetGold()));
		if (BonusGold > 0) Summary = FString::Printf(TEXT("%s  +%s\n"), *Settlement.BonusReason, *TerminusUI::GoldText(BonusGold)) + Summary;
		if (bNewRelic) Summary += FString::Printf(TEXT("\n\n창고에 보관: %s"), *UTerminusDataSettings::GetRelicName(Chosen));
		if (bNewSkill) Summary += FString::Printf(TEXT("\n새 보유 스킬: %s"), *UTerminusDataSettings::GetSkillName(Settlement.KeptSkill));
		SummaryText->SetText(FText::FromString(Summary));
	}

	if (MainPanel) MainPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (SummaryPanel) SummaryPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USettlementWidget::HandleClose()
{
	RemoveFromParent();
}
