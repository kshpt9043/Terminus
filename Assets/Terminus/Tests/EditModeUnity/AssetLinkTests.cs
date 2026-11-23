// 표(CSV) -> 유니티 에셋 연결 검사. 에셋을 읽어야 해서 유니티 Test Runner 에서만 돔 (dotnet 테스트엔 안 들어감)
// 언리얼 자동 테스트 Terminus.Data.MonsterVisuals / SkillIcons 의 에셋 부분
using NUnit.Framework;
using Terminus.Art;
using Terminus.Data;
using Terminus.Game;

namespace Terminus.Tests
{
    public class AssetLinkTests
    {
        [OneTimeSetUp]
        public void Load() => GameDataLoader.Load();

        [Test]
        public void EveryMonsterVisualRow_HasAllSixActions()
        {
            Assert.Greater(GameData.MonsterVisuals.Count, 0);
            foreach (var row in GameData.MonsterVisuals.Rows)
            {
                Assert.NotNull(GameData.FindMonsterRow(row.RowName), $"{row.RowName}: DT_Monster 에 없는 행");

                var visual = MonsterVisualSet.LoadBySource(row.Source);
                Assert.NotNull(visual, $"{row.RowName}: '{row.Source}' 그림이 없음 (Terminus/Art/Build Monster Visuals)");
                foreach (MonsterAction action in System.Enum.GetValues(typeof(MonsterAction)))
                {
                    var fb = visual.Get(action);
                    Assert.NotNull(fb, $"{row.RowName}: {action} 플립북 없음");
                    Assert.Greater(fb.FrameCount, 0, $"{row.RowName}: {action} 프레임 없음");
                    foreach (var frame in fb.Frames) Assert.NotNull(frame, $"{row.RowName}: {action} 빈 프레임");
                }
            }
        }

        [Test]
        public void BossSlimes_UseTheirOwnArt()
        {
            // 퓨전 슬라임 / 슬라임 킹 그림 (언리얼 6dcfbf6, 128px). 예전엔 큰 파란 슬라임을 임시로 씀
            Assert.AreEqual("slime_fusion", MonsterVisualSet.SourceToName(GameData.FindMonsterVisualRow("mon_fusionslime").Source));
            Assert.AreEqual("slime_king", MonsterVisualSet.SourceToName(GameData.FindMonsterVisualRow("mon_slimeking").Source));
            var king = MonsterVisualSet.LoadBySource("PixelLab/slime_king");
            Assert.AreEqual(128f, king.Idle.Frames[0].rect.width, "보스는 128px");
        }

        [Test]
        public void EveryRelicAndSkillIcon_Resolves()
        {
            foreach (var r in GameData.Relics.Rows)
            {
                if (string.IsNullOrEmpty(r.Icon)) continue;
                Assert.NotNull(GameIcons.FindRelicIcon(r.Icon), $"유물 {r.RowName}: 아이콘 '{r.Icon}' 없음");
            }
            foreach (var s in GameData.Skills.Rows)
            {
                if (Ids.IsNone(s.IconID)) continue;
                Assert.NotNull(GameIcons.FindSkillIcon(s.IconID, s.SkillType.ToString()), $"스킬 {s.RowName}: 아이콘 '{s.IconID}' 없음");
            }
        }

        [Test]
        public void EveryClass_HasCharacterArt()
        {
            foreach (CharacterClass c in System.Enum.GetValues(typeof(CharacterClass)))
            {
                var visual = UnityEngine.Resources.Load<MonsterVisualSet>("Visuals/Characters/" + c);
                Assert.NotNull(visual, $"{c} 그림 없음");
                Assert.Greater(visual.Idle.FrameCount, 0);
            }
        }
    }
}
