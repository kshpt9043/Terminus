#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/CharacterTypes.h"
#include "Data/UpgradeTypes.h"
#include "BaseWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UBaseWidget;

UENUM()
enum class EBaseView : uint8
{
	Faction,    // 세력 고르기 (모험가 길드 / 마탑 / 테르미누스)
	Class,      // 그 세력의 직업 고르기
	Facility,   // 연무장 / 훈련소 고르기
	StatHall,   // 연무장: 스테이터스 강화
	SkillHall   // 훈련소: 기본 스킬 강화
};

// 버튼마다 "몇 번째 것" 을 넘기려고 쓰는 작은 중계 (UButton 의 OnClicked 는 인자가 없음)
UCLASS()
class TERMINUS_API UBaseClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UBaseWidget> Owner;
	EBaseView View = EBaseView::Faction;
	int32 Index = 0;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 거점 (메인 메뉴). 기획 UI 레퍼런스 > 직업의 탑
 *  세력 -> 직업 -> 연무장(스테이터스) / 훈련소(기본 스킬). 골드로 강화하고 프로필에 바로 저장
 *  - 강화는 직업마다 따로. 다음 런을 시작할 때 그 직업의 강화가 런 스텟에 들어감
 *  - 비용 / 한도 / 오르는 양은 Project Settings > Game > Terminus Upgrades
 *  - 우상단 보유 골드, 돌아가기(한 단계 뒤) / 메인화면(닫기). ESC = 돌아가기
 *
 * 화면 내용은 C++ 이 ContentBox 안에 매번 새로 채움
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Base) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항, 루트는 화면 전체)
 */
UCLASS()
class TERMINUS_API UBaseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open();
	void Close();

	// 버튼 중계가 부름
	void HandleRelayClicked(EBaseView InView, int32 Index);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// 화면 제목 ("거점" / "모험가 길드" / "연무장 · 무도가")
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 보유 골드
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GoldText;

	// 결과 안내 ("골드가 부족합니다" 등)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> MainMenuButton;

	// 화면마다 카드 / 줄이 채워지는 곳
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> ContentBox;

	// 카드 크기
	UPROPERTY(EditAnywhere, Category = "Base")
	FVector2D CardSize = FVector2D(220.f, 320.f);

	// 세력 화면에 띄울 세력 (황실은 거점이 없음)
	UPROPERTY(EditAnywhere, Category = "Base")
	TArray<EFaction> Factions = { EFaction::AdventurersGuild, EFaction::MageTower, EFaction::Religion };

	// 직업 화면 최소 카드 수 (모자라면 '추가 예정' 카드)
	UPROPERTY(EditAnywhere, Category = "Base")
	int32 MinClassCards = 3;

private:
	EBaseView View = EBaseView::Faction;
	EFaction SelectedFaction = EFaction::AdventurersGuild;
	ECharacterClass SelectedClass = ECharacterClass::Fighter;

	// 지금 화면의 직업 카드 순서 (Index -> 직업)
	TArray<ECharacterClass> ShownClasses;

	UPROPERTY()
	TArray<TObjectPtr<UBaseClickRelay>> Relays;

	FDelegateHandle UpgradesChangedHandle;

	void BuildDefaultLayout();
	void ShowView(EBaseView InView);
	void Rebuild();

	void BuildFactionView();
	void BuildClassView();
	void BuildFacilityView();
	void BuildStatHallView();
	void BuildSkillHallView();

	UButton* AddCard(const FString& Label, EBaseView InView, int32 Index, bool bEnabled = true);
	UButton* MakeRelayButton(const FString& Label, EBaseView InView, int32 Index, int32 FontSize);

	void SetMessage(const FString& Text, bool bError = false);
	void RefreshGold();
	void HandleEscape();
	FText GetClassDisplayName(ECharacterClass InClass) const;

	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleMainMenuClicked();
	UFUNCTION() void HandleGoldChanged(int32 NewGold, int32 Delta);
};
