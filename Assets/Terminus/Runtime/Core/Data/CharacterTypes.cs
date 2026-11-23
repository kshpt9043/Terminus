// 직업 / 세력 (언리얼 Data/CharacterTypes.h 이식)
namespace Terminus.Data
{
    public enum CharacterClass
    {
        Fighter,   // 무도가
        Engineer,  // 마도 공학자
        Paladin,   // 성기사
        Assassin,  // 암살자
    }

    public enum Faction
    {
        AdventurersGuild, // 모험가 길드
        MageTower,        // 마탑
        Religion,         // 테르미누스
        Empire,           // 황실
    }

    public static class CharacterNames
    {
        public const int ClassCount = 4;

        public static string GetClassName(CharacterClass c)
        {
            switch (c)
            {
                case CharacterClass.Fighter: return "무도가";
                case CharacterClass.Engineer: return "마도 공학자";
                case CharacterClass.Paladin: return "성기사";
                case CharacterClass.Assassin: return "암살자";
                default: return "";
            }
        }

        public static string GetFactionName(Faction f)
        {
            switch (f)
            {
                case Faction.AdventurersGuild: return "모험가 길드";
                case Faction.MageTower: return "마탑";
                case Faction.Religion: return "테르미누스";
                case Faction.Empire: return "황실";
                default: return "";
            }
        }

        // 직업 열거형과 스킬 소유자 열거형은 따로 있다(공용 / 몬스터 때문에). 여기서 한 번만 잇는다
        public static SkillOwner ToSkillOwner(CharacterClass c)
        {
            switch (c)
            {
                case CharacterClass.Fighter: return SkillOwner.Fighter;
                case CharacterClass.Engineer: return SkillOwner.Engineer;
                case CharacterClass.Paladin: return SkillOwner.Paladin;
                case CharacterClass.Assassin: return SkillOwner.Assassin;
                default: return SkillOwner.Shared;
            }
        }
    }

    /// <summary>
    /// 직업 정의. DT_CharacterClass.csv 한 행 (언리얼에선 CSV 없이 에디터에서 만든 DataTable 이었음).
    /// BaseStats 는 "BaseStats.MaxHealth" 같은 점 표기 열, 또는 언리얼 내보내기 형식 "(MaxHealth=70,Attack=0,...)" 둘 다 읽음
    /// </summary>
    [System.Serializable]
    public sealed class CharacterClassRow : ITableRow
    {
        public string RowName { get; set; }

        public CharacterClass Class = CharacterClass.Fighter;
        public string DisplayName = "";
        public string Description = "";
        public string PassiveText = "";
        // 일러스트 경로 (언리얼 경로 또는 Resources 경로). 게임 쪽에서 파일 이름으로 찾음
        public string Illustration = "";
        public Faction Faction = Faction.AdventurersGuild;
        // 강화 전 기본 스텟. 던전에서 늘어난 값은 RunState 가 들고 있음
        public CharacterStats BaseStats = new CharacterStats();
        // 언리얼 ZD 애님블프 경로. 유니티에선 Animator 컨트롤러 이름으로 매핑
        public string AnimInstanceClass = "";
    }
}
