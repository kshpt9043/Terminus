// 이벤트 방 / 퀘스트 방 (언리얼 ADungeonArea::BeginEvent / HandleEventChoice / ApplyEventOption / CheckEventDone / FinishEvent
// + BeginQuestRoom / HandleQuestChoice / GiveGuardianWinBonus)
// 10-24 기획 엑셀: 방 하나에 이벤트 하나(DT_Event), 선택지 3개(DT_EventChoice) 중 하나를 고름
//  - 공용 이벤트 + 지금 테마 이벤트 중 가중치(AppearanceWeight)로 하나. 한 런 한 번 이벤트는 다시 안 나옴
//  - 멀티: 같은 이벤트를 보고 각자 고름 (받을 스킬 / 유물은 사람마다 따로 미리 정함). 전원이 고르면 끝
//  - 대가는 툴팁 대괄호 안 문구를 따름 (CostType / CostValue 칸과 문구가 다른 행이 있어서). 문구가 없으면 칸 값
//  - '쉬는 가디언' 을 누가 고르면 전원이 고른 뒤 그 방 전원이 약해진 가디언과 싸움
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonArea
    {
        // 이벤트 (서버만). 방 하나에 이벤트 하나(DT_Event), 선택지는 사람마다 따로 풀어서 보냄
        string _currentEvent = "";
        readonly Dictionary<DungeonPlayer, List<EventOption>> _eventOffers = new Dictionary<DungeonPlayer, List<EventOption>>();
        readonly HashSet<DungeonPlayer> _eventChosen = new HashSet<DungeonPlayer>();
        bool _bInEvent;
        readonly TimerHandle _eventTimer = new TimerHandle();

        // '쉬는 가디언' 을 고른 사람이 있으면 전원이 고른 뒤 그 체력 비율로 가디언 전투 (0 = 없음)
        float _eventGuardianHealth;

        // 쉬는 가디언을 이기면 줄 추가 보상 (고른 사람만). 방이 클리어될 때 줌 (언리얼 TMap -> 넣은 순서 유지)
        sealed class GuardianWinBonus
        {
            public DungeonPlayer Player;
            public int Currency;
            public bool bThemeRelic;
        }
        readonly List<GuardianWinBonus> _eventGuardianBonus = new List<GuardianWinBonus>();

        // 퀘스트 방 (테마 첫 층의 첫 칸): 각자 3개 중 1개 수락 또는 거절. 화면은 이벤트 화면을 같이 씀
        readonly Dictionary<DungeonPlayer, List<string>> _questOffers = new Dictionary<DungeonPlayer, List<string>>();
        bool _bInQuest;

        public bool IsInEvent => _bInEvent;
        public bool IsInQuest => _bInQuest;

        // 이번 방의 이벤트 (DT_Event 행). 없으면 ""
        public string CurrentEvent => _currentEvent;

        // 전원이 고른 뒤 시작할 쉬는 가디언 체력 비율 (0 = 없음)
        public float PendingGuardianHealth => _eventGuardianHealth;

        public bool HasChosenEvent(DungeonPlayer p) => p != null && _eventChosen.Contains(p);

        /// <summary>이 사람에게 보여 준 선택지 (사본). 없으면 빈 목록</summary>
        public List<EventOption> GetEventOptions(DungeonPlayer p)
        {
            var result = new List<EventOption>();
            if (p != null && _eventOffers.TryGetValue(p, out var offers))
            {
                foreach (var o in offers) result.Add(AreaEventRules.CloneOption(o));
            }
            return result;
        }

        /// <summary>퀘스트 방에서 이 사람에게 보여 준 퀘스트 (DT_Quest 행). 마지막 카드(거절)는 빠짐</summary>
        public List<string> GetQuestOffers(DungeonPlayer p)
        {
            return p != null && _questOffers.TryGetValue(p, out var offers) ? new List<string>(offers) : new List<string>();
        }

        // =====================================================================
        // 퀘스트 방 (사용자 결정 2026-10-26: 3개 중 1개, 1개는 내 세력, 사람마다 따로, 그 테마 동안)
        // =====================================================================

        void BeginQuestRoom()
        {
            _bInQuest = true;
            _questOffers.Clear();
            _eventChosen.Clear();

            int floor = CurrentFloor;
            string themeName = CurrentTheme != null ? CurrentTheme.DisplayName ?? "" : "";

            foreach (var p in _occupants)
            {
                var offers = Quests.MakeOffers(p.Run, floor, themeName, Rng);
                _questOffers[p] = offers;

                // 카드: 퀘스트들 + 맨 끝에 거절
                var options = new List<EventOption>();
                foreach (var row in offers)
                {
                    var quest = GameData.FindQuestRow(row);
                    options.Add(new EventOption
                    {
                        ChoiceRow = row,
                        Outcome = EventOutcomeType.Leave,
                        Title = quest != null ? quest.QuestName : row,
                        Description = Quests.DescribeOffer(row),
                    });
                }
                options.Add(new EventOption
                {
                    Outcome = EventOutcomeType.Leave,
                    Title = "의뢰를 받지 않는다",
                    Description = "이번 테마에서는 퀘스트 없이 진행합니다.",
                });

                Notifier?.ShowEvent(p, "세력의 의뢰",
                    $"{(string.IsNullOrEmpty(themeName) ? "새 테마" : themeName)}에 들어서자 각 세력의 의뢰서가 도착했습니다. 하나를 수락하면 이 테마가 끝날 때까지 진행됩니다.",
                    options);
            }

            CheckEventDone();
        }

        void HandleQuestChoice(DungeonPlayer chooser, int index)
        {
            if (chooser == null || !_occupants.Contains(chooser) || _eventChosen.Contains(chooser)) return;
            if (!_questOffers.TryGetValue(chooser, out var offers)) return;

            string result;
            bool valid = index >= 0 && index < offers.Count;
            if (valid)
            {
                var row = offers[index];
                // 수락: 특수 방을 넣는 퀘스트면 관리자를 거쳐 Game 쪽이 지도에 넣고 다시 퍼뜨림
                Quests.Accept(chooser.Run, row, CurrentFloor, () => Manager?.RequestSpecialRoomInjection(chooser));
                var quest = GameData.FindQuestRow(row);
                result = $"[{(quest != null ? quest.QuestName : row)}] 의뢰를 수락했습니다.\n\n{Quests.DescribeOffer(row)}";
            }
            else
            {
                result = "의뢰를 받지 않고 길을 나섭니다.";
            }

            _eventChosen.Add(chooser);
            Notifier?.EventResult(chooser, result);
            Log($"퀘스트: {chooser.Name} -> {(valid ? offers[index] : "거절")}");

            CheckEventDone();
        }

        // =====================================================================
        // 이벤트
        // =====================================================================

        void BeginEvent()
        {
            _bInEvent = true;
            _eventOffers.Clear();
            _eventChosen.Clear();
            _eventGuardianHealth = 0f;
            _eventGuardianBonus.Clear();

            // 이벤트 고르기 / 선택지 수치 / 무작위 유물을 (런 시드, 층, 방 번호)로 고정
            // -> 나갔다가 이어하기로 다시 들어와도 같은 이벤트 (언리얼은 FMath::RandInit 후 끝에서 난수를 다시 섞음)
            var session = Manager?.Session;
            int floor = CurrentFloor;
            var seeded = session != null ? new Random(session.GetRoomSeed(floor, _room.RoomId)) : Rng;

            string themeName = CurrentTheme != null ? CurrentTheme.DisplayName ?? "" : "";
            _currentEvent = AreaEventRules.PickEventRow(floor, themeName, session?.SeenEvents, seeded, Settings.DebugForcedEvent);

            var ev = Ids.IsNone(_currentEvent) ? null : GameData.FindEventRow(_currentEvent);
            if (ev != null)
            {
                session?.AddSeenEvent(_currentEvent);
                Log($"이벤트: {ev.EventName_KR} ({_currentEvent})");
            }

            string title = ev != null ? ev.EventName_KR : "이벤트";
            string situation = ev != null ? ev.SituationDesc_KR : Settings.EventFallbackText;

            foreach (var p in _occupants)
            {
                var options = new List<EventOption>();
                if (ev != null)
                {
                    var seen = new List<string>();
                    foreach (var choiceRow in new[] { ev.Choice1_ID, ev.Choice2_ID, ev.Choice3_ID })
                    {
                        if (Ids.IsNone(choiceRow) || GameData.FindEventChoiceRow(choiceRow) == null) continue;
                        var option = AreaEventRules.MakeEventOption(p, choiceRow, floor, seeded);
                        if (!Ids.IsNone(option.RelicRow)) seen.Add(option.RelicRow);
                        options.Add(option);
                    }
                    p.Run?.MarkRelicsSeen(seen);   // 카드에 보인 유물은 이번 런에 다시 안 나옴 (사용자 결정 10-06)
                }
                _eventOffers[p] = options;

                // 고를 수 있는 게 없으면 고른 걸로 (화면엔 '아무 일도 일어나지 않았습니다')
                if (!options.Exists(o => o.bAvailable))
                {
                    _eventChosen.Add(p);
                }

                Notifier?.ShowEvent(p, title, situation, CloneOptions(options));
            }

            CheckEventDone();
        }

        static List<EventOption> CloneOptions(List<EventOption> options) => options.ConvertAll(AreaEventRules.CloneOption);

        /// <summary>
        /// [이벤트] 선택지 고르기 (PC 의 Server_ChooseEvent). 한 사람 한 번, 전원이 고르면 방이 끝남.
        /// replaceSlot = 강화 칸이 꽉 찼을 때 바꿀 칸, relicRow = 유물 칸이 꽉 찼을 때 버릴 내 유물. 퀘스트 방이면 index 만 봄
        /// </summary>
        public void HandleEventChoice(DungeonPlayer chooser, int index, int replaceSlot, string relicRow)
        {
            if (_bInQuest)
            {
                HandleQuestChoice(chooser, index);
                return;
            }
            if (!_bInEvent || chooser == null) return;
            if (!_occupants.Contains(chooser) || _eventChosen.Contains(chooser)) return;
            if (!_eventOffers.TryGetValue(chooser, out var offers)) return;

            bool valid = index >= 0 && index < offers.Count;
            if (!valid || !ApplyEventOption(chooser, offers[index], replaceSlot, relicRow ?? "", out string result))
            {
                // 잘못 골랐거나(칸 / 유물 / 재화가 그새 바뀜) 적용 실패: 선택지를 지금 상태로 다시 풀어서 보내 다시 고르게
                Log($"이벤트: {chooser.Name} 의 선택({index}) 적용 실패. 다시 고르게 함");
                var fresh = new List<EventOption>();
                foreach (var old in offers)
                {
                    var again = AreaEventRules.MakeEventOption(chooser, old.ChoiceRow, CurrentFloor, Rng);
                    // 미리 보여 준 스킬 / 유물 / 재화는 그대로 (다시 굴려서 바뀌지 않게)
                    again.SkillRow = old.SkillRow;
                    again.RelicRow = old.RelicRow;
                    again.Amount = old.Amount;
                    fresh.Add(again);
                }
                _eventOffers[chooser] = fresh;

                var ev = GameData.FindEventRow(_currentEvent);
                Notifier?.ShowEvent(chooser, ev != null ? ev.EventName_KR : "", ev != null ? ev.SituationDesc_KR : Settings.EventFallbackText, CloneOptions(fresh));
                return;
            }

            _eventChosen.Add(chooser);
            Notifier?.EventResult(chooser, result);

            string summary = offers[index].Title;
            if (_occupants.Count > 1)
            {
                Chat($"[이벤트] {chooser.Name} 님: {summary}");
            }

            Log($"이벤트: {chooser.Name} -> {offers[index].ChoiceRow} / {result.Replace("\n", " / ")}");

            CheckEventDone();
        }

        bool ApplyEventOption(DungeonPlayer player, EventOption option, int replaceSlot, string relicRow, out string outResult)
        {
            outResult = "";
            var ps = player?.Run;
            if (ps == null || option == null || !option.bAvailable) return false;

            var choice = GameData.FindEventChoiceRow(option.ChoiceRow);
            var stats = player.Battler;
            if (choice == null) return false;

            var run = ps.RunState;

            // ---- 먼저 다 되는지 확인 (대가만 치르고 못 받는 일이 없게)
            if (option.CostHealth > 0 && (stats == null || stats.State.Health <= option.CostHealth)) return false;
            if (option.CostCurrency > 0 && run.Currency < option.CostCurrency) return false;
            if (option.GivesSkill() && run.EnhanceSkills.Count >= ps.GetEnhanceSlotCount()
                && !(replaceSlot >= 0 && replaceSlot < run.EnhanceSkills.Count)) return false;

            bool bRelicFull = run.Relics.Count >= ps.GetRelicCapacity();
            if (option.GivesRelic() && bRelicFull && !AreaEventRules.LosableRelics(ps).Contains(relicRow)) return false;

            bool bHigher = option.Outcome == EventOutcomeType.SwapRelicHigherTierRandom;
            bool bSwap = bHigher || option.Outcome == EventOutcomeType.SwapRelicSameTierRandom;
            var swappable = bSwap ? AreaEventRules.SwappableRelics(ps, bHigher) : new List<string>();
            if (bSwap && swappable.Count == 0) return false;

            var lines = new List<string>();
            var rng = Rng;

            // ---- 대가
            if (option.CostCurrency > 0)
            {
                ps.AddCurrency(-option.CostCurrency);
                lines.Add($"던전 재화 -{option.CostCurrency}");
            }
            if (option.CostHealth > 0 && stats != null)
            {
                stats.ApplyDamage(option.CostHealth);
                lines.Add($"체력 -{option.CostHealth}");
            }
            if (option.CostMaxHealth > 0)
            {
                ps.ApplyPermanentStat(0, -option.CostMaxHealth);
                lines.Add($"최대 체력 -{option.CostMaxHealth}");
            }
            for (int i = 0; i < option.CostSkills; ++i)
            {
                var equipped = AreaEventRules.EquippedSkills(ps, false, false);
                if (equipped.Count == 0) break;
                var lost = AreaEventRules.PickRandom(equipped, rng);
                ps.UnequipEnhanceSkill(lost);
                lines.Add($"스킬 소실: {GameData.GetSkillName(lost)}");
            }
            for (int i = 0; i < option.CostRelics; ++i)
            {
                var losable = AreaEventRules.LosableRelics(ps);
                if (losable.Count == 0) break;
                var lost = AreaEventRules.PickRandom(losable, rng);
                ps.RemoveRelic(lost);
                lines.Add($"유물 소실: {GameData.GetRelicName(lost)}");
            }
            if (!Ids.IsNone(option.CostRelic) && ps.GainRelic(option.CostRelic))
            {
                lines.Add($"저주 유물: {GameData.GetRelicName(option.CostRelic)}");
            }

            // ---- 결과
            switch (option.Outcome)
            {
                case EventOutcomeType.GiveOwnClassSkill:
                case EventOutcomeType.GiveCommonSkill:
                case EventOutcomeType.GiveOtherClassSkill:
                case EventOutcomeType.GiveEventSkill:
                case EventOutcomeType.GiveHigherTierSkill:
                    if (ps.EquipEnhanceSkill(option.SkillRow, replaceSlot))
                    {
                        lines.Add($"스킬 획득: {GameData.GetSkillName(option.SkillRow)}");
                    }
                    break;

                case EventOutcomeType.UpgradeRandomSkill:
                    for (int i = 0; i < option.Amount; ++i)
                    {
                        var candidates = AreaEventRules.EquippedSkills(ps, (choice.OutcomeTargetPool ?? "").Contains("Attack"), true);
                        if (candidates.Count == 0) break;
                        var skill = AreaEventRules.PickRandom(candidates, rng);
                        if (ps.UpgradePickupSkill(skill))
                        {
                            lines.Add($"스킬 강화: {GameData.GetSkillName(skill)} +{ps.GetPickupSkillLevel(skill)}");
                        }
                    }
                    break;

                case EventOutcomeType.GiveTempStatRelic:
                case EventOutcomeType.GivePermStatRelic:
                case EventOutcomeType.GiveThemeQuestRelic:
                case EventOutcomeType.GiveHigherTierRelic:
                    if (bRelicFull && ps.RemoveRelic(relicRow))
                    {
                        lines.Add($"유물 버림: {GameData.GetRelicName(relicRow)}");
                    }
                    if (ps.GainRelic(option.RelicRow))
                    {
                        int battles = ps.GetTempRelicBattlesLeft(option.RelicRow);
                        lines.Add(battles > 0
                            ? $"유물 획득: {GameData.GetRelicName(option.RelicRow)} (전투 {battles}번)"
                            : $"유물 획득: {GameData.GetRelicName(option.RelicRow)}");
                    }
                    break;

                case EventOutcomeType.SwapRelicSameTierRandom:
                case EventOutcomeType.SwapRelicHigherTierRandom:
                {
                    var oldRelic = AreaEventRules.PickRandom(swappable, rng);
                    var newRelic = AreaEventRules.PickRandom(AreaEventRules.SwapPool(ps, oldRelic, bHigher), rng);
                    if (ps.SwapRelic(oldRelic, newRelic))
                    {
                        lines.Add($"유물 변화: {GameData.GetRelicName(oldRelic)} → {GameData.GetRelicName(newRelic)}");
                    }
                    if (option.Amount > 0)
                    {
                        ps.AddCurrency(option.Amount);
                        lines.Add($"던전 재화 +{option.Amount}");
                    }
                    break;
                }

                case EventOutcomeType.GiveCurrencyRange:
                    ps.AddCurrency(option.Amount);
                    lines.Add($"던전 재화 +{option.Amount}");
                    break;

                case EventOutcomeType.HealHP:
                    if (stats != null)
                    {
                        stats.Heal(option.Amount);
                        lines.Add($"체력 +{option.Amount}");
                    }
                    break;

                case EventOutcomeType.StartWeakGuardianBattle:
                    _eventGuardianHealth = Math.Max(_eventGuardianHealth, option.Amount / 100f);
                    lines.Add("모두가 고르면 약해진 가디언과 전투합니다.");
                    if (option.BonusWinCurrency > 0 || option.bBonusWinRelic)
                    {
                        var bonus = _eventGuardianBonus.Find(b => b.Player == player);
                        if (bonus == null)
                        {
                            bonus = new GuardianWinBonus { Player = player };
                            _eventGuardianBonus.Add(bonus);
                        }
                        bonus.Currency = option.BonusWinCurrency;
                        bonus.bThemeRelic = option.bBonusWinRelic;
                    }
                    break;
            }

            // 툴팁에만 적힌 추가 결과
            if (option.BonusHeal > 0 && stats != null)
            {
                stats.Heal(option.BonusHeal);
                lines.Add($"체력 +{option.BonusHeal}");
            }
            for (int i = 0; i < option.BonusUpgrades; ++i)
            {
                var candidates = AreaEventRules.EquippedSkills(ps, false, true);
                if (candidates.Count == 0) break;
                var skill = AreaEventRules.PickRandom(candidates, rng);
                if (ps.UpgradePickupSkill(skill))
                {
                    lines.Add($"스킬 강화: {GameData.GetSkillName(skill)} +{ps.GetPickupSkillLevel(skill)}");
                }
            }

            // 기획 결과 문구 + 실제로 바뀐 것
            var story = choice.ResultText_KR ?? "";
            outResult = story.Length == 0
                ? string.Join("\n", lines)
                : story + (lines.Count > 0 ? "\n\n" + string.Join("\n", lines) : "");
            return true;
        }

        void CheckEventDone()
        {
            foreach (var p in _occupants)
            {
                if (!_eventChosen.Contains(p)) return;
            }

            // 전원이 골랐으면 결과를 잠깐 보여주고 끝 (전투가 있으면 조금 더 길게)
            SetTimer(_eventTimer, _eventGuardianHealth > 0f ? Settings.EventGuardianFinishDelay : Settings.EventFinishDelay, FinishEvent);
        }

        void GiveGuardianWinBonus()
        {
            // 이벤트 '쉬는 가디언' 을 이김: 고른 사람에게 툴팁의 테마 유물 + 던전 재화 (일반 가디언 보상에 더해서)
            foreach (var entry in _eventGuardianBonus)
            {
                var p = entry.Player;
                if (p == null || p.Run == null || !_occupants.Contains(p)) continue;

                var lines = new List<string>();
                if (entry.Currency > 0)
                {
                    p.Run.AddCurrency(entry.Currency);
                    lines.Add($"던전 재화 +{entry.Currency}");
                }
                if (entry.bThemeRelic)
                {
                    var pool = AreaEventRules.ThemeRelicPool(p.Run, AreaEventRules.RelicTierOf(AreaEventRules.SkillTierOf(CurrentFloor)), "");
                    if (pool.Count > 0)
                    {
                        var relic = AreaEventRules.PickRandom(pool, Rng);
                        p.Run.MarkRelicsSeen(new[] { relic });
                        lines.Add(p.Run.GainRelic(relic)
                            ? $"유물 획득: {GameData.GetRelicName(relic)}"
                            : $"유물 [{GameData.GetRelicName(relic)}] 을(를) 찾았지만 유물 칸이 가득 차 가져가지 못했습니다.");
                    }
                }
                if (lines.Count == 0) continue;

                var text = $"[이벤트] {p.Name} 님 가디언 기습 성공: {string.Join(", ", lines)}";
                Chat(text);
                Log(text);
            }
            _eventGuardianBonus.Clear();
        }

        void FinishEvent()
        {
            if (_bInQuest)
            {
                MarkCleared();
                return;
            }
            if (!_bInEvent) return;

            if (_eventGuardianHealth <= 0f || Combat == null)
            {
                MarkCleared();
                return;
            }

            // 쉬는 가디언: 이벤트 화면을 닫고 이 방 전원이 가디언과 싸움. 끝나면 전투 쪽이 가디언 보상 후 MarkCleared
            ClearTimer(_eventTimer);
            var players = new List<DungeonPlayer>();
            foreach (var p in _occupants)
            {
                players.Add(p);
                Notifier?.CloseEvent(p);
            }
            _bInEvent = false;
            _eventOffers.Clear();
            _eventChosen.Clear();

            var guardianRoom = _room.Clone();
            guardianRoom.Type = RoomType.GUARDIAN;
            Combat.SetNextSpawnHealthScale(_eventGuardianHealth);
            _eventGuardianHealth = 0f;

            Log("이벤트: 쉬는 가디언 전투 시작");
            Combat.StartCombat(guardianRoom, players, CurrentTheme);
        }
    }
}
