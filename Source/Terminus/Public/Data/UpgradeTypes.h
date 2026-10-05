#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/CharacterTypes.h"
#include "UpgradeTypes.generated.h"

/**
 * 거점(직업의 탑) 강화. 직업마다 따로, 골드로 사고 프로필(영구)에 저장
 *  - 연무장: 기본 스테이터스 강화 (EStatUpgrade)
 *  - 훈련소: 기본 스킬 3개 강화 (BaseValue +1, BaseValue 가 0 이면 StatusValue +1)
 * 런을 시작할 때(BeginRun) 고른 직업의 강화가 런 스텟에 더해짐
 */

// 연무장 강화 항목. 순서 = 화면 순서 = FClassUpgrades::StatLevels 인덱스 (뒤에만 추가할 것)
UENUM(BlueprintType)
enum class EStatUpgrade : uint8
{
	Health        UMETA(DisplayName = "체력"),
	Attack        UMETA(DisplayName = "공격"),
	Defense       UMETA(DisplayName = "방어"),
	Evasion       UMETA(DisplayName = "회피"),
	Energy        UMETA(DisplayName = "에너지"),
	SkillEnergy   UMETA(DisplayName = "스킬 에너지"),     // 전투 시작 때 갖고 시작하는 스킬 에너지
	RelicCapacity UMETA(DisplayName = "유물 최대치"),
	PickupSlots   UMETA(DisplayName = "픽업 스킬 칸"),    // 강화 스킬 칸

	MAX UMETA(Hidden)
};

// 화면에 띄울 이름 (UMETA DisplayName 은 에디터 전용)
TERMINUS_API FText GetStatUpgradeName(EStatUpgrade Stat);

// 연무장 항목 하나의 규칙
USTRUCT(BlueprintType)
struct FStatUpgradeRule
{
	GENERATED_BODY()

	// 최대 강화 단계
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MaxLevel = 5;

	// 첫 강화 비용 (골드)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 BaseCost = 1000;

	// 단계마다 늘어나는 비용 (기획: 강화할수록 비용 증가, 수치 미정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 CostStep = 500;

	// 한 단계에 오르는 수치
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountPerLevel = 1;
};

// 직업 하나의 강화 단계. 프로필에 저장되고, 멀티에선 손님이 서버에 알려 줌 (TMap 은 RPC 에 못 실어서 배열)
USTRUCT(BlueprintType)
struct FClassUpgrades
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECharacterClass Class = ECharacterClass::Fighter;

	// 연무장. 인덱스 = EStatUpgrade
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> StatLevels;

	// 훈련소. 인덱스 = 기본 스킬 칸 (0~2, 전투 HUD 기본 칸 순서와 같음)
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> SkillLevels;

	int32 GetStatLevel(EStatUpgrade Stat) const
	{
		const int32 Index = static_cast<int32>(Stat);
		return StatLevels.IsValidIndex(Index) ? StatLevels[Index] : 0;
	}

	int32 GetSkillLevel(int32 SkillIndex) const
	{
		return SkillLevels.IsValidIndex(SkillIndex) ? SkillLevels[SkillIndex] : 0;
	}
};

/**
 * 강화 비용 / 한도 / 증가량. Project Settings > Game > Terminus Upgrades
 * 기획(UI 레퍼런스 > 직업의 탑) 기준 기본값. 비용 증가 폭과 단계별 수치는 기획 미정이라 임시
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Terminus Upgrades"))
class TERMINUS_API UTerminusUpgradeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTerminusUpgradeSettings();

	static const UTerminusUpgradeSettings* Get() { return GetDefault<UTerminusUpgradeSettings>(); }

	// 연무장 항목별 규칙
	UPROPERTY(Config, EditAnywhere, Category = "연무장")
	TMap<EStatUpgrade, FStatUpgradeRule> StatRules;

	// 훈련소 (기본 스킬 3개 공통)
	UPROPERTY(Config, EditAnywhere, Category = "훈련소", meta = (ClampMin = "0"))
	int32 SkillMaxLevel = 3;

	UPROPERTY(Config, EditAnywhere, Category = "훈련소", meta = (ClampMin = "0"))
	int32 SkillBaseCost = 5000;

	UPROPERTY(Config, EditAnywhere, Category = "훈련소", meta = (ClampMin = "0"))
	int32 SkillCostStep = 0;

	const FStatUpgradeRule* FindStatRule(EStatUpgrade Stat) const { return StatRules.Find(Stat); }

	// 다음 단계 비용. 이미 최대면 -1
	int32 GetStatCost(EStatUpgrade Stat, int32 CurrentLevel) const;
	int32 GetSkillCost(int32 CurrentLevel) const;

	// 강화를 런 스텟에 더함 (BeginRun)
	void ApplyToStats(const FClassUpgrades& Upgrades, FCharacterStats& InOutStats) const;

	// 단계를 규칙 한도 안으로 (손님이 보낸 값 검증)
	FClassUpgrades Sanitize(const FClassUpgrades& In) const;
};
