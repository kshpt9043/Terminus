#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TerminusUISettings.generated.h"

class UBaseWidget;
class UChatWidget;
class UChoiceCardWidget;
class UCombatHUDWidget;
class UCommsMenuWidget;
class UConfirmPopupWidget;
class UEventWidget;
class UFloorTitleWidget;
class UFloorVoteWidget;
class UItemCardWidget;
class UItemPickPanelWidget;
class UItemSlotWidget;
class UMainMenuWidget;
class UMapScreenWidget;
class UMonsterRewardWidget;
class UPartyCommsWidget;
class UPartyProfileCardWidget;
class URelicBarWidget;
class URelicTooltipWidget;
class URescueWidget;
class URestWidget;
class URiftWidget;
class URunSaveEntryWidget;
class URunSaveListWidget;
class USettlementWidget;
class UShopWidget;
class USkillCardWidget;
class USkillTooltipWidget;
class USpectateWidget;
class UStartRelicPickWidget;
class UStartSkillPickWidget;
class UStorageWidget;
class UTavernWidget;

/**
 * 위젯 클래스를 한 곳에 모은 곳 (사용자 결정 2026-10-12)
 * 에디터 Project Settings > Game > Terminus UI 에 뜨고, 값은 Config/DefaultGame.ini 에 저장됨
 *
 * 클래스를 고르는 순서 (UTerminusUISettings::Pick)
 *  1. 그 화면 / PC 블루프린트에 직접 지정한 값 (특정 곳만 다르게 하고 싶을 때 덮어쓰기)
 *  2. 여기 값
 *  3. 둘 다 비면 C++ 기본 모양
 *
 * 그래서 WBP 를 만들면 여기 한 번만 넣으면 주점 / 던전 / 메인 메뉴 PC 가 모두 같이 씀
 * 칸 / 카드 / 툴팁 같은 부품도 여기 넣으면 그 부품을 쓰는 모든 화면에 적용됨
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Terminus UI"))
class TERMINUS_API UTerminusUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTerminusUISettings() { CategoryName = TEXT("Game"); }

	static const UTerminusUISettings* Get() { return GetDefault<UTerminusUISettings>(); }

	// 덮어쓴 값 -> 여기 값 -> C++ 기본 순서로 클래스 고르기
	template <typename T>
	static TSubclassOf<T> Pick(const TSubclassOf<T>& Override, TSoftClassPtr<T> UTerminusUISettings::* Field)
	{
		if (Override) return Override;
		if (UClass* Loaded = (Get()->*Field).LoadSynchronous()) return Loaded;
		return T::StaticClass();
	}

	// ---------------------------------------------------------------- 메뉴 / 주점
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<UMainMenuWidget> MainMenu;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<UTavernWidget> Tavern;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<UBaseWidget> Base;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<UStorageWidget> Storage;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<URunSaveListWidget> SaveList;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<URunSaveEntryWidget> SaveEntry;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftClassPtr<USettlementWidget> Settlement;

	// ---------------------------------------------------------------- 공용 화면
	UPROPERTY(Config, EditAnywhere, Category = "Common") TSoftClassPtr<UConfirmPopupWidget> ConfirmPopup;
	UPROPERTY(Config, EditAnywhere, Category = "Common") TSoftClassPtr<UChatWidget> Chat;
	UPROPERTY(Config, EditAnywhere, Category = "Common") TSoftClassPtr<UPartyCommsWidget> PartyComms;
	UPROPERTY(Config, EditAnywhere, Category = "Common") TSoftClassPtr<UCommsMenuWidget> CommsMenu;

	// ---------------------------------------------------------------- 던전 화면
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UMapScreenWidget> MapScreen;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UCombatHUDWidget> CombatHUD;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UStartSkillPickWidget> StartSkillPick;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UStartRelicPickWidget> StartRelicPick;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UMonsterRewardWidget> MonsterReward;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<URestWidget> Rest;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UEventWidget> Event;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UShopWidget> Shop;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<URescueWidget> Rescue;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<USpectateWidget> Spectate;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UFloorVoteWidget> FloorVote;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<UFloorTitleWidget> FloorTitle;
	UPROPERTY(Config, EditAnywhere, Category = "Dungeon") TSoftClassPtr<URiftWidget> Rift;

	// ---------------------------------------------------------------- 부품 (여러 화면이 같이 씀)
	// 스킬 / 유물 칸 (전투 HUD 스킬 칸, 유물바, 상점, 보상, 창고, 정산 ...)
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<UItemSlotWidget> ItemSlot;
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<USkillTooltipWidget> SkillTooltip;
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<URelicTooltipWidget> RelicTooltip;
	// 스킬 카드 (보상 / 시작 스킬 고르기)
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<USkillCardWidget> SkillCard;
	// 선택지 카드 (휴식 선택지 / 이벤트 카드 / 층 투표 카드 / 관전 버튼)
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<UChoiceCardWidget> ChoiceCard;
	// 칸 + 설명 + 버튼 카드 (상점 진열 / 보상 유물)
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<UItemCardWidget> ItemCard;
	// 바꿀 칸 고르기 패널 (이벤트 / 상점 / 보상)
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<UItemPickPanelWidget> ItemPickPanel;
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<URelicBarWidget> RelicBar;
	UPROPERTY(Config, EditAnywhere, Category = "Parts") TSoftClassPtr<UPartyProfileCardWidget> ProfileCard;
};
