// 던전을 도는 동안 유지되는 데이터 (언리얼 Data/RunTypes.h 이식)
// 행 이름 참조(FName)는 string. 빈 문자열 = 없음 (언리얼 NAME_None)
using System.Collections.Generic;

namespace Terminus.Data
{
    // 일시 스탯 버프 (이벤트 방). 전투 시작 때 상태로 걸리고(공격 = 용기, 방어 감소 = 용암), 전투가 끝날 때마다 1 줄어듦
    [System.Serializable]
    public sealed class TempStatBuff
    {
        public int Attack = 0;
        public int Defense = 0;        // 음수면 방어 감소
        public int BattlesLeft = 0;
        public string Source = "";     // 어디서 받았는지 (표시용)

        public TempStatBuff Clone() => (TempStatBuff)MemberwiseClone();
    }

    // 일시적 이벤트 유물 (유물 RemainBattle > 0). 전투가 끝날 때마다 1 줄고 0 이 되면 유물이 사라짐
    [System.Serializable]
    public sealed class TempRelicEntry
    {
        public string Relic = "";
        public int BattlesLeft = 0;

        public TempRelicEntry Clone() => (TempRelicEntry)MemberwiseClone();
    }

    // 진행 중 퀘스트 (DT_Quest 행). 테마 하나 동안 하나
    [System.Serializable]
    public sealed class QuestProgress
    {
        public string Quest = "";
        public int Progress = 0;
        public bool bCompleted = false;

        public QuestProgress Clone() => (QuestProgress)MemberwiseClone();
    }

    // 픽업 스킬 강화 단계 (상점 / 유물 RandomUpgrade). 스킬 행 이름으로 찾음
    [System.Serializable]
    public sealed class SkillLevelEntry
    {
        public string Skill = "";
        public int Level = 0;

        public SkillLevelEntry Clone() => (SkillLevelEntry)MemberwiseClone();
    }

    // 런이 끝날 때 서버가 정산에 더하거나 빼라고 알려 주는 것 (배신 결과)
    [System.Serializable]
    public sealed class SettlementAdjust
    {
        // 정산 유물 후보에 더함 (배신 성공: 동료들의 보스 유물)
        public List<string> ExtraRelics = new List<string>();
        // 정산에서 뺌 (배신에서 진 쪽: 보스전에서 얻은 유물 / 스킬)
        public List<string> LostRelics = new List<string>();
        public List<string> LostSkills = new List<string>();
        // 추가 골드와 그 이유
        public int BonusGold = 0;
        public string BonusReason = "";

        public SettlementAdjust Clone()
        {
            var c = (SettlementAdjust)MemberwiseClone();
            c.ExtraRelics = new List<string>(ExtraRelics);
            c.LostRelics = new List<string>(LostRelics);
            c.LostSkills = new List<string>(LostSkills);
            return c;
        }
    }

    /// <summary>던전을 도는 동안 유지되는 상태. 맵을 넘어가도 살아야 함</summary>
    [System.Serializable]
    public sealed class RunState
    {
        public CharacterClass CharacterClass = CharacterClass.Fighter;
        public CharacterStats Stats = new CharacterStats();

        // Stats 가 이번 런용으로 채워졌는지. 주점 출발(BeginRun) 때 true
        public bool bStatsInitialized = false;

        // 장착한 강화 스킬 (DT_Skill 행 이름). 최대 Stats.EnhanceSlots 칸
        public List<string> EnhanceSkills = new List<string>();

        // 런 시작 때 강화 스킬 / 창고 유물 고르기를 마쳤는지. false 인 동안은 지도에서 방을 못 고름
        public bool bStartSkillChosen = false;
        public bool bStartRelicsChosen = false;

        // 던전에 들어갈 때 창고에서 들고 간 유물. 정산에서 고르는 후보에서 뺌
        public List<string> StartRelics = new List<string>();

        // 이번 층 보스방 보상으로 받은 유물 / 스킬 (배신 결과에 씀). 층이 바뀌면 비움
        public string BossRelic = "";
        public string BossSkill = "";

        // 하드 모드 (방장이 정함). 켜면 런 시작 때 들고 간 창고 유물이 창고에서 사라짐
        public bool bHardMode = false;

        // 던전 재화. 런 동안만 유효
        public int Currency = 0;

        // 보유 유물 (DT_Relic 행 이름). 처음엔 직업 기본 유물(RLC_<직업>_001) 하나
        public List<string> Relics = new List<string>();

        // 맵 진행
        public int CurrentRoomId = -1;   // 방금 클리어하고 나온 방 (-1: 시작 전)
        public int CurrentMapLevel = 0;  // 현재 진행 레벨 (Row 0 ~ 11)
        public int SelectedRoomId = -1;  // 선택한 다음 방 (-1: 선택 안 함)

        // 이번 층에서 지나온 방들 (들어간 순서). 층이 바뀌면 비움
        public List<int> VisitedRoomIds = new List<int>();

        // 훈련소 기본 스킬 강화 단계 (0~2 칸). 런 시작 때 프로필에서 복사
        public List<int> BasicSkillLevels = new List<int>();

        // 픽업 스킬 강화 단계 (이번 런만)
        public List<SkillLevelEntry> PickupSkillLevels = new List<SkillLevelEntry>();

        // 이번 런에서 한 번이라도 나온 유물. 한 번 나온 유물은 그 런 동안 다시 안 나옴
        public List<string> SeenRelics = new List<string>();

        public List<TempStatBuff> TempBuffs = new List<TempStatBuff>();
        public List<TempRelicEntry> TempRelics = new List<TempRelicEntry>();

        // 세이브에서 이어할 때의 체력. 처음 스폰될 때 적용하고 -1 로 비움 (-1 = 최대 체력)
        public int SavedHealth = -1;

        // 퀘스트
        public QuestProgress ActiveQuest = new QuestProgress();
        public int FloorMonsterRooms = 0;
        public int FloorGuardianRooms = 0;
        public int QuestGold = 0;

        public RunState Clone()
        {
            var c = (RunState)MemberwiseClone();
            c.Stats = Stats.Clone();
            c.EnhanceSkills = new List<string>(EnhanceSkills);
            c.StartRelics = new List<string>(StartRelics);
            c.Relics = new List<string>(Relics);
            c.VisitedRoomIds = new List<int>(VisitedRoomIds);
            c.BasicSkillLevels = new List<int>(BasicSkillLevels);
            c.PickupSkillLevels = PickupSkillLevels.ConvertAll(e => e.Clone());
            c.SeenRelics = new List<string>(SeenRelics);
            c.TempBuffs = TempBuffs.ConvertAll(e => e.Clone());
            c.TempRelics = TempRelics.ConvertAll(e => e.Clone());
            c.ActiveQuest = ActiveQuest.Clone();
            return c;
        }
    }
}
