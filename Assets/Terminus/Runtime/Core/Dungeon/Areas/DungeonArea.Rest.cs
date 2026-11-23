// 휴식터 (언리얼 ADungeonArea::BeginRest / HandleRestChoice / HandleRestExplore / CheckRestDone / FinishRest)
// 휴식 / 탐색 중 하나만 (기획 10-07, 사용자 결정 10-08: 선택지 화면). 한 사람 한 번, 전원이 고르면 방이 끝남
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonArea
    {
        // 휴식터: 이미 고른 사람 (서버만)
        readonly HashSet<DungeonPlayer> _restChosen = new HashSet<DungeonPlayer>();
        bool _bResting;
        readonly TimerHandle _restTimer = new TimerHandle();

        public bool IsResting => _bResting;
        public bool HasChosenRest(DungeonPlayer p) => p != null && _restChosen.Contains(p);

        void BeginRest()
        {
            _bResting = true;
            _restChosen.Clear();

            var present = new List<DungeonPlayer>(_occupants);
            var s = Settings;
            foreach (var p in _occupants)
            {
                Notifier?.ShowRest(p, present, s.RestHealRatio, s.RestExploreCurrencyMin, s.RestExploreCurrencyMax, s.RestExploreRelicChance);
            }

            if (present.Count == 0)
            {
                MarkCleared();
            }
        }

        /// <summary>휴식: 회복 대상 고르기 (PC 의 Server_ChooseRestTarget). 같은 휴식터 사람만. 대상은 최대 체력의 RestHealRatio 만큼 회복</summary>
        public void HandleRestChoice(DungeonPlayer chooser, DungeonPlayer target)
        {
            if (!_bResting || chooser == null || target == null) return;
            if (!_occupants.Contains(chooser) || !_occupants.Contains(target)) return;   // 같은 휴식터 사람만
            if (_restChosen.Contains(chooser)) return;                                   // 한 번만

            _restChosen.Add(chooser);

            var stats = target.Battler;
            if (stats != null)
            {
                int amount = System.Math.Max(1, CombatMath.RoundToInt(stats.Stats.MaxHealth * Settings.RestHealRatio));
                stats.Heal(amount);

                Notifier?.RestResult(chooser, chooser == target
                    ? $"푹 쉬었습니다. 체력을 {amount} 회복했습니다."
                    : $"{target.Name} 님의 체력을 {amount} 회복시켰습니다.");

                if (_occupants.Count > 1)
                {
                    Chat(chooser == target
                        ? $"{chooser.Name} 님이 휴식으로 체력을 {amount} 회복했습니다."
                        : $"{chooser.Name} 님이 {target.Name} 님의 체력을 {amount} 회복시켰습니다.");
                }

                Log($"휴식: {chooser.Name} -> {target.Name} +{amount}");
            }

            CheckRestDone();
        }

        /// <summary>탐색 (PC 의 Server_ChooseRestExplore): 던전 재화 RestExploreCurrency + RestExploreRelicChance 확률로 유물</summary>
        public void HandleRestExplore(DungeonPlayer chooser)
        {
            if (!_bResting || chooser == null) return;
            if (!_occupants.Contains(chooser) || _restChosen.Contains(chooser)) return;   // 휴식 / 탐색 중 하나만, 한 번

            _restChosen.Add(chooser);

            var s = Settings;
            int lo = System.Math.Min(s.RestExploreCurrencyMin, s.RestExploreCurrencyMax);
            int hi = System.Math.Max(s.RestExploreCurrencyMin, s.RestExploreCurrencyMax);
            int currency = Rng.Next(lo, hi + 1);   // FMath::RandRange (양 끝 포함)
            chooser.Run?.AddCurrency(currency);
            string result = $"주변을 뒤져 던전 재화 {currency} 을(를) 찾았습니다.";

            if (Rng.NextDouble() < s.RestExploreRelicChance)
            {
                var found = Combat != null ? Combat.PickRewardRelics(chooser, 1) : new List<string>();
                if (found != null && found.Count > 0 && chooser.Run != null)
                {
                    var relicName = GameData.GetRelicName(found[0]);

                    chooser.Run.MarkRelicsSeen(found);   // 못 가져가도 이번 런엔 다시 안 나옴
                    result += chooser.Run.GainRelic(found[0])
                        ? $"\n그리고 유물 [{relicName}] 을(를) 발견했습니다!"
                        : $"\n유물 [{relicName}] 을(를) 발견했지만 유물 칸이 가득 차 가져가지 못했습니다.";
                }
            }

            Notifier?.RestResult(chooser, result);

            if (_occupants.Count > 1)
            {
                Chat($"{chooser.Name} 님이 휴식터를 탐색했습니다. {result.Replace("\n", " ")}");
            }

            Log($"탐색: {chooser.Name} -> {result.Replace("\n", " / ")}");

            CheckRestDone();
        }

        void CheckRestDone()
        {
            foreach (var p in _occupants)
            {
                if (!_restChosen.Contains(p)) return;
            }

            // 전원이 골랐으면 결과를 잠깐 보여주고 끝
            SetTimer(_restTimer, Settings.RestFinishDelay, FinishRest);
        }

        void FinishRest()
        {
            if (!_bResting) return;
            MarkCleared();
        }
    }
}
