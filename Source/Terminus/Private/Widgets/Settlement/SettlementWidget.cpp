#include "Widgets/Settlement/SettlementWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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

	FString SettleGold(int32 Gold)
	{
		return FText::AsNumber(Gold).ToString() + TEXT(" G");
	}

	const FLinearColor SettleGold_Color(1.f, 0.82f, 0.35f);
	const FLinearColor SettleChosen(0.25f, 0.55f, 0.3f);
	const FLinearColor SettleIdle(0.22f, 0.22f, 0.26f);
}

void USettlementClickRelay::HandleClicked()
{
	if (USettlementWidget* Widget = Owner.Get())
	{
		Widget->HandleRelicChoice(Index, Choice);
	}
}

void USettlementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !RelicList)
	{
		BuildDefaultLayout();
	}

	if (SellAllButton) SellAllButton->OnClicked.AddDynamic(this, &USettlementWidget::HandleSellAll);
	if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &USettlementWidget::HandleConfirm);
	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &USettlementWidget::HandleClose);

	SetVisibility(ESlateVisibility::Visible);   // 아래 메인 메뉴를 못 누르게

	// 정산 내용
	if (const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Settlement = Profile->GetPendingSettlement();
	}

	// 기본 선택: 창고에 없으면 보관, 이미 있으면 비싼 쪽에 판매 (보관해도 아무 일도 없어서)
	Choices.Reset();
	for (const FSettlementRelic& Relic : Settlement.Relics)
	{
		ESettlementChoice Choice = ESettlementChoice::Keep;
		if (IsAlreadyStored(Relic.Row) && FMath::Max(Relic.MagePrice, Relic.ReligionPrice) >= 0)
		{
			Choice = Relic.MagePrice >= Relic.ReligionPrice ? ESettlementChoice::Mage : ESettlementChoice::Religion;
		}
		Choices.Add(Choice);
	}

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
		SkillText->SetText(FText::FromString(Skill
			? FString::Printf(TEXT("[%s] 을(를) 보유 스킬로 가져갑니다."), *Skill->DisplayName_KR.ToString())
			: FString(TEXT("새로 가져갈 스킬이 없습니다. (강화 칸 스킬을 이미 모두 보유)"))));
	}

	if (SummaryPanel) SummaryPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (MainPanel) MainPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	RebuildRelics();
	RefreshTotal();
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
	Width->SetWidthOverride(820.f);
	Panel->SetContent(Width);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	Width->SetContent(Root);

	TitleText = MakeSettleText(WidgetTree, TEXT("TitleText"), TEXT("정산"), 30, SettleGold_Color);
	AddSettleRow(Root, TitleText, 0.f);
	SubtitleText = MakeSettleText(WidgetTree, TEXT("SubtitleText"), TEXT(""), 15, FLinearColor(0.8f, 0.8f, 0.8f));
	AddSettleRow(Root, SubtitleText, 6.f);

	// ---- 정산 목록
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	MainPanel = Main;
	AddSettleRow(Root, Main, 18.f);

	AddSettleRow(Main, MakeSettleText(WidgetTree, NAME_None, TEXT("1. 획득한 유물"), 19, FLinearColor::White, ETextJustify::Left), 0.f);
	AddSettleRow(Main, MakeSettleText(WidgetTree, NAME_None,
		TEXT("마탑이나 테르미누스에 팔거나 창고에 보관합니다. 보관하면 골드는 없고, 창고에 이미 있는 유물은 보관해도 아무 일도 일어나지 않습니다."),
		13, FLinearColor(0.7f, 0.7f, 0.7f), ETextJustify::Left), 4.f);

	USizeBox* ListHeight = WidgetTree->ConstructWidget<USizeBox>();
	ListHeight->SetMaxDesiredHeight(380.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RelicList"));
	RelicList = Scroll;
	ListHeight->SetContent(Scroll);
	AddSettleRow(Main, ListHeight, 10.f);

	AddSettleRow(Main, MakeSettleText(WidgetTree, NAME_None, TEXT("2. 완료된 퀘스트"), 19, FLinearColor::White, ETextJustify::Left), 18.f);
	QuestText = MakeSettleText(WidgetTree, TEXT("QuestText"), TEXT(""), 14, FLinearColor(0.85f, 0.85f, 0.85f), ETextJustify::Left);
	AddSettleRow(Main, QuestText, 4.f);

	AddSettleRow(Main, MakeSettleText(WidgetTree, NAME_None, TEXT("3. 가져가는 스킬"), 19, FLinearColor::White, ETextJustify::Left), 18.f);
	SkillText = MakeSettleText(WidgetTree, TEXT("SkillText"), TEXT(""), 14, FLinearColor(0.85f, 0.85f, 0.85f), ETextJustify::Left);
	AddSettleRow(Main, SkillText, 4.f);

	TotalText = MakeSettleText(WidgetTree, TEXT("TotalText"), TEXT(""), 20, SettleGold_Color, ETextJustify::Right);
	AddSettleRow(Main, TotalText, 20.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddSettleRow(Main, Buttons, 12.f, HAlign_Right);
	SellAllButton = MakeSettleButton(WidgetTree, TEXT("SellAllButton"), TEXT("  전부 비싼 곳에 판매  "), SettleIdle);
	if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(SellAllButton)) S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	ConfirmButton = MakeSettleButton(WidgetTree, TEXT("ConfirmButton"), TEXT("  정산 완료  "), SettleChosen);
	Buttons->AddChildToHorizontalBox(ConfirmButton);

	// ---- 최종 골드
	UVerticalBox* Summary = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryPanel"));
	SummaryPanel = Summary;
	AddSettleRow(Root, Summary, 18.f);

	AddSettleRow(Summary, MakeSettleText(WidgetTree, NAME_None, TEXT("최종 골드"), 19, FLinearColor::White), 0.f);
	SummaryText = MakeSettleText(WidgetTree, TEXT("SummaryText"), TEXT(""), 17, FLinearColor(0.92f, 0.92f, 0.92f));
	AddSettleRow(Summary, SummaryText, 10.f);
	CloseButton = MakeSettleButton(WidgetTree, TEXT("CloseButton"), TEXT("  확인  "), SettleChosen);
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
	Relays.Reset();

	if (Settlement.Relics.Num() == 0)
	{
		RelicList->AddChild(MakeSettleText(WidgetTree, NAME_None, TEXT("정산할 유물이 없습니다."), 14, FLinearColor(0.75f, 0.75f, 0.75f), ETextJustify::Left));
		return;
	}

	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());

	for (int32 i = 0; i < Settlement.Relics.Num(); ++i)
	{
		const FSettlementRelic& Relic = Settlement.Relics[i];
		const bool bStored = IsAlreadyStored(Relic.Row);

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();

		if (UItemSlotWidget* Icon = CreateWidget<UItemSlotWidget>(this, Class))
		{
			Icon->SetSlotSize(FVector2D(56.f, 56.f));
			Icon->SetRelic(Relic.Row);
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Icon)) S->SetVerticalAlignment(VAlign_Center);
		}

		const FRelicRow* Row = UTerminusDataSettings::FindRelicRow(Relic.Row);
		FString Name = Row ? Row->RelicName.ToString() : Relic.Row.ToString();
		if (bStored) Name += TEXT("\n(창고에 있음)");
		UTextBlock* NameText = MakeSettleText(WidgetTree, NAME_None, Name, 15, FLinearColor::White, ETextJustify::Left);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(NameText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(12.f, 0.f));
		}

		auto AddChoice = [&](ESettlementChoice Choice, const FString& Label, bool bEnabled)
		{
			UButton* Button = MakeSettleButton(WidgetTree, NAME_None, Label, Choices[i] == Choice ? SettleChosen : SettleIdle);
			Button->SetIsEnabled(bEnabled);

			USettlementClickRelay* Relay = NewObject<USettlementClickRelay>(this);
			Relay->Owner = this;
			Relay->Index = i;
			Relay->Choice = Choice;
			Button->OnClicked.AddDynamic(Relay, &USettlementClickRelay::HandleClicked);
			Relays.Add(Relay);

			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
			Size->SetWidthOverride(150.f);
			Size->SetHeightOverride(52.f);
			Size->SetContent(Button);
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Size))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(4.f, 0.f));
			}
		};

		AddChoice(ESettlementChoice::Mage, Relic.MagePrice >= 0 ? FString::Printf(TEXT("마탑\n%s"), *SettleGold(Relic.MagePrice)) : FString(TEXT("마탑\n판매 불가")), Relic.MagePrice >= 0);
		AddChoice(ESettlementChoice::Religion, Relic.ReligionPrice >= 0 ? FString::Printf(TEXT("테르미누스\n%s"), *SettleGold(Relic.ReligionPrice)) : FString(TEXT("테르미누스\n판매 불가")), Relic.ReligionPrice >= 0);
		AddChoice(ESettlementChoice::Keep, bStored ? FString(TEXT("보관\n(아무 일 없음)")) : FString(TEXT("보관\n창고에 넣기")), true);

		if (UPanelSlot* S = RelicList->AddChild(Line))
		{
			if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(S)) ScrollSlot->SetPadding(FMargin(0.f, 4.f));
			else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(S)) VSlot->SetPadding(FMargin(0.f, 4.f));
		}
	}
}

int32 USettlementWidget::CalcGold() const
{
	int64 Total = 0;
	for (int32 i = 0; i < Settlement.Relics.Num() && i < Choices.Num(); ++i)
	{
		const FSettlementRelic& Relic = Settlement.Relics[i];
		if (Choices[i] == ESettlementChoice::Mage && Relic.MagePrice > 0) Total += Relic.MagePrice;
		if (Choices[i] == ESettlementChoice::Religion && Relic.ReligionPrice > 0) Total += Relic.ReligionPrice;
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

void USettlementWidget::RefreshTotal()
{
	if (TotalText)
	{
		TotalText->SetText(FText::FromString(FString::Printf(TEXT("판매 골드 +%s"), *SettleGold(CalcGold()))));
	}
}

void USettlementWidget::HandleRelicChoice(int32 Index, ESettlementChoice Choice)
{
	if (!Choices.IsValidIndex(Index) || !Settlement.Relics.IsValidIndex(Index)) return;

	const FSettlementRelic& Relic = Settlement.Relics[Index];
	if (Choice == ESettlementChoice::Mage && Relic.MagePrice < 0) return;
	if (Choice == ESettlementChoice::Religion && Relic.ReligionPrice < 0) return;

	Choices[Index] = Choice;
	RebuildRelics();
	RefreshTotal();
}

void USettlementWidget::HandleSellAll()
{
	for (int32 i = 0; i < Settlement.Relics.Num() && i < Choices.Num(); ++i)
	{
		const FSettlementRelic& Relic = Settlement.Relics[i];
		if (FMath::Max(Relic.MagePrice, Relic.ReligionPrice) < 0) continue;   // 못 파는 건 그대로
		Choices[i] = Relic.MagePrice >= Relic.ReligionPrice ? ESettlementChoice::Mage : ESettlementChoice::Religion;
	}
	RebuildRelics();
	RefreshTotal();
}

void USettlementWidget::HandleConfirm()
{
	UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	if (!Profile) return;

	TArray<FName> Kept;
	int32 NewlyStored = 0;
	for (int32 i = 0; i < Settlement.Relics.Num() && i < Choices.Num(); ++i)
	{
		if (Choices[i] != ESettlementChoice::Keep) continue;
		Kept.Add(Settlement.Relics[i].Row);
		if (!IsAlreadyStored(Settlement.Relics[i].Row)) ++NewlyStored;
	}

	const int32 Earned = CalcGold();
	const bool bSkill = !Settlement.KeptSkill.IsNone() && !Profile->GetOwnedSkills().Contains(Settlement.KeptSkill);
	Profile->FinishSettlement(Kept, Earned);

	if (SummaryText)
	{
		FString Summary = FString::Printf(TEXT("판매 골드  +%s\n퀘스트 골드  +0 G\n\n보유 골드  %s"), *SettleGold(Earned), *SettleGold(Profile->GetGold()));
		if (NewlyStored > 0) Summary += FString::Printf(TEXT("\n\n창고에 새 유물 %d개 보관"), NewlyStored);
		if (bSkill)
		{
			const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Settlement.KeptSkill);
			Summary += FString::Printf(TEXT("\n새 보유 스킬 [%s]"), Skill ? *Skill->DisplayName_KR.ToString() : *Settlement.KeptSkill.ToString());
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
