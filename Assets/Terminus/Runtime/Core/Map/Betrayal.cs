// 배신 (언리얼 AMapManager::StartBetrayalBattle 의 판단 + HandleBetrayalResult 의 정산 조정 계산)
// 전투 자체(DungeonAreaSubsystem::StartBetrayal)와 정산 화면 띄우기(Client_BeginSettlement)는 Game 쪽
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Map
{
    /// <summary>런 끝 사람마다 안내 / 정산 조정 (언리얼 AMapManager::FRunEndResult)</summary>
    public sealed class RunEndResult
    {
        public string Message = "";
        public SettlementAdjust Adjust = new SettlementAdjust();
    }

    /// <summary>배신 결과 계산에 넣는 플레이어 하나 (언리얼 ATerminusPlayerState 에서 필요한 것만)</summary>
    public sealed class BetrayalParticipant
    {
        public ulong PlayerId = 0;
        public string PlayerName = "";
        public RunState Run = new RunState();

        public BetrayalParticipant() { }

        public BetrayalParticipant(ulong playerId, string playerName, RunState run)
        {
            PlayerId = playerId;
            PlayerName = playerName ?? "";
            Run = run ?? new RunState();
        }
    }

    public static class BetrayalRules
    {
        public const string MsgNoOpponent = "배신할 상대가 없어 던전을 탈출합니다.";
        public const string MsgBattleOver = "배신 전투가 끝나 던전을 나갑니다.";

        /// <summary>배신 전투에 넣는 순서: 팀 먼저, 배신자 마지막 (DungeonAreaSubsystem::StartBetrayal 에 넘김)</summary>
        public static List<ulong> BuildBattleOrder(IEnumerable<ulong> allPlayers, ulong? betrayerId)
        {
            var players = new List<ulong>();
            foreach (ulong id in allPlayers)
            {
                if (!betrayerId.HasValue || id != betrayerId.Value) players.Add(id);
            }
            if (betrayerId.HasValue) players.Add(betrayerId.Value);
            return players;
        }

        /// <summary>
        /// 배신 전투를 열 수 있나 (배신자가 있고 둘 이상). false 거나 구역 쪽이 전투를 못 열면
        /// FloorProgression.EndRunByEscape(MsgNoOpponent)
        /// </summary>
        public static bool CanStartBattle(ulong? betrayerId, IReadOnlyCollection<ulong> battleOrder)
        {
            return betrayerId.HasValue && battleOrder != null && battleOrder.Count >= 2;
        }

        public static string StartNotice(string betrayerName) => $"{betrayerName} 님이 배신했습니다! 배신 전투가 시작됩니다.";

        /// <summary>
        /// [서버] 배신 전투가 끝남. 결과대로 각자 정산 조정을 만든다
        ///  - 이긴 쪽이 보스 유물을 가짐: 배신자 승리면 동료들의 보스 유물이 배신자 정산 후보로
        ///  - 배신자 패배: 배신자의 보스 유물을 팔아 그 골드를 나머지가 나눠 가짐
        ///  - 진 쪽은 이번 보스전에서 얻은 유물 / 스킬을 정산에서 못 가져감. 나머지는 평소 탈출 정산
        /// players = 지금 접속한 전원 (배신자 포함). findRelic 을 비우면 GameData.FindRelicRow
        /// </summary>
        public static Dictionary<ulong, RunEndResult> ComputeBetrayalResult(ulong? betrayerId, bool bBetrayerWon,
            IReadOnlyList<BetrayalParticipant> players, Func<string, RelicRow> findRelic = null)
        {
            findRelic = findRelic ?? GameData.FindRelicRow;

            var team = new List<BetrayalParticipant>();
            BetrayalParticipant betrayer = null;
            if (players != null)
            {
                foreach (var p in players)
                {
                    if (p == null) continue;
                    if (betrayerId.HasValue && p.PlayerId == betrayerId.Value) betrayer = p;
                    else team.Add(p);
                }
            }

            RunState betrayerRun = betrayer?.Run ?? new RunState();
            string betrayerName = betrayer != null ? betrayer.PlayerName : "배신자";
            var results = new Dictionary<ulong, RunEndResult>();

            void LoseBossRewards(RunState run, SettlementAdjust adjust)
            {
                if (!Ids.IsNone(run.BossRelic)) adjust.LostRelics.Add(run.BossRelic);
                if (!Ids.IsNone(run.BossSkill)) adjust.LostSkills.Add(run.BossSkill);
            }

            if (bBetrayerWon)
            {
                // 배신자: 동료들의 보스 유물을 가져감 / 동료: 보스전 유물 / 스킬을 못 가져감
                // (배신자를 모르면 언리얼은 nullptr 키로 넣어 아무에게도 안 감 -> 여기선 안 넣음)
                RunEndResult winner = new RunEndResult { Message = "배신에 성공했습니다! 동료들의 보스 유물을 빼앗았습니다." };
                if (betrayerId.HasValue) results[betrayerId.Value] = winner;

                foreach (var ps in team)
                {
                    RunState run = ps.Run ?? new RunState();
                    if (!Ids.IsNone(run.BossRelic) && !winner.Adjust.ExtraRelics.Contains(run.BossRelic))
                    {
                        winner.Adjust.ExtraRelics.Add(run.BossRelic);
                    }

                    var loser = new RunEndResult { Message = $"배신자 {betrayerName} 에게 패배했습니다. 보스전에서 얻은 유물과 스킬을 잃었습니다." };
                    LoseBossRewards(run, loser.Adjust);
                    results[ps.PlayerId] = loser;
                }
            }
            else
            {
                // 배신자: 보스전 유물 / 스킬을 못 가져감 / 동료: 배신자 보스 유물을 판 골드를 나눠 가짐
                if (betrayerId.HasValue)
                {
                    var loser = new RunEndResult { Message = "배신에 실패했습니다. 보스전에서 얻은 유물과 스킬을 잃었습니다." };
                    LoseBossRewards(betrayerRun, loser.Adjust);
                    results[betrayerId.Value] = loser;
                }

                RelicRow bossRelic = Ids.IsNone(betrayerRun.BossRelic) ? null : findRelic(betrayerRun.BossRelic);
                int pot = bossRelic != null && bossRelic.CanSellAtSettlement() ? bossRelic.SellPrice_Gold : 0;
                int share = team.Count > 0 ? pot / team.Count : 0;

                foreach (var ps in team)
                {
                    var winner = new RunEndResult { Message = $"배신자 {betrayerName} 을(를) 물리쳤습니다!" };
                    if (share > 0)
                    {
                        winner.Adjust.BonusGold = share;
                        winner.Adjust.BonusReason = $"배신자의 보스 유물 [{bossRelic.RelicName}] 판매 분배";
                    }
                    results[ps.PlayerId] = winner;
                }
            }

            return results;
        }
    }
}
