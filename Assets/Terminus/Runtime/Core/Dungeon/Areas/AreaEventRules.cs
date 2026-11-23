// 이벤트 방 / 상점이 쓰는 후보 풀과 선택지 만들기 (언리얼 DungeonArea.cpp 의 익명 namespace 함수들 + MakeEventOption)
// 10-24 기획 엑셀: 방 하나에 이벤트 하나(DT_Event), 선택지 3개(DT_EventChoice) 중 하나를 고름
//  - 대가는 툴팁 대괄호 안 문구를 따름 (CostType / CostValue 칸과 문구가 다른 행이 있어서). 문구가 없으면 칸 값
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Dungeon
{
    public static class AreaEventRules
    {
        // 3전투 디버프 유물 (Enum_Reference: RLC_Event_Temp_006)
        public const string DebuffRelicRow = "RLC_Event_Temp_006";

        // 쉬는 가디언 체력 % (데이터에 없을 때)
        public const int DefaultWeakGuardianHealthPercent = 70;

        // ------------------------------------------------------------ 계층

        // 1~2층 표층, 3~4층 중층, 5층부터 심층
        public static SkillTier SkillTierOf(int floor)
        {
            return floor >= 5 ? SkillTier.Deep : (floor >= 3 ? SkillTier.Mid : SkillTier.Surface);
        }

        public static SkillTier NextSkillTier(SkillTier tier)
        {
            return tier == SkillTier.Surface ? SkillTier.Mid : SkillTier.Deep;
        }

        public static RelicTier RelicTierOf(SkillTier tier)
        {
            return tier == SkillTier.Deep ? RelicTier.Deep : (tier == SkillTier.Mid ? RelicTier.Mid : RelicTier.Surface);
        }

        public static RelicTier NextRelicTier(RelicTier tier)
        {
            return tier == RelicTier.Surface ? RelicTier.Mid : RelicTier.Deep;
        }

        public static EventAppearTier AppearTierOf(int floor)
        {
            var tier = SkillTierOf(floor);
            return tier == SkillTier.Deep ? EventAppearTier.Deep : (tier == SkillTier.Mid ? EventAppearTier.Mid : EventAppearTier.Surface);
        }

        static bool IsSwappableTier(RelicTier tier) => tier != RelicTier.Basic && tier != RelicTier.Upgrade;

        internal static T PickRandom<T>(IReadOnlyList<T> list, Random rng) => list[rng.Next(0, list.Count)];

        // ------------------------------------------------------------ 유물 풀

        // 이벤트가 줄 수 있는 유물인가: 공용 / 내 직업 / 이벤트 / 퀘스트 유물, 안 가진 것, 이번 런에 안 나온 것
        static bool IsGivableRelic(RunPlayer p, string row, RelicRow relic)
        {
            if (!IsSwappableTier(relic.RelicTier) || relic.OwnerClass == SkillOwner.Monster) return false;
            bool givenOnly = relic.OwnerClass == SkillOwner.Event || relic.OwnerClass == SkillOwner.Quest;
            if (!givenOnly && !RunPlayer.CanClassHoldRelic(p.CharacterClass, relic)) return false;
            return !p.RunState.Relics.Contains(row) && !p.HasSeenRelic(row);
        }

        // 무작위 유물 후보 (tier 계층의 보상용 유물: 공용 / 내 직업. 이벤트 / 퀘스트 전용은 뺌)
        public static List<string> RelicPool(RunPlayer p, RelicTier tier)
        {
            var pool = new List<string>();
            if (p == null) return pool;

            foreach (var relic in GameData.Relics.Rows)
            {
                if (relic.RelicTier != tier) continue;
                if (relic.OwnerClass == SkillOwner.Event || relic.OwnerClass == SkillOwner.Quest) continue;
                if (IsGivableRelic(p, relic.RowName, relic)) pool.Add(relic.RowName);
            }
            return pool;
        }

        // 테마 / 퀘스트 유물 후보. 유물 데이터에 테마 칸이 없어서 '지금 계층의 퀘스트 유물' 로 봄
        // 정령계 'KingCore' 풀은 이름에 "코어" 가 있는 것(정령왕 코어), 'Lesser' 는 나머지
        public static List<string> ThemeRelicPool(RunPlayer p, RelicTier tier, string poolName)
        {
            var pool = new List<string>();
            if (p == null) return pool;
            poolName = poolName ?? "";

            bool kingCore = poolName.Contains("KingCore");
            bool lesser = poolName.Contains("Lesser");
            foreach (var relic in GameData.Relics.Rows)
            {
                if (relic.RelicTier != tier || relic.OwnerClass != SkillOwner.Quest) continue;
                bool core = (relic.RelicName ?? "").Contains("코어");
                if ((kingCore && !core) || (lesser && core)) continue;
                if (IsGivableRelic(p, relic.RowName, relic)) pool.Add(relic.RowName);
            }
            return pool;
        }

        // oldRelic 을 바꿀 수 있는 유물: tier 계층, 공용이거나 내 직업, 이번 런에 안 나왔던 것
        public static List<string> SwapPool(RunPlayer p, string oldRelic, bool bHigherTier)
        {
            var old = GameData.FindRelicRow(oldRelic);
            if (p == null || old == null || !IsSwappableTier(old.RelicTier)) return new List<string>();
            if (old.OwnerClass == SkillOwner.Event || old.OwnerClass == SkillOwner.Quest) return new List<string>();   // 이벤트 / 퀘스트 유물은 계층이 없는 셈이라 안 바꿈
            return RelicPool(p, bHigherTier ? NextRelicTier(old.RelicTier) : old.RelicTier);
        }

        // 바꿀 수 있는 내 유물 (기본 / 업그레이드 / 이벤트 유물 제외, 바꿀 후보가 있는 것만)
        public static List<string> SwappableRelics(RunPlayer p, bool bHigherTier)
        {
            var result = new List<string>();
            if (p == null) return result;

            foreach (var owned in p.RunState.Relics)
            {
                if (SwapPool(p, owned, bHigherTier).Count > 0) result.Add(owned);
            }
            return result;
        }

        // 대가로 잃을 수 있는 내 유물 (기본 / 업그레이드 유물은 안 잃음)
        public static List<string> LosableRelics(RunPlayer p)
        {
            var result = new List<string>();
            if (p == null) return result;

            foreach (var owned in p.RunState.Relics)
            {
                var relic = GameData.FindRelicRow(owned);
                if (relic != null && IsSwappableTier(relic.RelicTier)) result.Add(owned);
            }
            return result;
        }

        // ------------------------------------------------------------ 스킬 풀

        // 장착한 픽업 스킬 (강화 칸)
        public static List<string> EquippedSkills(RunPlayer p, bool bAttackOnly, bool bUpgradableOnly)
        {
            var result = new List<string>();
            if (p == null) return result;

            foreach (var row in p.RunState.EnhanceSkills)
            {
                var skill = GameData.FindSkillRow(row);
                if (skill == null) continue;
                if (bAttackOnly && skill.SkillType != SkillType.Attack) continue;
                if (bUpgradableOnly && !p.CanUpgradePickupSkill(row)) continue;
                if (!result.Contains(row)) result.Add(row);   // AddUnique
            }
            return result;
        }

        // 스킬 풀 (OutcomeTargetPool 이름으로 조건을 정함)
        //  Pool_Skill_OwnClass_* / OtherClass_* / Shared_Common_* / Event_All / Event_Offensive / Event_HighTier_*
        //  CurrentTier = 지금 계층, NextTier = 한 계층 위. 이미 장착한 스킬은 뺌
        public static List<string> SkillPool(RunPlayer p, string poolName, SkillTier currentTier)
        {
            var pool = new List<string>();
            if (p == null) return pool;
            poolName = poolName ?? "";

            var myOwner = CharacterNames.ToSkillOwner(p.CharacterClass);
            var equipped = p.RunState.EnhanceSkills;
            bool bEvent = poolName.Contains("_Event_");
            bool bNextTier = poolName.Contains("NextTier");
            var wantTier = bNextTier ? NextSkillTier(currentTier) : currentTier;

            var eventAll = new List<string>();
            var otherTier = new List<string>();
            foreach (var skill in GameData.Skills.Rows)
            {
                var row = skill.RowName;
                if (!ProfileService.IsOwnableSkill(row) || equipped.Contains(row)) continue;

                if (bEvent)
                {
                    if (skill.SkillCategory != SkillCategory.Event) continue;
                    if (poolName.Contains("Offensive") && skill.SkillType != SkillType.Attack) continue;
                    eventAll.Add(row);
                    continue;
                }

                if (skill.SkillEnergyCost <= 0) continue;   // 픽업 스킬 = 스킬 에너지를 쓰는 스킬
                if (poolName.Contains("OwnClass"))
                {
                    if (skill.SkillCategory != SkillCategory.Personal || skill.OwnerClass != myOwner) continue;
                }
                else if (poolName.Contains("OtherClass"))
                {
                    if (skill.SkillCategory != SkillCategory.Personal || skill.OwnerClass == myOwner || skill.OwnerClass == SkillOwner.Shared) continue;
                }
                else if (poolName.Contains("Common"))
                {
                    if (skill.SkillCategory != SkillCategory.Common || !ProfileService.IsEquippableSkill(row, p.CharacterClass)) continue;
                }
                else
                {
                    continue;
                }
                (skill.SkillTier == wantTier ? pool : otherTier).Add(row);
            }

            // 그 계층에 맞는 게 없으면 다른 계층에서라도 (공용 픽업 스킬은 10-24 데이터에 중층 1개뿐)
            if (!bEvent && pool.Count == 0)
            {
                pool = otherTier;
            }

            if (bEvent)
            {
                // HighTier: 이벤트 스킬 중 가장 높은 계층 것만
                if (poolName.Contains("HighTier") && eventAll.Count > 0)
                {
                    var highest = SkillTier.Surface;
                    foreach (var row in eventAll)
                    {
                        var t = GameData.FindSkillRow(row).SkillTier;
                        if (t > highest) highest = t;
                    }
                    eventAll.RemoveAll(row => GameData.FindSkillRow(row).SkillTier != highest);
                }
                return eventAll;
            }
            return pool;
        }

        // ------------------------------------------------------------ 툴팁 읽기

        // 툴팁 대괄호 안에서 "체력 4 감소", "던전 재화 30 소모", "스킬 1개", "유물 1개" 같은 수치를 읽음
        public static int FindNumber(string text, string pattern)
        {
            if (string.IsNullOrEmpty(text)) return 0;
            var m = Regex.Match(text, pattern);
            // 언리얼 FCString::Atoi: 숫자가 아니면 0
            return m.Success && int.TryParse(m.Groups[1].Value, out int v) ? v : 0;
        }

        // 대괄호 안 문구. 없으면 ""
        static string BracketOf(string tooltip)
        {
            tooltip = tooltip ?? "";
            int open = tooltip.IndexOf('[');
            int close = tooltip.IndexOf(']');
            return open >= 0 && close > open ? tooltip.Substring(open + 1, close - open - 1) : "";
        }

        // 대괄호 뒤 문구. ']' 가 없으면 툴팁 전체
        static string AfterBracketOf(string tooltip)
        {
            tooltip = tooltip ?? "";
            int close = tooltip.IndexOf(']');
            return close >= 0 ? tooltip.Substring(close + 1) : tooltip;
        }

        public static void ParseCost(EventChoiceRow choice, EventOption output)
        {
            var bracket = BracketOf(choice.ChoiceTooltip_KR);

            output.CostMaxHealth = FindNumber(bracket, @"최대 체력 (\d+)");
            output.CostHealth = FindNumber(bracket.Replace("최대 체력", ""), @"체력 (\d+) 감소");
            output.CostCurrency = FindNumber(bracket, @"재화 (\d+) 소모");
            output.CostSkills = FindNumber(bracket, @"스킬 (\d+)개");
            output.CostRelics = FindNumber(bracket, @"유물 (\d+)개");

            bool parsed = output.CostMaxHealth + output.CostHealth + output.CostCurrency + output.CostSkills + output.CostRelics > 0;
            if (!parsed)
            {
                switch (choice.CostType)
                {
                    case EventCostType.LoseHP: output.CostHealth = choice.CostValue; break;
                    case EventCostType.LoseMaxHP: output.CostMaxHealth = choice.CostValue; break;
                    case EventCostType.SpendCurrency: output.CostCurrency = choice.CostValue; break;
                    case EventCostType.LoseRandomSkill: output.CostSkills = Math.Max(1, choice.CostValue); break;
                    case EventCostType.LoseRandomRelic:
                    case EventCostType.LoseRandomRelicAndHP: output.CostRelics = 1; break;
                }
            }

            // 3전투 디버프 유물 (Enum_Reference: RLC_Event_Temp_006)
            if (choice.CostType == EventCostType.GainDebuffRelic)
            {
                output.CostRelic = DebuffRelicRow;
            }
        }

        // ------------------------------------------------------------ 이벤트 고르기

        /// <summary>
        /// 공용 이벤트 + 지금 테마 이벤트 중 가중치(AppearanceWeight)로 하나. 한 런 한 번 이벤트는 다시 안 나옴 (언리얼 PickEventRow).
        /// forced 가 표에 있으면 그것 (디버그). 후보가 없으면 ""
        /// </summary>
        public static string PickEventRow(int floor, string themeName, IReadOnlyCollection<string> seenEvents, Random rng, string forced = "")
        {
            if (!Ids.IsNone(forced) && GameData.FindEventRow(forced) != null)
            {
                return forced;
            }

            var wantTier = AppearTierOf(floor);
            themeName = themeName ?? "";

            var candidates = new List<KeyValuePair<string, int>>();
            int total = 0;
            foreach (var ev in GameData.Events.Rows)
            {
                if (ev.AppearanceWeight <= 0) continue;

                // 공용 이벤트 또는 지금 테마 이벤트, 계층은 Any 이거나 지금 계층
                bool bCommon = string.IsNullOrEmpty(ev.EventScope) || ev.EventScope == "공용";
                if (!bCommon && ev.EventScope != themeName) continue;
                if (ev.AppearTier != EventAppearTier.Any && ev.AppearTier != wantTier) continue;
                if (ev.bOneTimePerRun && seenEvents != null && Contains(seenEvents, ev.RowName)) continue;

                candidates.Add(new KeyValuePair<string, int>(ev.RowName, ev.AppearanceWeight));
                total += ev.AppearanceWeight;
            }
            if (total <= 0) return "";

            // FMath::RandRange(1, Total)
            int roll = rng.Next(1, total + 1);
            foreach (var c in candidates)
            {
                roll -= c.Value;
                if (roll <= 0) return c.Key;
            }
            return candidates[candidates.Count - 1].Key;
        }

        static bool Contains(IReadOnlyCollection<string> list, string value)
        {
            foreach (var s in list)
            {
                if (s == value) return true;
            }
            return false;
        }

        /// <summary>
        /// 선택지 하나를 이 사람 기준으로 풀어 둠 (언리얼 MakeEventOption): 받을 스킬 / 유물 / 재화를 미리 굴리고,
        /// 대가를 치를 수 있는지 보고 못 고르면 이유를 적음
        /// </summary>
        public static EventOption MakeEventOption(DungeonPlayer player, string choiceRow, int floor, Random rng)
        {
            var option = new EventOption();
            var choice = GameData.FindEventChoiceRow(choiceRow);
            var p = player?.Run;
            if (p == null || choice == null)
            {
                option.bAvailable = false;
                return option;
            }

            option.ChoiceRow = choiceRow;
            option.Outcome = choice.OutcomeType;
            option.Title = choice.ChoiceTitle_KR;
            option.Description = choice.ChoiceTooltip_KR;
            ParseCost(choice, option);

            var tier = SkillTierOf(floor);
            var pool = choice.OutcomeTargetPool ?? "";
            int min = Math.Min(choice.OutcomeValueMin, choice.OutcomeValueMax);
            int max = Math.Max(choice.OutcomeValueMin, choice.OutcomeValueMax);

            void Unavailable(string reason)
            {
                option.bAvailable = false;
                option.UnavailableReason = reason;
            }

            // ---- 결과 미리 정하기
            switch (choice.OutcomeType)
            {
                case EventOutcomeType.GiveOwnClassSkill:
                case EventOutcomeType.GiveCommonSkill:
                case EventOutcomeType.GiveOtherClassSkill:
                case EventOutcomeType.GiveEventSkill:
                case EventOutcomeType.GiveHigherTierSkill:
                {
                    var skills = SkillPool(p, pool, tier);
                    if (skills.Count > 0) option.SkillRow = PickRandom(skills, rng);
                    else Unavailable("받을 수 있는 스킬이 없습니다.");
                    break;
                }

                case EventOutcomeType.UpgradeRandomSkill:
                    option.Amount = Math.Max(1, min);
                    if (EquippedSkills(p, pool.Contains("Attack"), true).Count == 0) Unavailable("강화할 스킬이 없습니다.");
                    break;

                case EventOutcomeType.GiveTempStatRelic:
                case EventOutcomeType.GivePermStatRelic:
                    option.RelicRow = choice.OutcomeTargetPool;
                    if (GameData.FindRelicRow(option.RelicRow) == null) Unavailable("유물 데이터가 없습니다.");
                    else if (p.RunState.Relics.Contains(option.RelicRow)) Unavailable("이미 가진 유물입니다.");
                    break;

                case EventOutcomeType.GiveThemeQuestRelic:
                case EventOutcomeType.GiveHigherTierRelic:
                {
                    var current = RelicTierOf(tier);
                    List<string> relics;
                    if (pool.Contains("Theme")) relics = ThemeRelicPool(p, current, pool);
                    else if (pool.Contains("DeepTier")) relics = RelicPool(p, RelicTier.Deep);
                    else if (pool.Contains("MidTier")) relics = RelicPool(p, RelicTier.Mid);
                    else relics = RelicPool(p, NextRelicTier(current));
                    if (relics.Count > 0) option.RelicRow = PickRandom(relics, rng);
                    else Unavailable("받을 수 있는 유물이 없습니다.");
                    break;
                }

                case EventOutcomeType.SwapRelicSameTierRandom:
                case EventOutcomeType.SwapRelicHigherTierRandom:
                    // 바뀌는 유물이 곧 대가 (LoseRandomRelic). 따로 또 잃지 않음
                    option.CostRelics = 0;
                    if (SwappableRelics(p, choice.OutcomeType == EventOutcomeType.SwapRelicHigherTierRandom).Count == 0)
                    {
                        Unavailable("바꿀 수 있는 유물이 없습니다.");
                    }
                    // PlusGold: 툴팁의 "던전 재화 20" 만큼 더
                    if (pool.Contains("PlusGold"))
                    {
                        // 언리얼: Tooltip.Mid(Close + 1). ']' 가 없으면 Close = -1 -> 툴팁 전체
                        option.Amount = FindNumber(AfterBracketOf(choice.ChoiceTooltip_KR), @"재화 (\d+)");
                    }
                    break;

                case EventOutcomeType.GiveCurrencyRange:
                    option.Amount = rng.Next(min, max + 1);   // FMath::RandRange(Min, Max)
                    break;

                case EventOutcomeType.HealHP:
                    option.Amount = max;
                    break;

                case EventOutcomeType.StartWeakGuardianBattle:
                    option.Amount = min > 0 ? min : DefaultWeakGuardianHealthPercent;   // 체력 %
                    break;
            }

            // ---- 툴팁에만 적힌 추가 결과 (대괄호 뒤 문구). 결과 칸이 하나뿐이라 데이터로는 안 들어옴
            {
                var after = AfterBracketOf(choice.ChoiceTooltip_KR);

                if (choice.OutcomeType != EventOutcomeType.HealHP)
                {
                    option.BonusHeal = FindNumber(after, @"체력을 (\d+) 회복");
                }
                if (choice.OutcomeType != EventOutcomeType.UpgradeRandomSkill && after.Contains("+1강 강화"))
                {
                    option.BonusUpgrades = 1;
                }
                if (choice.OutcomeType == EventOutcomeType.StartWeakGuardianBattle && after.Contains("승리 시"))
                {
                    option.BonusWinCurrency = FindNumber(after, @"재화 (\d+) 획득");
                    option.bBonusWinRelic = after.Contains("유물");
                }
            }

            // ---- 대가를 치를 수 있나
            var stats = player.Battler;
            int health = stats != null ? stats.State.Health : 0;
            int maxHealth = stats != null ? stats.Stats.MaxHealth : 0;

            if (option.bAvailable && option.CostHealth > 0 && health <= option.CostHealth) Unavailable("체력이 부족합니다.");
            if (option.bAvailable && option.CostMaxHealth > 0 && maxHealth <= option.CostMaxHealth) Unavailable("최대 체력이 부족합니다.");
            if (option.bAvailable && option.CostCurrency > 0 && p.RunState.Currency < option.CostCurrency) Unavailable("던전 재화가 부족합니다.");
            if (option.bAvailable && option.CostSkills > 0 && EquippedSkills(p, false, false).Count < option.CostSkills) Unavailable("잃을 스킬이 없습니다.");
            if (option.bAvailable && option.CostRelics > 0 && LosableRelics(p).Count < option.CostRelics) Unavailable("잃을 유물이 없습니다.");
            if (option.bAvailable && !Ids.IsNone(option.CostRelic))
            {
                // 디버프 유물은 칸 하나를 차지함. 꽉 찼거나 이미 있으면 못 고름 (버릴 유물을 고르게 하면 대가가 없어지는 셈이라)
                bool bFull = p.RunState.Relics.Count >= p.GetRelicCapacity();
                if (GameData.FindRelicRow(option.CostRelic) == null) Unavailable("유물 데이터가 없습니다.");
                else if (p.RunState.Relics.Contains(option.CostRelic)) Unavailable("이미 같은 저주를 받고 있습니다.");
                else if (bFull) Unavailable("유물 칸이 가득 차 고를 수 없습니다.");
            }

            return option;
        }

        // ------------------------------------------------------------ 상점 진열

        /// <summary>
        /// 상점 진열 (언리얼 MakeShopState). 노션 방 생성 메커니즘(10-09): 같은 계층 + 같은 직업 또는 shared 인 유물 / 픽업 스킬.
        /// 진열에 나온 유물은 '나온 유물' 로 표시됨 (안 사도 이번 런엔 다시 안 나옴)
        /// </summary>
        public static ShopState MakeShopState(RunPlayer p, int floor, DungeonAreaSettings settings, Random rng)
        {
            settings = settings ?? new DungeonAreaSettings();
            var state = new ShopState { UpgradeCostPerLevel = settings.ShopUpgradeCostPerLevel };
            if (p == null) return state;

            var tier = SkillTierOf(floor);
            var relicTier = RelicTierOf(tier);

            // 유물: 상점에 나올 수 있고(가격 있음, 기본 / 업그레이드 아님) 지금 계층, 이 직업이 가질 수 있는 것, 가진 것 / 이번 런에 나온 것 빼고
            var relics = new List<string>();
            foreach (var relic in GameData.Relics.Rows)
            {
                if (!relic.CanBuyInShop() || relic.RelicTier != relicTier) continue;
                if (!RunPlayer.CanClassHoldRelic(p.CharacterClass, relic)) continue;
                if (p.RunState.Relics.Contains(relic.RowName) || p.HasSeenRelic(relic.RowName)) continue;
                relics.Add(relic.RowName);
            }
            Shuffle(relics, rng);
            Truncate(relics, settings.ShopRelicCount);
            foreach (var row in relics)
            {
                state.RelicOffers.Add(row);
                state.RelicPrices.Add(Math.Max(0, GameData.FindRelicRow(row).BuyPrice_Dungeon));
                state.RelicBought.Add(false);
            }
            p.MarkRelicsSeen(state.RelicOffers);   // 상점에 나온 유물은 안 사도 이번 런엔 다시 안 나옴 (기획 유물 특이사항)

            // 픽업 스킬: 지금 계층, 이 직업이 장착할 수 있는, 스킬 에너지를 쓰는 스킬 (이벤트 전용 제외). 이미 장착한 것 빼고
            var skills = new List<string>();
            var equipped = new List<string>(p.RunState.EnhanceSkills);
            foreach (var skill in GameData.Skills.Rows)
            {
                if (skill.SkillEnergyCost <= 0 || equipped.Contains(skill.RowName)) continue;
                if (skill.SkillTier != tier || skill.SkillCategory == SkillCategory.Event) continue;
                if (!ProfileService.IsEquippableSkill(skill.RowName, p.CharacterClass)) continue;
                skills.Add(skill.RowName);
            }
            Shuffle(skills, rng);
            Truncate(skills, settings.ShopSkillCount);
            foreach (var row in skills)
            {
                state.SkillOffers.Add(row);
                // 10-24 엑셀 스킬 구매가. 가격이 비어 있는(0) 스킬만 임시 고정가
                int dataPrice = GameData.FindSkillRow(row).BuyPrice_Dungeon;
                state.SkillPrices.Add(dataPrice > 0 ? dataPrice : settings.ShopSkillPrice);
                state.SkillBought.Add(false);
            }
            return state;
        }

        // 언리얼 ShopShuffle: 뒤에서부터 FMath::RandRange(0, i) 와 바꿈
        static void Shuffle<T>(List<T> list, Random rng)
        {
            for (int i = list.Count - 1; i > 0; --i)
            {
                int j = rng.Next(0, i + 1);
                var t = list[i];
                list[i] = list[j];
                list[j] = t;
            }
        }

        static void Truncate<T>(List<T> list, int count)
        {
            count = Math.Max(0, count);
            if (list.Count > count) list.RemoveRange(count, list.Count - count);
        }

        public static ShopState CloneShopState(ShopState s)
        {
            return new ShopState
            {
                RelicOffers = new List<string>(s.RelicOffers),
                RelicPrices = new List<int>(s.RelicPrices),
                RelicBought = new List<bool>(s.RelicBought),
                SkillOffers = new List<string>(s.SkillOffers),
                SkillPrices = new List<int>(s.SkillPrices),
                SkillBought = new List<bool>(s.SkillBought),
                UpgradeCostPerLevel = s.UpgradeCostPerLevel,
                Message = s.Message,
            };
        }

        // EventOption 은 Data 쪽 클래스라 Clone 이 없음. 값 칸뿐이라 하나씩 복사
        public static EventOption CloneOption(EventOption o)
        {
            return new EventOption
            {
                ChoiceRow = o.ChoiceRow, Outcome = o.Outcome, Title = o.Title, Description = o.Description,
                SkillRow = o.SkillRow, RelicRow = o.RelicRow, Amount = o.Amount,
                CostHealth = o.CostHealth, CostMaxHealth = o.CostMaxHealth, CostCurrency = o.CostCurrency,
                CostSkills = o.CostSkills, CostRelics = o.CostRelics, CostRelic = o.CostRelic,
                BonusHeal = o.BonusHeal, BonusUpgrades = o.BonusUpgrades, BonusWinCurrency = o.BonusWinCurrency,
                bBonusWinRelic = o.bBonusWinRelic, bAvailable = o.bAvailable, UnavailableReason = o.UnavailableReason,
            };
        }
    }
}
