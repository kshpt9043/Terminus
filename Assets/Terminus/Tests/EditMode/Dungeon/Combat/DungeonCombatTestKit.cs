// 던전 전투 테스트 공용: 전투 하나 + 바깥 알림 기록 + 시간 넘기기, 테스트용 플레이어 만들기
using System;
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Tests
{
    public sealed class DungeonCombatKit
    {
        public readonly DungeonCombatHost Host = new DungeonCombatHost();
        public readonly DungeonCombatSettings Settings = new DungeonCombatSettings();
        public readonly DungeonCombat Combat;
        public double Now;

        // 바깥 알림 기록
        public readonly List<DungeonMonster> Spawned = new List<DungeonMonster>();
        public readonly List<DungeonMonster> Removed = new List<DungeonMonster>();
        public readonly List<DungeonMonster> HideRequested = new List<DungeonMonster>();
        public readonly List<DungeonMonster> Acted = new List<DungeonMonster>();
        public readonly List<KeyValuePair<DungeonPlayer, RoomRewardOffer>> Offers = new List<KeyValuePair<DungeonPlayer, RoomRewardOffer>>();
        public readonly List<DungeonPlayer> Closed = new List<DungeonPlayer>();
        public readonly List<KeyValuePair<DungeonPlayer, bool>> BetrayalResults = new List<KeyValuePair<DungeonPlayer, bool>>();
        public int Cleared;
        public int Wiped;

        /// <param name="deterministic">몬스터 체력 / 공방 랜덤을 끔 (기대값을 바로 계산할 수 있게)</param>
        public DungeonCombatKit(int seed = 1, bool deterministic = true)
        {
            if (deterministic)
            {
                Settings.MonsterHealthRandom = new CombatFloatRange(0f, 0f);
                Settings.MonsterStatRandomMax = 0;
            }
            // 다타는 한꺼번에 (타격 간격은 DungeonCombatSkillTests.MultiHit_* 에서 따로 봄)
            Settings.HitInterval = 0f;
            Combat = new DungeonCombat(Settings, Host, new Random(seed));
            Combat.MonsterSpawned += m => Spawned.Add(m);
            Combat.MonsterRemoved += m => Removed.Add(m);
            Combat.MonsterHideRequested += m => HideRequested.Add(m);
            Combat.MonsterActing += (m, kind) => Acted.Add(m);
            Combat.RoomRewardOffered += (p, o) => Offers.Add(new KeyValuePair<DungeonPlayer, RoomRewardOffer>(p, o));
            Combat.RoomRewardClosed += p => Closed.Add(p);
            Combat.BetrayalFinished += (b, won) => BetrayalResults.Add(new KeyValuePair<DungeonPlayer, bool>(b, won));
            Combat.AreaCleared += () => ++Cleared;
            Combat.AreaWiped += () => ++Wiped;
        }

        // 걸린 일 하나만 실행 (지금 걸린 일의 시각까지 시간을 넘김)
        public void Step()
        {
            Now = Math.Max(Now, Combat.PendingStepTime);
            Combat.Tick(Now);
        }

        // 몬스터 턴을 끝까지 돌려 다음 플레이어 턴(또는 승패)까지
        public void RunMonsterTurn(int max = 50)
        {
            for (int i = 0; i < max && Combat.Phase == CombatPhase.MonsterTurn; ++i) Step();
        }

        public void EndTurnAll()
        {
            foreach (var p in new List<DungeonPlayer>(Combat.Players)) Combat.HandleEndTurn(p);
        }

        public void Start(RoomType type, DungeonTheme theme, params DungeonPlayer[] players)
        {
            Combat.StartCombat(new RoomNode { RoomId = 7, Type = type }, players, theme);
        }

        public void StartForced(RoomType type, string[] monsters, params DungeonPlayer[] players)
        {
            Combat.SetNextForcedMonsters(monsters);
            Start(type, DungeonCombatThemes.Slime, players);
        }
    }

    public static class DungeonCombatThemes
    {
        public static readonly DungeonTheme Slime = new DungeonTheme("slime", "슬라임 왕국");
        public static readonly DungeonTheme Lizard = new DungeonTheme("lizard", "리자드의 늪");
        public static readonly DungeonTheme Elemental = new DungeonTheme("elemental", "정령계");
        public static readonly DungeonTheme Unknown = new DungeonTheme("none", "없는 테마");
    }

    public static class DungeonCombatPlayers
    {
        /// <summary>런을 시작한 플레이어. 기본은 직업 기본 유물까지 빼고 relics 만 들게 함</summary>
        public static DungeonPlayer Make(ulong id, CharacterClass c = CharacterClass.Fighter, bool keepBasicRelic = false, params string[] relics)
        {
            var run = new RunPlayer(new Random((int)id + 100), "P" + id);
            run.SetCharacterClass(c);
            run.BeginRun();
            run.ModifyRunState(s =>
            {
                if (!keepBasicRelic) s.Relics.Clear();
                s.Relics.AddRange(relics);
            });

            var stats = new CombatStats { DebugName = "P" + id };
            stats.InitFrom(run.RunState.Stats);
            return new DungeonPlayer(id, run, stats);
        }

        public static void Kill(DungeonPlayer p)
        {
            p.Battler.PreventDeath = null;
            p.Battler.ApplyDamage(p.Battler.State.Health + p.Battler.State.Shield + 1000);
        }

        public static int Hp(DungeonPlayer p) => p.Battler.State.Health;
    }

    public static class DungeonCombatMonsters
    {
        public static int Hp(DungeonMonster m) => m.Stats.State.Health;

        public static void Kill(DungeonMonster m)
        {
            m.Stats.PreventDeath = null;
            m.Stats.ClearCombatEffects();
            m.Stats.ApplyDamage(m.Stats.State.Health + m.Stats.State.Shield + 1000);
        }
    }
}
