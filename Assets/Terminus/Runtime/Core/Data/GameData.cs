// 프로젝트에 하나뿐인 마스터 데이터 (언리얼 UTerminusDataSettings 이식)
// 주점이든 던전이든 여기서 꺼내 쓴다. 엔진을 모르게 하려고 CSV 글자를 받아서 채움
//  - 유니티: Terminus.Game.GameDataLoader 가 Resources/Data/*.csv 를 읽어 Load 호출
//  - 테스트 / 서버: 파일 경로에서 직접 읽어 Load 호출
using System;
using System.Collections.Generic;

namespace Terminus.Data
{
    public static class GameData
    {
        // CSV 파일 이름 (확장자 없음). 언리얼 DT 에셋 이름과 같음
        public const string CharacterClassTableName = "DT_CharacterClass";
        public const string SkillTableName = "DT_Skill";
        public const string MonsterTableName = "DT_Monster";
        public const string MonsterVisualTableName = "DT_MonsterVisual";
        public const string RelicTableName = "DT_Relic";
        public const string EventTableName = "DT_Event";
        public const string EventChoiceTableName = "DT_EventChoice";
        public const string QuestTableName = "DT_Quest";
        public const string QuestRoomTableName = "DT_QuestRoom";

        public static readonly string[] AllTableNames =
        {
            CharacterClassTableName, SkillTableName, MonsterTableName, MonsterVisualTableName, RelicTableName,
            EventTableName, EventChoiceTableName, QuestTableName, QuestRoomTableName,
        };

        public static DataTable<CharacterClassRow> CharacterClasses { get; private set; } = new DataTable<CharacterClassRow>(CharacterClassTableName);
        public static DataTable<SkillRow> Skills { get; private set; } = new DataTable<SkillRow>(SkillTableName);
        public static DataTable<MonsterRow> Monsters { get; private set; } = new DataTable<MonsterRow>(MonsterTableName);
        public static DataTable<MonsterVisualRow> MonsterVisuals { get; private set; } = new DataTable<MonsterVisualRow>(MonsterVisualTableName);
        public static DataTable<RelicRow> Relics { get; private set; } = new DataTable<RelicRow>(RelicTableName);
        public static DataTable<EventRow> Events { get; private set; } = new DataTable<EventRow>(EventTableName);
        public static DataTable<EventChoiceRow> EventChoices { get; private set; } = new DataTable<EventChoiceRow>(EventChoiceTableName);
        public static DataTable<QuestRow> Quests { get; private set; } = new DataTable<QuestRow>(QuestTableName);
        public static DataTable<QuestRoomRow> QuestRooms { get; private set; } = new DataTable<QuestRoomRow>(QuestRoomTableName);

        public static UpgradeSettings Upgrades { get; set; } = UpgradeSettings.Default;

        public static bool IsLoaded { get; private set; }

        /// <summary>읽다가 나온 경고 전부 (표 이름 포함)</summary>
        public static IEnumerable<string> AllWarnings
        {
            get
            {
                foreach (var w in CharacterClasses.Warnings) yield return w;
                foreach (var w in Skills.Warnings) yield return w;
                foreach (var w in Monsters.Warnings) yield return w;
                foreach (var w in MonsterVisuals.Warnings) yield return w;
                foreach (var w in Relics.Warnings) yield return w;
                foreach (var w in Events.Warnings) yield return w;
                foreach (var w in EventChoices.Warnings) yield return w;
                foreach (var w in Quests.Warnings) yield return w;
                foreach (var w in QuestRooms.Warnings) yield return w;
            }
        }

        /// <summary>
        /// 표 이름 -> CSV 글자를 돌려주는 함수로 전부 채운다. 없는 표는 null 을 돌려주면 빈 표.
        /// 다시 부르면 갈아끼움 (핫 리로드)
        /// </summary>
        public static void Load(Func<string, string> readCsv)
        {
            DataTable<T> Read<T>(string name) where T : class, ITableRow, new()
            {
                var text = readCsv(name);
                return text == null ? new DataTable<T>(name) : DataTable<T>.FromCsv(name, text);
            }

            CharacterClasses = Read<CharacterClassRow>(CharacterClassTableName);
            Skills = Read<SkillRow>(SkillTableName);
            Monsters = Read<MonsterRow>(MonsterTableName);
            MonsterVisuals = Read<MonsterVisualRow>(MonsterVisualTableName);
            Relics = Read<RelicRow>(RelicTableName);
            Events = Read<EventRow>(EventTableName);
            EventChoices = Read<EventChoiceRow>(EventChoiceTableName);
            Quests = Read<QuestRow>(QuestTableName);
            QuestRooms = Read<QuestRoomRow>(QuestRoomTableName);
            _skillIdToRow = null;
            IsLoaded = true;
        }

        // ------------------------------------------------------------ 조회 (언리얼 FindXxxRow)

        public static CharacterClassRow FindCharacterClassRow(CharacterClass c)
        {
            foreach (var row in CharacterClasses.Rows)
            {
                if (row.Class == c) return row;
            }
            return null;
        }

        public static SkillRow FindSkillRow(string rowName) => Skills.Find(rowName);
        public static MonsterRow FindMonsterRow(string rowName) => Monsters.Find(rowName);
        public static MonsterVisualRow FindMonsterVisualRow(string rowName) => MonsterVisuals.Find(rowName);
        public static RelicRow FindRelicRow(string rowName) => Relics.Find(rowName);
        public static EventRow FindEventRow(string rowName) => Events.Find(rowName);
        public static EventChoiceRow FindEventChoiceRow(string rowName) => EventChoices.Find(rowName);
        public static QuestRow FindQuestRow(string rowName) => Quests.Find(rowName);
        public static QuestRoomRow FindQuestRoomRow(string rowName) => QuestRooms.Find(rowName);

        static Dictionary<string, string> _skillIdToRow;

        /// <summary>행 이름 또는 SkillID(SK_SLI_001) -> 행 이름. 둘 다 없으면 ""</summary>
        public static string ResolveSkillRow(string rowOrSkillId)
        {
            if (Ids.IsNone(rowOrSkillId)) return "";
            if (Skills.Contains(rowOrSkillId)) return rowOrSkillId;

            if (_skillIdToRow == null)
            {
                _skillIdToRow = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (var row in Skills.Rows)
                {
                    if (!string.IsNullOrEmpty(row.SkillID) && !_skillIdToRow.ContainsKey(row.SkillID)) _skillIdToRow.Add(row.SkillID, row.RowName);
                }
            }
            return _skillIdToRow.TryGetValue(rowOrSkillId, out var found) ? found : "";
        }

        // 화면에 보일 이름. 표에 없으면 행 이름 그대로
        public static string GetSkillName(string rowName)
        {
            var s = FindSkillRow(rowName);
            return s != null ? s.DisplayName_KR : rowName;
        }

        public static string GetRelicName(string rowName)
        {
            var r = FindRelicRow(rowName);
            return r != null ? r.RelicName : rowName;
        }

        /// <summary>직업의 기본 스킬. 공격 방어 특수 순서 (데이터에 없는 칸은 빠짐)</summary>
        public static List<SkillRow> FindBasicSkills(CharacterClass c)
        {
            var owner = CharacterNames.ToSkillOwner(c);
            var result = new List<SkillRow>();
            foreach (var row in Skills.Rows)
            {
                if (row.OwnerClass == owner && row.SkillCategory == SkillCategory.Basic) result.Add(row);
            }
            // 표 순서에 기대지 않고 공격 방어 특수 순 (안정 정렬)
            var sorted = new List<SkillRow>(result.Count);
            foreach (SkillType t in Enum.GetValues(typeof(SkillType)))
            {
                foreach (var row in result)
                {
                    if (row.SkillType == t) sorted.Add(row);
                }
            }
            return sorted;
        }
    }
}
