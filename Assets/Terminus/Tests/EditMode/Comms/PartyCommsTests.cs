// 파티 소통 도구: 문구 / 예상 피해 / 서버 판정 (핑 / 재촉 받는 사람) / 요청 간격
using System.Collections.Generic;
using NUnit.Framework;
using Terminus.Combat;
using Terminus.Comms;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;

namespace Terminus.Tests
{
    [TestFixture]
    public class PartyCommsTests
    {
        [OneTimeSetUp]
        public void LoadData() => TestData.LoadGameData();

        // ------------------------------------------------------------ 문구

        [Test]
        public void Texts_MatchUnreal()
        {
            Assert.AreEqual("좋아요!", PartyComms.GetQuickChatText(QuickChat.Good));
            Assert.AreEqual("가자!", PartyComms.GetQuickChatText(QuickChat.Go));
            Assert.AreEqual("집중 공격", PartyComms.GetPingText(PingKind.Focus));
            Assert.AreEqual("회복 필요", PartyComms.GetPingText(PingKind.NeedHeal));
            Assert.AreEqual("FF5A4A", PartyComms.GetPingColorHex(PingKind.Focus));
            Assert.IsTrue(PartyComms.IsEnemyPing(PingKind.Danger));
            Assert.IsFalse(PartyComms.IsEnemyPing(PingKind.NeedGuard));
            Assert.IsTrue(PartyComms.IsQuickChatText("도와주세요!"));
            Assert.IsFalse(PartyComms.IsQuickChatText("도와줘"));
            Assert.IsFalse(PartyComms.IsValidQuickChat((QuickChat)6));
            Assert.IsFalse(PartyComms.IsValidPingKind((PingKind)4));
        }

        [Test]
        public void PingLine_RoundTripsKind()
        {
            var line = PartyComms.FormatPingLine("홍길동", "적 2", PingKind.NeedGuard);
            Assert.AreEqual("[핑] 홍길동 -> 적 2: 보호 필요", line);
            Assert.IsTrue(PartyComms.FindPingKind(line, out var kind));
            Assert.AreEqual(PingKind.NeedGuard, kind);
            Assert.IsFalse(PartyComms.FindPingKind("그냥 말", out _));
        }

        [Test]
        public void DescribeEstimate_LethalMark()
        {
            Assert.AreEqual("예상 피해 약 14 / 체력 15", PartyComms.DescribeEstimate(14, 15, 0, out var lethal));
            Assert.IsFalse(lethal);
            Assert.AreEqual("예상 피해 약 14 / 체력 12 (+2)  처치!", PartyComms.DescribeEstimate(14, 12, 2, out lethal));
            Assert.IsTrue(lethal);
            Assert.AreEqual("", PartyComms.DescribeEstimate(0, 12, 2, out _));
        }

        [Test]
        public void EstimateHit_FollowsDamageOrder()
        {
            var caster = Stats();
            var target = Stats();
            Assert.AreEqual(10, PartyComms.EstimateHit(10, caster, target));
            Assert.AreEqual(0, PartyComms.EstimateHit(0, caster, target));

            // 공포 -25% -> 급소 +25%: 10 -> 8 (7.5 반올림) -> 10
            caster.ApplyStatus(StatusEffect.Fear, 0, 1);
            Assert.AreEqual(8, PartyComms.EstimateHit(10, caster, target));
            target.ApplyStatus(StatusEffect.Mark, 0, 1);
            Assert.AreEqual(10, PartyComms.EstimateHit(10, caster, target));

            // 철벽 감소
            target.ApplyStatus(StatusEffect.IronWall, 3, 1);
            Assert.AreEqual(7, PartyComms.EstimateHit(10, caster, target));

            // 불굴: 1 고정 -> 철벽으로 0
            target.ApplyStatus(StatusEffect.Indomitable, 0, 1);
            Assert.AreEqual(0, PartyComms.EstimateHit(10, caster, target));
        }

        static CombatStats Stats()
        {
            var s = new CombatStats();
            s.InitFrom(new CharacterStats { MaxHealth = 50 });
            return s;
        }

        // ------------------------------------------------------------ 서버 판정 (보통 전투)

        static (DungeonCombatKit kit, DungeonPlayer a, DungeonPlayer b) Fight()
        {
            var kit = new DungeonCombatKit(5);
            var a = DungeonCombatPlayers.Make(1);
            var b = DungeonCombatPlayers.Make(2, CharacterClass.Paladin);
            kit.StartForced(RoomType.MONSTER, new[] { "mon_slime", "mon_redslime" }, a, b);
            return (kit, a, b);
        }

        [Test]
        public void Ping_TargetSideRules()
        {
            var (kit, a, b) = Fight();
            var occ = kit.Combat.Players;
            var m = kit.Combat.Monsters[0];

            Assert.IsTrue(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Focus, CommsTarget.Monster(m.Id)));
            Assert.IsFalse(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.NeedHeal, CommsTarget.Monster(m.Id)), "아군 핑은 몬스터에게 못 찍음");
            Assert.IsTrue(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.NeedHeal, CommsTarget.Player(b.Id)));
            Assert.IsTrue(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.NeedGuard, CommsTarget.Player(a.Id)), "나에게도 찍음");
            Assert.IsFalse(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Danger, CommsTarget.Player(b.Id)));
            Assert.IsFalse(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Focus, CommsTarget.Monster(9999)));
            Assert.IsFalse(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Focus, CommsTarget.None));

            DungeonCombatMonsters.Kill(m);
            Assert.IsFalse(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Focus, CommsTarget.Monster(m.Id)), "죽은 몬스터");

            var recipients = PartyCommsRules.GetAreaRecipients(occ, kit.Combat, a, true);
            CollectionAssert.AreEquivalent(new[] { a, b }, recipients);
        }

        [Test]
        public void Nudge_OnlyWhenWaiting_ToThoseStillPlaying()
        {
            var (kit, a, b) = Fight();
            var occ = kit.Combat.Players;
            Assert.IsEmpty(PartyCommsRules.GetNudgeTargets(occ, kit.Combat, a), "아직 내 턴이면 재촉 못 함");

            kit.Combat.HandleEndTurn(a);
            CollectionAssert.AreEqual(new[] { b }, PartyCommsRules.GetNudgeTargets(occ, kit.Combat, a));
            Assert.IsTrue(PartyCommsRules.IsWaiting(kit.Combat, a));
            Assert.IsFalse(PartyCommsRules.IsWaiting(kit.Combat, b));
        }

        // ------------------------------------------------------------ 배신 전투 (편)

        [Test]
        public void Betrayal_SidesSplitRecipientsAndTargets()
        {
            var kit = new DungeonCombatKit(31);
            var a = DungeonCombatPlayers.Make(1);
            var b = DungeonCombatPlayers.Make(2, CharacterClass.Paladin);
            var traitor = DungeonCombatPlayers.Make(3);
            kit.Combat.StartBetrayal(new[] { a, b, traitor }, traitor, 0.5f, 2);
            var occ = new List<DungeonPlayer> { a, b, traitor };

            CollectionAssert.AreEquivalent(new[] { a, b }, PartyCommsRules.GetAreaRecipients(occ, kit.Combat, a, true));
            CollectionAssert.AreEquivalent(new[] { traitor }, PartyCommsRules.GetAreaRecipients(occ, kit.Combat, traitor, true));
            CollectionAssert.AreEquivalent(occ, PartyCommsRules.GetAreaRecipients(occ, kit.Combat, a, false));

            Assert.IsTrue(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Focus, CommsTarget.Player(traitor.Id)), "배신자 = 적");
            Assert.IsFalse(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.Focus, CommsTarget.Player(b.Id)));
            Assert.IsTrue(PartyCommsRules.CanPing(occ, kit.Combat, a, PingKind.NeedHeal, CommsTarget.Player(b.Id)));

            // 팀 차례: 배신자는 기다리는 쪽 -> 팀원에게 재촉
            CollectionAssert.AreEquivalent(new[] { a, b }, PartyCommsRules.GetNudgeTargets(occ, kit.Combat, traitor));
        }

        // ------------------------------------------------------------ 간격

        [Test]
        public void Throttle_Cooldowns()
        {
            var t = new CommsThrottle();
            Assert.IsTrue(t.TryPing(10));
            Assert.IsFalse(t.TryPing(10.3));
            Assert.IsTrue(t.TryPing(10.41));

            Assert.IsTrue(t.TryQuickChat(0));
            Assert.IsFalse(t.TryQuickChat(0.9));
            Assert.IsTrue(t.TryQuickChat(1.0));

            Assert.IsTrue(t.CanNudge(0));
            t.NudgeSent(0);
            Assert.IsFalse(t.CanNudge(7.9));
            Assert.IsTrue(t.CanNudge(8));

        }
    }
}
