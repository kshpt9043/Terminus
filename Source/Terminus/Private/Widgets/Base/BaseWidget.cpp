#include "Widgets/Base/BaseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/SkillTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/EscapeStackSubsystem.h"

namespace
{
	UTextBlock* MakeBaseText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color, const FName& Name = NAME_None)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	// 고정 폭 칸에 글자 하나 (연무장 줄)
	USizeBox* MakeBaseCell(UWidgetTree* Tree, const FString& Label, float Width, int32 Size, const FLinearColor& Color)
	{
		USizeBox* Cell = Tree->ConstructWidget<USizeBox>();
		Cell->SetWidthOverride(Width);
		Cell->SetContent(MakeBaseText(Tree, Label, Size, Color));
		return Cell;
	}

	void AddBaseCell(UHorizontalBox* Line, USizeBox* Cell)
	{
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Cell))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
	}

	const FLinearColor BaseGold(1.f, 0.85f, 0.35f);
	const FLinearColor BaseDim(0.65f, 0.65f, 0.65f);
}

void UBaseClickRelay::HandleClicked()
{
	if (UBaseWidget* Widget = Owner.Get())
	{
		Widget->HandleRelayClicked(View, Index);
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UBaseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !ContentBox)
	{
		BuildDefaultLayout();
	}

	if (BackButton)     BackButton->OnClicked.AddDynamic(this, &UBaseWidget::HandleBackClicked);
	if (MainMenuButton) MainMenuButton->OnClicked.AddDynamic(this, &UBaseWidget::HandleMainMenuClicked);

	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->OnGoldChanged.AddUniqueDynamic(this, &UBaseWidget::HandleGoldChanged);
		UpgradesChangedHandle = Profile->OnUpgradesChanged.AddUObject(this, &UBaseWidget::Rebuild);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UBaseWidget::NativeDestruct()
{
	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->OnGoldChanged.RemoveDynamic(this, &UBaseWidget::HandleGoldChanged);
		Profile->OnUpgradesChanged.Remove(UpgradesChangedHandle);
	}
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	Super::NativeDestruct();
}

void UBaseWidget::BuildDefaultLayout()
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BaseBackground"));
	Background->SetBrushColor(FLinearColor(0.07f, 0.065f, 0.06f, 0.97f));
	Background->SetPadding(FMargin(48.f, 32.f));
	WidgetTree->RootWidget = Background;

	UOverlay* Stage = WidgetTree->ConstructWidget<UOverlay>();
	Background->SetContent(Stage);

	// 우상단: 보유 골드 + 돌아가기 / 메인화면
	UVerticalBox* Corner = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* S = Stage->AddChildToOverlay(Corner))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Top);
	}

	GoldText = MakeBaseText(WidgetTree, TEXT(""), 14, BaseGold, TEXT("GoldText"));
	Corner->AddChildToVerticalBox(GoldText);

	auto AddCornerButton = [&](const FName& Name, const FString& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(FLinearColor(0.18f, 0.18f, 0.18f));
		UTextBlock* Text = MakeBaseText(WidgetTree, Label, 16, FLinearColor::White);
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(170.f);
		Size->SetHeightOverride(46.f);
		Size->SetContent(Button);
		if (UVerticalBoxSlot* S = Corner->AddChildToVerticalBox(Size)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		return Button;
	};
	BackButton     = AddCornerButton(TEXT("BackButton"), TEXT("돌아가기"));
	MainMenuButton = AddCornerButton(TEXT("MainMenuButton"), TEXT("메인화면"));

	// 가운데: 제목 / 안내 / 내용
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* S = Stage->AddChildToOverlay(Column))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	TitleText = MakeBaseText(WidgetTree, TEXT("거점"), 30, FLinearColor::White, TEXT("TitleText"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

	MessageText = MakeBaseText(WidgetTree, TEXT(""), 14, BaseDim, TEXT("MessageText"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContentBox"));
	ContentBox = Content;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Content))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
}

// =====================================================================
// 열기 / 화면 전환
// =====================================================================

void UBaseWidget::Open()
{
	SetVisibility(ESlateVisibility::Visible);
	RefreshGold();
	ShowView(EBaseView::Faction);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &UBaseWidget::HandleEscape));
	}
}

void UBaseWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}
}

void UBaseWidget::HandleEscape()
{
	HandleBackClicked();
}

void UBaseWidget::HandleBackClicked()
{
	switch (View)
	{
	case EBaseView::Faction:   Close(); break;
	case EBaseView::Class:     ShowView(EBaseView::Faction); break;
	case EBaseView::Facility:  ShowView(EBaseView::Class); break;
	default:                   ShowView(EBaseView::Facility); break;
	}
}

void UBaseWidget::HandleMainMenuClicked()
{
	Close();
}

void UBaseWidget::ShowView(EBaseView InView)
{
	View = InView;
	SetMessage(FString());

	// 시안: 세력 / 직업 화면은 돌아가기만, 그 아래부터 메인화면도
	if (MainMenuButton)
	{
		const bool bDeep = View != EBaseView::Faction && View != EBaseView::Class;
		MainMenuButton->SetVisibility(bDeep ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	Rebuild();
}

void UBaseWidget::Rebuild()
{
	if (!ContentBox) return;

	ContentBox->ClearChildren();
	Relays.Reset();

	switch (View)
	{
	case EBaseView::Faction:   BuildFactionView(); break;
	case EBaseView::Class:     BuildClassView(); break;
	case EBaseView::Facility:  BuildFacilityView(); break;
	case EBaseView::StatHall:  BuildStatHallView(); break;
	case EBaseView::SkillHall: BuildSkillHallView(); break;
	}
}

void UBaseWidget::HandleRelayClicked(EBaseView InView, int32 Index)
{
	if (InView != View) return;

	UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;

	switch (InView)
	{
	case EBaseView::Faction:
		if (Factions.IsValidIndex(Index))
		{
			SelectedFaction = Factions[Index];
			ShowView(EBaseView::Class);
		}
		break;

	case EBaseView::Class:
		if (ShownClasses.IsValidIndex(Index))
		{
			SelectedClass = ShownClasses[Index];
			ShowView(EBaseView::Facility);
		}
		break;

	case EBaseView::Facility:
		ShowView(Index == 0 ? EBaseView::StatHall : EBaseView::SkillHall);
		break;

	case EBaseView::StatHall:
		if (Profile && Index >= 0 && Index < static_cast<int32>(EStatUpgrade::MAX))
		{
			const EStatUpgrade Stat = static_cast<EStatUpgrade>(Index);
			if (Profile->TryUpgradeStat(SelectedClass, Stat))
			{
				SetMessage(FString::Printf(TEXT("%s 강화 완료"), *GetStatUpgradeName(Stat).ToString()));
			}
			else
			{
				SetMessage(TEXT("골드가 부족합니다."), true);
			}
		}
		break;

	case EBaseView::SkillHall:
		if (Profile)
		{
			if (Profile->TryUpgradeSkill(SelectedClass, Index))
			{
				SetMessage(TEXT("기본 스킬 강화 완료"));
			}
			else
			{
				SetMessage(TEXT("골드가 부족합니다."), true);
			}
		}
		break;
	}
}

// =====================================================================
// 화면별 내용
// =====================================================================

void UBaseWidget::BuildFactionView()
{
	if (TitleText) TitleText->SetText(FText::FromString(TEXT("거점")));

	for (int32 i = 0; i < Factions.Num(); ++i)
	{
		AddCard(GetFactionName(Factions[i]).ToString(), EBaseView::Faction, i);
	}
}

void UBaseWidget::BuildClassView()
{
	if (TitleText) TitleText->SetText(GetFactionName(SelectedFaction));

	ShownClasses.Reset();
	for (int32 i = 0; i < static_cast<int32>(ECharacterClass::MAX); ++i)
	{
		const ECharacterClass ThisClass = static_cast<ECharacterClass>(i);
		const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(ThisClass);
		if (Row && Row->Faction == SelectedFaction)
		{
			AddCard(Row->DisplayName.ToString(), EBaseView::Class, ShownClasses.Num());
			ShownClasses.Add(ThisClass);
		}
	}

	// 시안: 직업이 적으면 '추가 예정'
	for (int32 i = ShownClasses.Num(); i < MinClassCards; ++i)
	{
		AddCard(TEXT("추가 예정"), EBaseView::Class, INDEX_NONE, false);
	}
}

void UBaseWidget::BuildFacilityView()
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *GetFactionName(SelectedFaction).ToString(), *GetClassDisplayName(SelectedClass).ToString())));
	}

	AddCard(TEXT("연무장"), EBaseView::Facility, 0);
	AddCard(TEXT("훈련소"), EBaseView::Facility, 1);
}

void UBaseWidget::BuildStatHallView()
{
	if (TitleText) TitleText->SetText(FText::FromString(FString::Printf(TEXT("연무장 · %s"), *GetClassDisplayName(SelectedClass).ToString())));

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	const UTerminusUpgradeSettings* Settings = UTerminusUpgradeSettings::Get();
	const FClassUpgrades Upgrades = Profile ? Profile->GetClassUpgrades(SelectedClass) : FClassUpgrades();
	const int32 Gold = Profile ? Profile->GetGold() : 0;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.3f, 1.f));
	Panel->SetPadding(FMargin(28.f, 20.f));
	ContentBox->AddChild(Panel);

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Rows);

	for (int32 i = 0; i < static_cast<int32>(EStatUpgrade::MAX); ++i)
	{
		const EStatUpgrade Stat = static_cast<EStatUpgrade>(i);
		const FStatUpgradeRule* Rule = Settings->FindStatRule(Stat);
		if (!Rule) continue;

		const int32 Level = Upgrades.GetStatLevel(Stat);
		const int32 Cost = Settings->GetStatCost(Stat, Level);
		const bool bMax = Cost < 0;

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* S = Rows->AddChildToVerticalBox(Line)) S->SetPadding(FMargin(0.f, 5.f));

		AddBaseCell(Line, MakeBaseCell(WidgetTree, GetStatUpgradeName(Stat).ToString(), 140.f, 17, FLinearColor::White));
		AddBaseCell(Line, MakeBaseCell(WidgetTree, FString::Printf(TEXT("[%d/%d]"), Level, Rule->MaxLevel), 90.f, 17, FLinearColor::White));
		AddBaseCell(Line, MakeBaseCell(WidgetTree, FString::Printf(TEXT("+%d"), Level * Rule->AmountPerLevel), 70.f, 15, BaseDim));
		AddBaseCell(Line, MakeBaseCell(WidgetTree, bMax ? FString(TEXT("최대")) : FString::Printf(TEXT("%d 골드"), Cost), 140.f, 17,
			bMax ? BaseDim : (Gold >= Cost ? BaseGold : FLinearColor(1.f, 0.45f, 0.4f))));

		UButton* Plus = MakeRelayButton(TEXT(" + "), EBaseView::StatHall, i, 16);
		Plus->SetIsEnabled(!bMax && Gold >= Cost);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Plus)) S->SetVerticalAlignment(VAlign_Center);
	}
}

void UBaseWidget::BuildSkillHallView()
{
	if (TitleText) TitleText->SetText(FText::FromString(FString::Printf(TEXT("훈련소 · %s"), *GetClassDisplayName(SelectedClass).ToString())));

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	const UTerminusUpgradeSettings* Settings = UTerminusUpgradeSettings::Get();
	const FClassUpgrades Upgrades = Profile ? Profile->GetClassUpgrades(SelectedClass) : FClassUpgrades();
	const int32 Gold = Profile ? Profile->GetGold() : 0;

	// 전투 HUD 기본 칸과 같은 순서
	const TArray<const FSkillRow*> Basic = UTerminusDataSettings::FindBasicSkills(SelectedClass);

	for (int32 i = 0; i < ATerminusPlayerState::NumBasicSkills; ++i)
	{
		const FSkillRow* Skill = Basic.IsValidIndex(i) ? Basic[i] : nullptr;
		const int32 Level = Upgrades.GetSkillLevel(i);
		const int32 Cost = Settings->GetSkillCost(Level);
		const bool bMax = Cost < 0;

		UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(ContentBox->AddChild(Card))) S->SetPadding(FMargin(14.f, 0.f));

		// 카드 몸통: 이름 / 설명 / 비용 + 강화 버튼
		UBorder* Body = WidgetTree->ConstructWidget<UBorder>();
		Body->SetBrushColor(FLinearColor(0.35f, 0.35f, 0.35f, 1.f));
		Body->SetPadding(FMargin(14.f));
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(CardSize.X);
		Size->SetHeightOverride(CardSize.Y * 0.8f);
		Size->SetContent(Body);
		Card->AddChildToVerticalBox(Size);

		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
		Body->SetContent(Inside);

		const FString Name = Skill ? Skill->DisplayName_KR.ToString() : FString::Printf(TEXT("기본 스킬 %d"), i + 1);
		UTextBlock* NameText = MakeBaseText(WidgetTree, Name, 20, FLinearColor::White);
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(NameText)) S->SetHorizontalAlignment(HAlign_Center);

		const FString Effect = !Skill ? FString()
			: Skill->BaseValue != 0 ? FString::Printf(TEXT("수치 %d → %d"), Skill->BaseValue + Level, Skill->BaseValue + Level + (bMax ? 0 : 1))
			: FString::Printf(TEXT("상태 수치 %d → %d"), Skill->StatusValue + Level, Skill->StatusValue + Level + (bMax ? 0 : 1));
		UTextBlock* Desc = MakeBaseText(WidgetTree,
			(Skill ? Skill->Description_KR.ToString().Replace(TEXT("\\n"), TEXT("\n")) : FString()) + TEXT("\n\n") + (bMax ? FString::Printf(TEXT("수치 +%d (최대)"), Level) : Effect),
			13, FLinearColor(0.92f, 0.92f, 0.92f));
		Desc->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(Desc))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}

		UHorizontalBox* Buy = WidgetTree->ConstructWidget<UHorizontalBox>();
		Inside->AddChildToVerticalBox(Buy);
		UTextBlock* CostText = MakeBaseText(WidgetTree, bMax ? FString(TEXT("최대")) : FString::Printf(TEXT("%d 골드"), Cost), 16,
			bMax ? BaseDim : (Gold >= Cost ? BaseGold : FLinearColor(1.f, 0.45f, 0.4f)));
		if (UHorizontalBoxSlot* S = Buy->AddChildToHorizontalBox(CostText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UButton* Plus = MakeRelayButton(TEXT(" + "), EBaseView::SkillHall, i, 16);
		Plus->SetIsEnabled(!bMax && Gold >= Cost && Skill != nullptr);
		Buy->AddChildToHorizontalBox(Plus);

		// 카드 아래 단계 칸 (시안: 작은 네모 3개)
		UHorizontalBox* Pips = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(Pips))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		for (int32 p = 0; p < Settings->SkillMaxLevel; ++p)
		{
			UBorder* Pip = WidgetTree->ConstructWidget<UBorder>();
			Pip->SetBrushColor(p < Level ? BaseGold : FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
			USizeBox* PipSize = WidgetTree->ConstructWidget<USizeBox>();
			PipSize->SetWidthOverride(26.f);
			PipSize->SetHeightOverride(26.f);
			PipSize->SetContent(Pip);
			if (UHorizontalBoxSlot* S = Pips->AddChildToHorizontalBox(PipSize)) S->SetPadding(FMargin(4.f, 0.f));
		}
	}
}

// =====================================================================
// 공용
// =====================================================================

UButton* UBaseWidget::MakeRelayButton(const FString& Label, EBaseView InView, int32 Index, int32 FontSize)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* Text = MakeBaseText(WidgetTree, Label, FontSize, FLinearColor::Black);
	Text->SetJustification(ETextJustify::Center);
	Button->SetContent(Text);

	UBaseClickRelay* Relay = NewObject<UBaseClickRelay>(this);
	Relay->Owner = this;
	Relay->View = InView;
	Relay->Index = Index;
	Button->OnClicked.AddDynamic(Relay, &UBaseClickRelay::HandleClicked);
	Relays.Add(Relay);
	return Button;
}

UButton* UBaseWidget::AddCard(const FString& Label, EBaseView InView, int32 Index, bool bEnabled)
{
	UButton* Button = MakeRelayButton(FString(), InView, Index, 1);
	Button->SetBackgroundColor(FLinearColor(0.35f, 0.35f, 0.35f));
	Button->SetIsEnabled(bEnabled);

	// 시안: 회색 상자, 아래쪽에 이름
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(CardSize.X);
	Size->SetHeightOverride(CardSize.Y);
	UOverlay* Inner = WidgetTree->ConstructWidget<UOverlay>();
	Size->SetContent(Inner);

	UTextBlock* Text = MakeBaseText(WidgetTree, Label, 24, bEnabled ? FLinearColor::White : BaseDim);
	Text->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* S = Inner->AddChildToOverlay(Text))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	}
	Button->SetContent(Size);

	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(ContentBox->AddChild(Button)))
	{
		S->SetPadding(FMargin(10.f, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	return Button;
}

void UBaseWidget::SetMessage(const FString& Text, bool bError)
{
	if (!MessageText) return;

	MessageText->SetText(FText::FromString(Text));
	MessageText->SetColorAndOpacity(FSlateColor(bError ? FLinearColor(1.f, 0.45f, 0.4f) : BaseDim));
}

void UBaseWidget::HandleGoldChanged(int32 NewGold, int32 Delta)
{
	RefreshGold();
}

void UBaseWidget::RefreshGold()
{
	if (!GoldText) return;

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	GoldText->SetText(FText::Format(FText::FromString(TEXT("보유 골드: {0}")), FText::AsNumber(Profile ? Profile->GetGold() : 0)));
}

FText UBaseWidget::GetClassDisplayName(ECharacterClass InClass) const
{
	const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(InClass);
	return Row ? Row->DisplayName : FText::GetEmpty();
}
