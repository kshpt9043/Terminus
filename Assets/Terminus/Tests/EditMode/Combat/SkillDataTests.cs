// 전투가 쓰는 데이터 확인 (언리얼 Tests/SkillExecutorTests.cpp 의 Terminus.Data.*)
using System;
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Data;

namespace Terminus.Tests
{
    public class SkillDataTests
    {
        [OneTimeSetUp]
        public void Load() => TestData.LoadGameData();

        // Terminus.Data.MonsterSkillRefs
        // 모든 몬스터의 스킬 칸이 실제 스킬 행을 가리키는지 (10-26: SkillID 로 적힌 칸 때문에 몬스터가 아무 행동도 안 하던 버그)
        // 언리얼과 같이 행 이름으로만 찾음 (DataTableTests.MonsterSkills_Resolve 는 SkillID 도 허용하는 느슨한 버전)
        [Test]
        public void MonsterSkillRefs()
        {
            var failures = new List<string>();
            int checkedCount = 0;
            foreach (var m in GameData.Monsters.Rows)
            {
                foreach (var skill in new[] { m.Skill1_ID, m.Skill2_ID, m.Skill3_ID })
                {
                    if (Ids.IsNone(skill)) continue;
                    ++checkedCount;
                    if (GameData.FindSkillRow(skill) == null) failures.Add($"{m.RowName}: 스킬 '{skill}' 이 DT_Skill 행");
                }
            }
            TestContext.WriteLine($"몬스터 스킬 칸 {checkedCount}개 확인");
            Assert.IsEmpty(failures, string.Join("\n", failures));
            Assert.Greater(checkedCount, 0);
        }

        // Terminus.Data.StatusTexts
        // 상태이상마다 화면 이름 + 효과 설명이 있음 (전투 툴팁 / 몬스터 카드 / 스킬 툴팁 상태이상 창이 씀)
        [Test]
        public void StatusTexts()
        {
            var failures = new List<string>();
            foreach (StatusEffect type in Enum.GetValues(typeof(StatusEffect)))
            {
                if (type == StatusEffect.None) continue;
                if (string.IsNullOrEmpty(StatusInfo.GetDisplayName(type))) failures.Add($"{type}: 화면 이름");
                if (string.IsNullOrEmpty(StatusInfo.GetDescription(type))) failures.Add($"{type}: 효과 설명");
            }
            Assert.IsEmpty(failures, string.Join("\n", failures));
        }

        // Terminus.Data.SkillIcons 중 데이터 부분
        // 플레이어가 쓰는 스킬(몬스터 스킬 빼고)은 IconID = SkillID
        // TODO(Game): 언리얼은 T_Icon_<IconID> 텍스처가 실제로 있는지도 봤음. 아이콘 에셋이 유니티로 넘어오면 PlayMode / 에디터 테스트로
        [Test]
        public void SkillIcons_IconIdMatchesSkillId()
        {
            var failures = new List<string>();
            int checkedCount = 0;
            foreach (var row in GameData.Skills.Rows)
            {
                if (row.OwnerClass == SkillOwner.Monster) continue;
                ++checkedCount;
                if (row.IconID != row.SkillID) failures.Add($"{row.RowName}: IconID '{row.IconID}' != SkillID '{row.SkillID}'");
            }
            TestContext.WriteLine($"플레이어 스킬 아이콘 {checkedCount}개 확인");
            Assert.IsEmpty(failures, string.Join("\n", failures));
            Assert.Greater(checkedCount, 0);
        }

        // Terminus.Data.MonsterVisuals 중 데이터 부분: 외형 표의 행마다 DT_Monster 행이 있음
        // TODO(Game): 언리얼은 플립북 6개가 다 있고 스프라이트가 시트의 정사각형 칸 하나씩인지도 봤음 (10-26 UV 틀어짐). 스프라이트 에셋 쪽 테스트로
        [Test]
        public void MonsterVisuals_HaveMonsterRow()
        {
            var failures = new List<string>();
            foreach (var row in GameData.MonsterVisuals.Rows)
            {
                if (GameData.FindMonsterRow(row.RowName) == null) failures.Add($"{row.RowName}: DT_Monster 에 있음");
            }
            Assert.IsEmpty(failures, string.Join("\n", failures));
            Assert.Greater(GameData.MonsterVisuals.Count, 0);
        }
    }
}
