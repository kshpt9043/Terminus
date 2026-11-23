// 이벤트 방 / 퀘스트 데이터 (언리얼 Data/EventQuestTypes.h 이식)
// 10-24 기획 엑셀 Event_DataTable.xlsx, Quest_DataTable.xlsx. 새 값은 맨 뒤에만
namespace Terminus.Data
{
    // ---------------------------------------------------------------- 이벤트

    // 10대 이벤트 컨셉 (공용 7 + 테마 3)
    public enum EventConceptType
    {
        ClassOrCommonSkill,
        CrossClassSkill,
        EventPoolSkill,
        TempStatRelic,
        PermStatRelic,
        RelicSwap,
        RandomCurrency,
        ThemeRelic,
        ThemeTempStat,
        WeakGuardianBattle,
    }

    public enum EventAppearTier
    {
        Any,
        Surface,
        Mid,
        Deep,
    }

    public enum EventCostType
    {
        None,
        LoseHP,
        LoseMaxHP,
        SpendCurrency,
        LoseRandomSkill,
        LoseRandomRelic,
        LoseRandomRelicAndHP,
        GainDebuffRelic,
    }

    public enum EventOutcomeType
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
        HealHP,
        Leave,
        GiveHigherTierSkill,
    }

    /// <summary>이벤트 하나. DT_Event.csv 한 행. 선택지는 Choice1~3_ID 로 DT_EventChoice 를 가리킴</summary>
    [System.Serializable]
    public sealed class EventRow : ITableRow
    {
        public string RowName { get; set; }

        public string EventID = "";
        public string EventName_KR = "";
        // "공용" 또는 테마 이름 (몬스터 Theme_KR 과 같은 글자)
        public string EventScope = "";
        public EventAppearTier AppearTier = EventAppearTier.Any;
        public EventConceptType EventConceptType = EventConceptType.ClassOrCommonSkill;
        public int AppearanceWeight = 0;
        public bool bOneTimePerRun = false;
        public string SituationDesc_KR = "";
        public int ChoiceCount = 0;
        [NameField] public string Choice1_ID = "";
        [NameField] public string Choice2_ID = "";
        [NameField] public string Choice3_ID = "";
        public string Design_Notes = "";
    }

    /// <summary>이벤트 선택지 하나. DT_EventChoice.csv 한 행 (행 이름 = ChoiceID)</summary>
    [System.Serializable]
    public sealed class EventChoiceRow : ITableRow
    {
        public string RowName { get; set; }

        public string EventID = "";
        public int ChoiceIndex = 0;
        public string ChoiceTitle_KR = "";
        public string ChoiceTooltip_KR = "";
        public EventCostType CostType = EventCostType.None;
        public int CostValue = 0;
        public EventOutcomeType OutcomeType = EventOutcomeType.Leave;
        // 보상 풀 이름(Pool_Skill_OwnClass_CurrentTier_Random 등) 또는 특정 행 이름
        [NameField] public string OutcomeTargetPool = "";
        public int OutcomeValueMin = 0;
        public int OutcomeValueMax = 0;
        // 일시적 유물의 유지 전투 수 (-1 = 영구)
        public int BattleDuration_Limit = -1;
        public string ResultText_KR = "";
        public string Choice_Design_Notes = "";
    }

    // ---------------------------------------------------------------- 퀘스트

    public enum QuestConditionType
    {
        None,
        KillMonster,             // ConditionTargetID = MonsterID 또는 몬스터 분류(Guardian 등)
        EarnCurrency,
        AcquireRelic,
        CompleteSpecialRoom,
        Betrayal,
        ReachFloor,
        EnterRoomInSingleFloor,  // 한 층 안에서 특정 방에 N 번 입장
    }

    public enum QuestRewardType
    {
        None,
        GiveCurrency,
        GiveRelic,
        GiveSkill,
        UpgradeSkill,
    }

    /// <summary>퀘스트 하나. DT_Quest.csv 한 행 (행 이름 = QuestID)</summary>
    [System.Serializable]
    public sealed class QuestRow : ITableRow
    {
        public string RowName { get; set; }

        public string QuestName = "";
        public string QuestDesc = "";
        // 세력 이름 그대로 ("마탑", "모험가 길드", "테르미누스", ...)
        public string Force = "";
        // 테마 이름 또는 계층 이름
        public string Theme = "";
        public int AppearanceWeight = 0;
        public bool bInjectSpecialRoom = false;
        [NameField] public string SpecialRoomID = "";
        public QuestConditionType ConditionType = QuestConditionType.None;
        [NameField] public string ConditionTargetID = "";
        public int ConditionValue = 0;
        public QuestRewardType AcceptRewardType = QuestRewardType.None;
        [NameField] public string AcceptRewardTargetID = "";
        public int AcceptRewardValue = 0;
        public QuestRewardType ClearRewardType = QuestRewardType.None;
        [NameField] public string ClearRewardTargetID = "";
        public int ClearRewardValue = 0;
    }

    /// <summary>퀘스트가 지도에 넣는 특수 방. DT_QuestRoom.csv 한 행 (행 이름 = RoomID)</summary>
    [System.Serializable]
    public sealed class QuestRoomRow : ITableRow
    {
        public string RowName { get; set; }

        public string Theme = "";
        // 테마 안의 몇 번째 층 (1 / 2)
        public int Floor = 1;
        // 나오는 몬스터의 MonsterID (DT_Monster 행 이름이 아님)
        [NameField] public string SpawnMonster = "";
        public int SpawnValue = 1;
        // 지도에서 넣을 줄 (레벨)
        public int Depth = 1;
    }
}
