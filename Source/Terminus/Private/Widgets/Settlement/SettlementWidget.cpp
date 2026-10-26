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
	Kept.Reset();

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

	// 유물: 원하는 만큼 보관, 나머지는 판매 (사망도 같음)
	if (TitleText && Settlement.bDeath) TitleText->SetText(FText::FromString(TEXT("사망 정산")));
	if (RelicHeaderText) RelicHeaderText->SetText(FText::FromString(TEXT("1. 유물 보관 / 판매")));
	{
		FString Help = TEXT("보관할 유물을 눌러 고르세요 (여러 개 가능). 고르지 않은 유물은 자동으로 판매되어 골드가 됩니다.");
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
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("SettlementDim"), TerminusUI::Color::Dim);

	UBorder* Panel = TerminusUI::MakePanel(WidgetTree, NAME_None, TerminusUI::Color::Board, FMargin(30.f, 24.f));
	Dim->SetContent(Panel);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(760.f);
	Panel->SetContent(Width);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	Width->SetContent(Root);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT("정산"), TerminusUI::FontSize::ScreenTitle, TerminusUI::Color::Gold);
	TerminusUI::SetBold(TitleText);
	TerminusUI::SetLetterSpacing(TitleText, 80);
	TerminusUI::AddRow(Root, TitleText, 0.f, HAlign_Fill);
	SubtitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SubtitleText"), TEXT(""), TerminusUI::FontSize::Subtitle, TerminusUI::Color::TextSub);
	TerminusUI::AddRow(Root, SubtitleText, 6.f, HAlign_Fill);

	// ---- 정산 목록
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	MainPanel = Main;
	TerminusUI::AddRow(Root, Main, 18.f, HAlign_Fill);

	RelicHeaderText = TerminusUI::MakeWrapText(WidgetTree, TEXT("RelicHeaderText"), TEXT("1. 유물 보관 / 판매"), TerminusUI::FontSize::Section, TerminusUI::Color::Gold, ETextJustify::Left);
	TerminusUI::SetBold(RelicHeaderText);
	TerminusUI::AddRow(Main, RelicHeaderText, 0.f, HAlign_Fill);
	RelicHelpText = TerminusUI::MakeWrapText(WidgetTree, TEXT("RelicHelpText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextAux, ETextJustify::Left);
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

	RelicGuideText = TerminusUI::MakeWrapText(WidgetTree, TEXT("RelicGuideText"), TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::GoldHover, ETextJustify::Left);
	TerminusUI::AddRow(Main, RelicGuideText, 8.f, HAlign_Fill);

	UTextBlock* QuestHeader = TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("2. 완료된 퀘스트"), TerminusUI::FontSize::Section, TerminusUI::Color::Gold, ETextJustify::Left);
	TerminusUI::SetBold(QuestHeader);
	TerminusUI::AddRow(Main, QuestHeader, 18.f, HAlign_Fill);
	QuestText = TerminusUI::MakeWrapText(WidgetTree, TEXT("QuestText"), TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::TextBody, ETextJustify::Left);
	TerminusUI::AddRow(Main, QuestText, 4.f, HAlign_Fill);

	UTextBlock* SkillHeader = TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("3. 가져가는 스킬"), TerminusUI::FontSize::Section, TerminusUI::Color::Gold, ETextJustify::Left);
	TerminusUI::SetBold(SkillHeader);
	TerminusUI::AddRow(Main, SkillHeader, 18.f, HAlign_Fill);
	SkillText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SkillText"), TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::TextBody, ETextJustify::Left);
	TerminusUI::AddRow(Main, SkillText, 4.f, HAlign_Fill);

	ConfirmButton = TerminusUI::MakeHexButton(WidgetTree, TEXT("ConfirmButton"), TEXT("정산 완료"), TerminusUI::EHexButton::Primary);
	TerminusUI::AddRow(Main, ConfirmButton, 22.f, HAlign_Right);

	// ---- 최종 골드
	UVerticalBox* Summary = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryPanel"));
	SummaryPanel = Summary;
	TerminusUI::AddRow(Root, Summary, 18.f, HAlign_Fill);

	UTextBlock* SummaryHeader = TerminusUI::MakeWrapText(WidgetTree, NAME_None, TEXT("최종 골드"), TerminusUI::FontSize::Section, TerminusUI::Color::Gold);
	TerminusUI::SetBold(SummaryHeader);
	TerminusUI::AddRow(Summary, SummaryHeader, 0.f, HAlign_Fill);
	SummaryText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SummaryText"), TEXT(""), TerminusUI::FontSize::Subtitle, TerminusUI::Color::TextBody);
	TerminusUI::AddRow(Summary, SummaryText, 10.f, HAlign_Fill);
	CloseButton = TerminusUI::MakeHexButton(WidgetTree, TEXT("CloseButton"), TEXT("확인"), TerminusUI::EHexButton::Primary);
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
			RelicSlot->SetSelected(Kept.Contains(Settlement.Relics[i]));
			RelicSlot->OnSlotClicked.BindUObject(this, &USettlementWidget::HandleRelicClicked);
			RelicList->AddChild(RelicSlot);
		}
	}
}

int32 USettlementWidget::SaleGold() const
{
	int64 Total = 0;
	for (int32 i = 0; i < Settlement.Relics.Num(); ++i)
	{
		if (Kept.Contains(Settlement.Relics[i]) || !Settlement.RelicGold.IsValidIndex(i)) continue;
		Total += FMath::Max(0, Settlement.RelicGold[i]);
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

void USettlementWidget::RefreshGuide()
{
	if (RelicGuideText)
	{
		FString Guide;
		if (Settlement.Relics.Num() == 0)
		{
			Guide = TEXT("던전에서 얻은 유물이 없습니다.");
		}
		else
		{
			Guide = FString::Printf(TEXT("보관 %d개 · 판매 %d개  +%s"), Kept.Num(), Settlement.Relics.Num() - Kept.Num(), *TerminusUI::GoldText(SaleGold()));

			// 창고에 이미 있는 걸 보관하면 아무 일도 없음 -> 파는 게 이득
			TArray<FString> Dup;
			for (const FName& Row : Kept)
			{
				if (IsAlreadyStored(Row)) Dup.Add(UTerminusDataSettings::GetRelicName(Row));
			}
			if (Dup.Num() > 0)
			{
				Guide += FString::Printf(TEXT("\n[%s] 은(는) 창고에 이미 있어 보관해도 아무 일도 없습니다. 판매하면 골드를 받습니다."), *FString::Join(Dup, TEXT(", ")));
			}
		}
		RelicGuideText->SetText(FText::FromString(Guide));
	}

	// 하나도 안 골라도 됨 (전부 판매)
	if (ConfirmButton) ConfirmButton->SetIsEnabled(true);
}

void USettlementWidget::HandleRelicClicked(UItemSlotWidget* ClickedSlot)
{
	if (!ClickedSlot || !Settlement.Relics.IsValidIndex(ClickedSlot->GetSlotIndex())) return;

	// 누를 때마다 보관 <-> 판매
	const FName Row = Settlement.Relics[ClickedSlot->GetSlotIndex()];
	if (Kept.Remove(Row) == 0) Kept.Add(Row);
	RebuildRelics();
	RefreshGuide();
}

void USettlementWidget::HandleConfirm()
{
	UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	if (!Profile) return;

	TArray<FName> NewRelics;
	for (const FName& Row : Kept)
	{
		if (!IsAlreadyStored(Row)) NewRelics.Add(Row);
	}
	const bool bNewSkill = !Settlement.KeptSkill.IsNone() && !Profile->GetOwnedSkills().Contains(Settlement.KeptSkill);
	const int32 RelicGold = SaleGold();
	const int32 BonusGold = FMath::Max(0, Settlement.BonusGold);   // 퀘스트 골드 / 배신 분배

	Profile->FinishSettlement(Kept, RelicGold + BonusGold);

	if (SummaryText)
	{
		FString Summary = FString::Printf(TEXT("유물 판매  +%s"), *TerminusUI::GoldText(RelicGold));
		if (BonusGold > 0) Summary = FString::Printf(TEXT("%s  +%s\n"), *Settlement.BonusReason, *TerminusUI::GoldText(BonusGold)) + Summary;
		Summary += FString::Printf(TEXT("\n\n보유 골드  %s"), *TerminusUI::GoldText(Profile->GetGold()));

		TArray<FString> Names;
		for (const FName& Row : NewRelics) Names.Add(UTerminusDataSettings::GetRelicName(Row));
		if (Names.Num() > 0) Summary += FString::Printf(TEXT("\n\n창고에 보관: %s"), *FString::Join(Names, TEXT(", ")));
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
