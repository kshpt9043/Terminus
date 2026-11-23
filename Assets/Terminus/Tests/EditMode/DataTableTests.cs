// 기획 CSV 가 C# 타입과 맞는지 (언리얼 CSV 임포트에서 조용히 버려지던 열 / 값을 여기서 잡음)
using System.Linq;
using NUnit.Framework;
using Terminus.Data;
using Terminus.Data.Csv;

namespace Terminus.Tests
{
    public class DataTableTests
    {
        [OneTimeSetUp]
        public void Load() => TestData.LoadGameData();

        [Test]
        public void CsvReader_HandlesQuotesCommasAndNewlines()
        {
            var rows = CsvReader.Parse("﻿RowName,A,B\nr1,\"x, y\",\"line1\nline2\"\nr2,\"say \"\"hi\"\"\",3\n");
            Assert.AreEqual(3, rows.Count);
            Assert.AreEqual("x, y", rows[1][1]);
            Assert.AreEqual("line1\nline2", rows[1][2]);
            Assert.AreEqual("say \"hi\"", rows[2][1]);
        }

        [Test]
        public void AllTables_LoadWithoutWarnings()
        {
            var warnings = GameData.AllWarnings.ToList();
            Assert.IsEmpty(warnings, string.Join("\n", warnings));
        }

        [Test]
        public void AllTables_HaveRows()
        {
            Assert.Greater(GameData.CharacterClasses.Count, 0);
            Assert.Greater(GameData.Skills.Count, 0);
            Assert.Greater(GameData.Monsters.Count, 0);
            Assert.Greater(GameData.MonsterVisuals.Count, 0);
            Assert.Greater(GameData.Relics.Count, 0);
            Assert.Greater(GameData.Events.Count, 0);
            Assert.Greater(GameData.EventChoices.Count, 0);
            Assert.Greater(GameData.Quests.Count, 0);
            Assert.Greater(GameData.QuestRooms.Count, 0);
        }

        [Test]
        public void EveryClass_HasRowAndThreeBasicSkills()
        {
            foreach (CharacterClass c in System.Enum.GetValues(typeof(CharacterClass)))
            {
                Assert.NotNull(GameData.FindCharacterClassRow(c), c.ToString());
                var basics = GameData.FindBasicSkills(c);
                Assert.AreEqual(3, basics.Count, c + " 기본 스킬 수");
                Assert.AreEqual(SkillType.Attack, basics[0].SkillType);
                Assert.AreEqual(SkillType.Defense, basics[1].SkillType);
                Assert.AreEqual(SkillType.Special, basics[2].SkillType);
            }
        }

        [Test]
        public void MonsterSkills_Resolve()
        {
            foreach (var m in GameData.Monsters.Rows)
            {
                foreach (var id in new[] { m.Skill1_ID, m.Skill2_ID, m.Skill3_ID })
                {
                    if (Ids.IsNone(id)) continue;
                    Assert.IsNotEmpty(GameData.ResolveSkillRow(id), $"{m.RowName} 의 스킬 '{id}' 가 DT_Skill 에 없음");
                }
            }
        }

        [Test]
        public void EventChoices_Exist()
        {
            foreach (var e in GameData.Events.Rows)
            {
                foreach (var id in new[] { e.Choice1_ID, e.Choice2_ID, e.Choice3_ID })
                {
                    if (Ids.IsNone(id)) continue;
                    Assert.NotNull(GameData.FindEventChoiceRow(id), $"{e.RowName} 의 선택지 '{id}' 가 없음");
                }
            }
        }

        [Test]
        public void SampleRow_ParsedExactly()
        {
            var s = GameData.FindSkillRow("fighter_attack");
            Assert.NotNull(s);
            Assert.AreEqual("SK_FTR_001", s.SkillID);
            Assert.AreEqual(SkillOwner.Fighter, s.OwnerClass);
            Assert.AreEqual(ActionKind.HitEnemyMulti, s.ActionKind);
            Assert.AreEqual(3, s.BaseValue);
            Assert.AreEqual(ScalingStat.ATK, s.ScalingStat);
            Assert.AreEqual(2, s.HitCount);

            var q = GameData.FindQuestRow("Q_SLI_001");
            Assert.NotNull(q);
            Assert.AreEqual("", q.SpecialRoomID, "FName 필드의 None 은 빈 문자열");
            Assert.AreEqual(QuestConditionType.KillMonster, q.ConditionType);
        }
    }
}
