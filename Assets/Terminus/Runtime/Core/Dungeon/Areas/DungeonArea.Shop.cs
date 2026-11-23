// 상점 (언리얼 ADungeonArea::BeginShop / HandleShop* / CheckShopDone / FinishShop)
// 사용자 결정 2026-10-09: 진열은 미정이라 도감(데이터 전체)에서 아무거나. 사람마다 따로, 전원이 나가면 방이 끝남
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonArea
    {
        // 상점: 각자 진열 / 나간 사람 (서버만)
        readonly Dictionary<DungeonPlayer, ShopState> _shopStates = new Dictionary<DungeonPlayer, ShopState>();
        readonly HashSet<DungeonPlayer> _shopDone = new HashSet<DungeonPlayer>();
        bool _bShopping;
        readonly TimerHandle _shopTimer = new TimerHandle();

        public bool IsShopping => _bShopping;
        public bool HasLeftShop(DungeonPlayer p) => p != null && _shopDone.Contains(p);

        /// <summary>이 사람의 상점 (사본). 없으면 null</summary>
        public ShopState GetShopState(DungeonPlayer p)
        {
            return p != null && _shopStates.TryGetValue(p, out var s) ? AreaEventRules.CloneShopState(s) : null;
        }

        void BeginShop()
        {
            _bShopping = true;
            _shopStates.Clear();
            _shopDone.Clear();

            foreach (var p in _occupants)
            {
                _shopStates[p] = AreaEventRules.MakeShopState(p.Run, CurrentFloor, Settings, Rng);
                SendShop(p, "");
            }

            CheckShopDone();
        }

        void SendShop(DungeonPlayer p, string message)
        {
            if (p == null || !_shopStates.TryGetValue(p, out var state)) return;

            state.Message = message ?? "";
            Notifier?.ShowShop(p, AreaEventRules.CloneShopState(state));
        }

        ShopState OpenShopOf(DungeonPlayer p)
        {
            return _bShopping && p != null && p.Run != null && !_shopDone.Contains(p) && _shopStates.TryGetValue(p, out var s) ? s : null;
        }

        public void HandleShopBuyRelic(DungeonPlayer p, int index)
        {
            var state = OpenShopOf(p);
            if (state == null || index < 0 || index >= state.RelicOffers.Count || state.RelicBought[index]) return;

            var ps = p.Run;
            var row = state.RelicOffers[index];
            int price = state.RelicPrices[index];
            if (ps.RunState.Currency < price)
            {
                SendShop(p, "던전 재화가 부족합니다.");
                return;
            }
            if (ps.RunState.Relics.Count >= ps.GetRelicCapacity())
            {
                SendShop(p, "유물 칸이 가득 찼습니다. 먼저 유물을 판매하세요.");
                return;
            }
            if (!ps.GainRelic(row))
            {
                SendShop(p, "이 유물은 가질 수 없습니다.");
                return;
            }

            ps.AddCurrency(-price);
            state.RelicBought[index] = true;
            SendShop(p, $"구매: {GameData.GetRelicName(row)}");
        }

        public void HandleShopBuySkill(DungeonPlayer p, int index, int replaceSlot)
        {
            var state = OpenShopOf(p);
            if (state == null || index < 0 || index >= state.SkillOffers.Count || state.SkillBought[index]) return;

            var ps = p.Run;
            var row = state.SkillOffers[index];
            int price = state.SkillPrices[index];
            if (ps.RunState.Currency < price)
            {
                SendShop(p, "던전 재화가 부족합니다.");
                return;
            }
            if (!ps.EquipEnhanceSkill(row, replaceSlot))
            {
                SendShop(p, "강화 스킬 칸이 가득 찼습니다. 바꿀 칸을 고르세요.");
                return;
            }

            ps.AddCurrency(-price);
            state.SkillBought[index] = true;
            SendShop(p, $"구매: {GameData.GetSkillName(row)}");
        }

        public void HandleShopSellRelic(DungeonPlayer p, string relicRow)
        {
            if (OpenShopOf(p) == null) return;

            var ps = p.Run;
            var relic = GameData.FindRelicRow(relicRow);
            if (relic == null || !ps.RunState.Relics.Contains(relicRow)) return;
            if (relic.RelicTier == RelicTier.Basic || !relic.CanSellInShop())
            {
                SendShop(p, "이 유물은 팔 수 없습니다.");
                return;
            }

            ps.RemoveRelic(relicRow);
            ps.AddCurrency(relic.SellPrice_Dungeon);
            ps.MarkRelicsSeen(new[] { relicRow });   // 판 유물은 이번 런엔 다시 안 나옴
            SendShop(p, $"판매: {relic.RelicName} (+{relic.SellPrice_Dungeon})");
        }

        public void HandleShopUpgradeSkill(DungeonPlayer p, string skillRow)
        {
            if (OpenShopOf(p) == null) return;

            var ps = p.Run;
            if (!ps.RunState.EnhanceSkills.Contains(skillRow)) return;

            // 노션(10-09): 같은 계층 + 같은 직업 또는 shared 인 픽업 스킬만 강화
            var skill = GameData.FindSkillRow(skillRow);
            var myOwner = CharacterNames.ToSkillOwner(ps.CharacterClass);
            if (skill == null || skill.SkillTier != AreaEventRules.SkillTierOf(CurrentFloor)
                || (skill.OwnerClass != myOwner && skill.OwnerClass != SkillOwner.Shared))
            {
                SendShop(p, "이 상점에서는 지금 계층의 내 직업 / 공용 스킬만 강화할 수 있습니다.");
                return;
            }

            if (!RunPlayer.IsUpgradableSkill(skill))
            {
                SendShop(p, "특수 스킬은 강화할 수 없습니다.");
                return;
            }

            int level = ps.GetPickupSkillLevel(skillRow);
            if (level >= RunPlayer.MaxPickupSkillLevel)
            {
                SendShop(p, "이미 최대로 강화했습니다.");
                return;
            }

            int cost = Settings.ShopUpgradeCostPerLevel * (level + 1);
            if (ps.RunState.Currency < cost)
            {
                SendShop(p, "던전 재화가 부족합니다.");
                return;
            }

            ps.UpgradePickupSkill(skillRow);
            ps.AddCurrency(-cost);
            SendShop(p, $"강화: {GameData.GetSkillName(skillRow)} +{level + 1}");
        }

        public void HandleShopLeave(DungeonPlayer p)
        {
            if (!_bShopping || p == null || !_occupants.Contains(p) || _shopDone.Contains(p)) return;

            _shopDone.Add(p);
            Notifier?.CloseShop(p);
            CheckShopDone();
        }

        void CheckShopDone()
        {
            foreach (var p in _occupants)
            {
                if (!_shopDone.Contains(p)) return;
            }
            SetTimer(_shopTimer, Settings.ShopFinishDelay, FinishShop);
        }

        void FinishShop()
        {
            if (!_bShopping) return;
            MarkCleared();
        }
    }
}
