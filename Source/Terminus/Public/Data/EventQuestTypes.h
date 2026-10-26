#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EventQuestTypes.generated.h"

// 이벤트 방 / 퀘스트 데이터 (10-24 기획 엑셀 Event_DataTable.xlsx, Quest_DataTable.xlsx)
// 프로퍼티 이름 = CSV 컬럼 이름, 열거형 값 이름 = 엑셀 값 (Enum_Reference 시트 순서). 새 값은 맨 뒤에만
// 아직 데이터만 들어와 있음 -> 이벤트 방은 임시 규칙(ADungeonArea), 퀘스트는 미구현

// ---------------------------------------------------------------------
// 이벤트
// ---------------------------------------------------------------------

// 기획서 기준 10대 이벤트 컨셉 (공용 7 + 테마 3)
UENUM(BlueprintType)
enum class EEventConceptType : uint8
{
	ClassOrCommonSkill,   // 직업 / 공용 스킬 획득
	CrossClassSkill,      // 다른 직업 스킬 획득
	EventPoolSkill,       // 이벤트 전용 스킬 획득
	TempStatRelic,        // 일시적 스탯 유물 (3전투)
	PermStatRelic,        // 영구 스탯 유물
	RelicSwap,            // 보유 유물 변경
	RandomCurrency,       // 던전 재화 랜덤
	ThemeRelic,           // 테마 / 퀘스트 유물
	ThemeTempStat,        // 테마 일시적 스탯 (공용보다 높음)
	WeakGuardianBattle    // 쉬는 가디언 전투 (체력 70%)
};

// 이벤트가 나오는 계층. 공용 이벤트는 Any
UENUM(BlueprintType)
enum class EEventAppearTier : uint8
{
	Any,
	Surface,
	Mid,
	Deep
};

// 선택지를 고를 때 치르는 대가
UENUM(BlueprintType)
enum class EEventCostType : uint8
{
	None,
	LoseHP,               // 현재 체력 CostValue 감소
	LoseMaxHP,            // 최대 체력 CostValue 영구 감소
	SpendCurrency,        // 던전 재화 CostValue 지불
	LoseRandomSkill,      // 픽업 스킬 CostValue 개 무작위로 잃음
	LoseRandomRelic,      // 유물 1개 무작위로 잃음
	LoseRandomRelicAndHP, // 유물 1개 + 체력 / 재화
	GainDebuffRelic       // 3전투 디버프 유물(RLC_Event_Temp_006) 획득
};

// 선택지 결과
UENUM(BlueprintType)
enum class EEventOutcomeType : uint8
{
	GiveOwnClassSkill,
	GiveCommonSkill,
	GiveOtherClassSkill,
	GiveEventSkill,
	UpgradeRandomSkill,
	GiveTempStatRelic,
	GivePermStatRelic,
	SwapRelicSameTierRandom,
	SwapRelicHigherTierRandom,
	GiveCurrencyRange,        // OutcomeValueMin ~ Max 던전 재화
	GiveThemeQuestRelic,
	GiveHigherTierRelic,
	StartWeakGuardianBattle,  // 가디언을 최대 체력 OutcomeValueMin% 로
	HealHP,                   // OutcomeValueMin 만큼 회복
	Leave,
	GiveHigherTierSkill
};

/** 이벤트 하나. Event_DataTable 시트 한 행. 선택지는 Choice1~3_ID 로 DT_EventChoice 를 가리킴 */
USTRUCT(BlueprintType)
struct FEventRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText EventName_KR;

	// "공용" 또는 테마 이름 ("슬라임 왕국" 등. 몬스터 Theme_KR 과 같은 글자)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EventScope;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEventAppearTier AppearTier = EEventAppearTier::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEventConceptType EventConceptType = EEventConceptType::ClassOrCommonSkill;

	// 등장 가중치
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AppearanceWeight = 0;

	// 한 런에 한 번만
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOneTimePerRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText SituationDesc_KR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ChoiceCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Choice1_ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Choice2_ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Choice3_ID;

	// 기획 메모. 런타임에선 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FString Design_Notes;
};

/** 이벤트 선택지 하나. Event_Choices 시트 한 행 (행 이름 = ChoiceID) */
USTRUCT(BlueprintType)
struct FEventChoiceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ChoiceIndex = 0;

	// 버튼 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText ChoiceTitle_KR;

	// 버튼 아래 대가 / 결과 안내
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText ChoiceTooltip_KR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEventCostType CostType = EEventCostType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CostValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEventOutcomeType OutcomeType = EEventOutcomeType::Leave;

	// 보상 풀 이름(Pool_Skill_OwnClass_CurrentTier_Random 등) 또는 특정 행 이름(RLC_Event_Temp_001)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName OutcomeTargetPool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 OutcomeValueMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 OutcomeValueMax = 0;

	// 일시적 유물의 유지 전투 수 (-1 = 영구). 유물 RemainBattle 과 같은 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BattleDuration_Limit = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText ResultText_KR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FString Choice_Design_Notes;
};

// ---------------------------------------------------------------------
// 퀘스트
// ---------------------------------------------------------------------

UENUM(BlueprintType)
enum class EQuestConditionType : uint8
{
	None,
	KillMonster,             // ConditionTargetID = MonsterID 또는 몬스터 분류(Guardian 등)
	EarnCurrency,
	AcquireRelic,            // ConditionTargetID = 유물 행 이름
	CompleteSpecialRoom,     // ConditionTargetID = 특수 방 RoomID
	Betrayal,
	ReachFloor,
	EnterRoomInSingleFloor   // 한 층 안에서 특정 방에 N 번 입장
};

UENUM(BlueprintType)
enum class EQuestRewardType : uint8
{
	None,
	GiveCurrency,
	GiveRelic,               // TargetID = 유물 행 이름
	GiveSkill,               // TargetID = 스킬 행 이름
	UpgradeSkill             // Value 개 랜덤 강화
};

/** 퀘스트 하나. Quest_DataTable 시트 한 행 (행 이름 = QuestID) */
USTRUCT(BlueprintType)
struct FQuestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText QuestName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText QuestDesc;

	// 세력 이름 그대로 ("마탑", "모험가 길드", "테르미누스", ...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Force;

	// 테마 이름 또는 계층 이름 ("슬라임 왕국", "표층", ...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Theme;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AppearanceWeight = 0;

	// 수락하면 지도에 특수 방(SpecialRoomID)을 넣음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInjectSpecialRoom = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SpecialRoomID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQuestConditionType ConditionType = EQuestConditionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ConditionTargetID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ConditionValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQuestRewardType AcceptRewardType = EQuestRewardType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AcceptRewardTargetID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AcceptRewardValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQuestRewardType ClearRewardType = EQuestRewardType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ClearRewardTargetID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ClearRewardValue = 0;
};

/** 퀘스트가 지도에 넣는 특수 방. Room_DataTable 시트 한 행 (행 이름 = RoomID) */
USTRUCT(BlueprintType)
struct FQuestRoomRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Theme;

	// 테마 안의 몇 번째 층 (1 / 2)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Floor = 1;

	// 나오는 몬스터의 MonsterID (DT_Monster 행 이름이 아님)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SpawnMonster;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SpawnValue = 1;

	// 지도에서 넣을 줄 (레벨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Depth = 1;
};
