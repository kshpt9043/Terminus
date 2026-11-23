// 던전 방 패널 (언리얼 UMonsterRewardWidget / URestWidget / UShopWidget / UEventWidget / URescueWidget /
// UFloorVoteWidget / UStartSkillPickWidget / UStartRelicPickWidget / USettlementWidget 의 최소판)
using System.Collections.Generic;
using System.Linq;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.Map;
using Terminus.Run;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Screens
{
    public sealed partial class DungeonScreen
    {
        static DungeonScreen _current;
        static bool _hooked;

        readonly Dictionary<string, VisualElement> _modals = new Dictionary<string, VisualElement>();

        // NetBus 처리기는 한 번만 붙이고 지금 화면으로 넘김 (씬마다 화면이 새로 생겨도 처리기가 쌓이지 않게)
        void HookPanels()
        {
            _current = this;
            if (_hooked) return;
            _hooked = true;
            NetBus.On<RoomSelectFailedMsg>(m => _current?.Toast(m.Message));
            NetBus.On<FloorVoteRejectedMsg>(m => _current?.Toast(m.Message));
            NetBus.On<FloorTitleMsg>(m => _current?.Toast(m.Title + "\n" + m.Subtitle, 3f));
            NetBus.On<RoomRewardMsg>(m => _current?.ShowReward(m.Offer));
            NetBus.On<CloseRoomRewardMsg>(_ => _current?.CloseModal("reward"));
            NetBus.On<ShowRestMsg>(m => _current?.ShowRest(m));
            NetBus.On<RestResultMsg>(m => _current?.Toast(m.Result));
            NetBus.On<CloseRestMsg>(_ => _current?.CloseModal("rest"));
            NetBus.On<ShowShopMsg>(m => _current?.ShowShop(m.State));
            NetBus.On<CloseShopMsg>(_ => _current?.CloseModal("shop"));
            NetBus.On<ShowEventMsg>(m => _current?.ShowEvent(m));
            NetBus.On<EventResultMsg>(m => _current?.Toast(m.Result, 4f));
            NetBus.On<CloseEventMsg>(_ => _current?.CloseModal("event"));
            NetBus.On<ShowRescueMsg>(m => _current?.ShowRescue(m));
            NetBus.On<CloseRescueMsg>(_ => _current?.CloseModal("rescue"));
            NetBus.On<EnterRiftMsg>(m => _current?.Toast("이공간: " + string.Join(", ", m.WaitingFor) + " 님을 기다리는 중", 5f));
            NetBus.On<LeaveRiftMsg>(_ => _current?.Toast("이공간에서 나왔습니다"));
            NetBus.On<RunEndedMsg>(m => _current?.ShowRunEnded(m));
        }

        VisualElement OpenModal(string key, string title, out VisualElement body, float width = 820)
        {
            CloseModal(key);
            var dim = UiKit.Modal(out var panel, width);
            panel.Add(UiKit.Title(title));
            panel.Add(UiKit.Spacer(12));
            body = new ScrollView(ScrollViewMode.Vertical);
            body.style.maxHeight = 700;
            panel.Add(body);
            _modals[key] = dim;
            _modalLayer.Add(dim);
            return panel;
        }

        void CloseModal(string key)
        {
            if (_modals.TryGetValue(key, out var v))
            {
                v.RemoveFromHierarchy();
                _modals.Remove(key);
            }
        }

        bool IsOpen(string key) => _modals.ContainsKey(key);

        static string SkillName(string row) => GameData.GetSkillName(row);
        static string RelicName(string row) => GameData.GetRelicName(row);

        static string SkillDesc(string row)
        {
            var s = GameData.FindSkillRow(row);
            return s == null ? row : $"{s.DisplayName_KR} ({CostText(s)})\n{s.Description_KR}";
        }

        static string RelicDesc(string row)
        {
            var r = GameData.FindRelicRow(row);
            return r == null ? row : $"{r.RelicName}\n{r.RelicDesc}";
        }

        // ------------------------------------------------------------ 시작 고르기 (강화 스킬 / 창고 유물)

        void RefreshStartPick()
        {
            var me = ClientState.Me;
            if (me == null || !me.bHasBattler) return;
            var run = me.Run;

            if (!run.bStartSkillChosen)
            {
                if (IsOpen("startSkill")) return;
                var owned = LocalProfile.Service.GetUsableOwnedSkills(me.Class);
                OpenModal("startSkill", "시작 강화 스킬", out var body);
                body.Add(UiKit.Text(owned.Count == 0 ? "보유한 강화 스킬이 없습니다." : "이번 런에 가져갈 강화 스킬 하나를 고르세요.", "t-subtitle t-wrap"));
                foreach (var row in owned)
                {
                    var r = row;
                    var b = UiKit.Button(SkillDesc(row), () => { NetBus.Request(new ChooseStartSkillReq { SkillRow = r }); CloseModal("startSkill"); });
                    b.style.whiteSpace = WhiteSpace.Normal;
                    b.style.marginTop = 6;
                    body.Add(b);
                }
                body.Add(UiKit.Spacer(12));
                body.Add(UiKit.Hex(owned.Count == 0 ? "확인" : "고르지 않음", () => { NetBus.Request(new ChooseStartSkillReq { SkillRow = "" }); CloseModal("startSkill"); }, "secondary"));
                return;
            }
            CloseModal("startSkill");

            if (!run.bStartRelicsChosen)
            {
                if (IsOpen("startRelic")) return;
                var stored = LocalProfile.Service.StoredRelics.ToList();
                OpenModal("startRelic", "창고 유물", out var body);
                body.Add(UiKit.Text(stored.Count == 0 ? "창고에 유물이 없습니다." : $"던전에 들고 갈 유물을 {RunPlayer.MaxStartRelics}개까지 고르세요." + (ClientState.Party.bHardMode ? "\n하드 모드: 들고 간 유물은 창고에서 사라집니다." : ""), "t-subtitle t-wrap"));
                foreach (var row in stored)
                {
                    var r = row;
                    var b = UiKit.Button(RelicDesc(row), () => { NetBus.Request(new ChooseStartRelicsReq { RelicRows = new List<string> { r } }); CloseModal("startRelic"); });
                    b.style.whiteSpace = WhiteSpace.Normal;
                    b.style.marginTop = 6;
                    body.Add(b);
                }
                body.Add(UiKit.Spacer(12));
                body.Add(UiKit.Hex(stored.Count == 0 ? "확인" : "들고 가지 않음", () => { NetBus.Request(new ChooseStartRelicsReq()); CloseModal("startRelic"); }, "secondary"));
                return;
            }
            CloseModal("startRelic");
        }

        // ------------------------------------------------------------ 층 투표 (테마 끝 층 보스 뒤)

        int _shownVoteId = -1;
        FloorVotePhase _shownVotePhase = FloorVotePhase.None;

        void RefreshFloorVote()
        {
            var vote = ClientState.Map.Vote;
            if (vote == null || vote.Phase == FloorVotePhase.None)
            {
                CloseModal("vote");
                _shownVoteId = -1;
                return;
            }
            if (_shownVoteId == vote.VoteId && _shownVotePhase == vote.Phase && IsOpen("vote"))
            {
                return;
            }
            _shownVoteId = vote.VoteId;
            _shownVotePhase = vote.Phase;

            OpenModal("vote", $"{vote.Floor}층 보스 처치", out var body, 700);
            string phase = vote.Phase == FloorVotePhase.Voting ? $"행선지를 고르세요 ({vote.VotedCount}/{vote.TotalVoters})" :
                           vote.Phase == FloorVotePhase.Revealing ? "모두 골랐습니다. 공개합니다..." : "결과: " + ChoiceName(vote.Result);
            body.Add(UiKit.Text(phase, "t-subtitle"));
            for (int i = 0; i < vote.Options.Count; ++i)
            {
                var choice = vote.Options[i];
                int count = i < vote.Counts.Count ? vote.Counts[i] : 0;
                var b = UiKit.Button($"{ChoiceName(choice)}  ({count})", () => NetBus.Request(new CastFloorVoteReq { Choice = choice }));
                b.style.marginTop = 6;
                b.SetEnabled(vote.Phase == FloorVotePhase.Voting);
                body.Add(b);
            }
            foreach (var v in vote.RevealedVotes)
            {
                body.Add(UiKit.Text($"{ClientState.NameOf(v.PlayerId)}: {ChoiceName(v.Choice)}", "t-aux"));
            }
        }

        static string ChoiceName(FloorChoice c)
        {
            switch (c)
            {
                case FloorChoice.NextFloor: return "다음 층으로";
                case FloorChoice.Escape: return "탈출 (정산)";
                case FloorChoice.Betray: return "배신";
                default: return "-";
            }
        }

        // ------------------------------------------------------------ 방 보상

        void ShowReward(RoomRewardOffer offer)
        {
            OpenModal("reward", "보상", out var body);
            if (offer.Currency > 0) body.Add(UiKit.Text($"던전 재화 +{offer.Currency}", "t-section t-gold"));

            string chosenSkill = "", chosenRelic = "";
            var skillButtons = new List<Button>();
            var relicButtons = new List<Button>();

            if (offer.SkillOffers.Count > 0) body.Add(UiKit.Text("스킬 (하나만)", "t-subtitle"));
            foreach (var s in offer.SkillOffers)
            {
                var row = s;
                Button b = null;
                b = UiKit.Button(SkillDesc(row), () =>
                {
                    chosenSkill = chosenSkill == row ? "" : row;
                    foreach (var x in skillButtons) x.style.opacity = (string)x.userData == chosenSkill || chosenSkill == "" ? 1f : 0.5f;
                });
                b.userData = row;
                b.style.whiteSpace = WhiteSpace.Normal;
                b.style.marginTop = 6;
                skillButtons.Add(b);
                body.Add(b);
            }
            if (offer.RelicOffers.Count > 0) body.Add(UiKit.Text("유물 (하나만)", "t-subtitle"));
            foreach (var r in offer.RelicOffers)
            {
                var row = r;
                Button b = null;
                b = UiKit.Button(RelicDesc(row), () =>
                {
                    chosenRelic = chosenRelic == row ? "" : row;
                    foreach (var x in relicButtons) x.style.opacity = (string)x.userData == chosenRelic || chosenRelic == "" ? 1f : 0.5f;
                });
                b.userData = row;
                b.style.whiteSpace = WhiteSpace.Normal;
                b.style.marginTop = 6;
                relicButtons.Add(b);
                body.Add(b);
            }

            body.Add(UiKit.Spacer(12));
            body.Add(UiKit.Hex("다음으로", () =>
            {
                var me = ClientState.Me;
                int replaceSlot = -1;
                string replaceRelic = "";
                // 칸이 꽉 찼으면 가장 앞 칸 / 첫 번째로 버릴 수 있는 유물과 바꿈 (정식 화면에선 고르게)
                if (me != null && chosenSkill != "" && me.Run.EnhanceSkills.Count >= Mathf.Max(1, me.Run.Stats.EnhanceSlots)) replaceSlot = 0;
                if (me != null && chosenRelic != "" && me.Run.Relics.Count >= Mathf.Max(RelicCapacity.Base, me.Run.Stats.MaxRelics))
                {
                    replaceRelic = me.Run.Relics.FirstOrDefault(x => GameData.FindRelicRow(x)?.RelicTier != RelicTier.Basic) ?? "";
                }
                NetBus.Request(new FinishRoomRewardReq { ChosenSkill = chosenSkill, ReplaceSlot = replaceSlot, ChosenRelic = chosenRelic, ReplaceRelic = replaceRelic });
                CloseModal("reward");
            }, "primary"));
        }

        // ------------------------------------------------------------ 휴식

        void ShowRest(ShowRestMsg m)
        {
            OpenModal("rest", "휴식터", out var body);
            body.Add(UiKit.Text($"한 명을 골라 최대 체력의 {Mathf.RoundToInt(m.HealRatio * 100)}% 를 회복시키거나, 주변을 탐색합니다 (던전 재화 {m.ExploreMin}~{m.ExploreMax}, 유물 {Mathf.RoundToInt(m.RelicChance * 100)}%).", "t-subtitle t-wrap"));
            foreach (var id in m.Occupants)
            {
                var target = id;
                var p = ClientState.Party.Find(id);
                var label = p == null ? "?" : $"{p.Name} 회복  ({p.Combat.Health}/{p.Stats.MaxHealth})";
                var b = UiKit.Button(label, () => NetBus.Request(new RestTargetReq { TargetId = target }));
                b.style.marginTop = 6;
                body.Add(b);
            }
            var explore = UiKit.Button("주변 탐색", () => NetBus.Request(new RestExploreReq()));
            explore.style.marginTop = 6;
            body.Add(explore);
        }

        // ------------------------------------------------------------ 상점

        void ShowShop(ShopState s)
        {
            OpenModal("shop", "상점", out var body, 980);
            var me = ClientState.Me;
            body.Add(UiKit.Text($"던전 재화 {(me != null ? me.Run.Currency : 0)}" + (string.IsNullOrEmpty(s.Message) ? "" : "   ·   " + s.Message), "t-section t-gold"));

            body.Add(UiKit.Text("유물", "t-subtitle"));
            for (int i = 0; i < s.RelicOffers.Count; ++i)
            {
                int index = i;
                bool bought = i < s.RelicBought.Count && s.RelicBought[i];
                var b = UiKit.Button($"{RelicDesc(s.RelicOffers[i])}\n가격 {s.RelicPrices[i]}" + (bought ? " (구매함)" : ""), () => NetBus.Request(new ShopBuyRelicReq { Index = index }));
                b.style.whiteSpace = WhiteSpace.Normal;
                b.style.marginTop = 6;
                b.SetEnabled(!bought);
                body.Add(b);
            }

            body.Add(UiKit.Text("스킬", "t-subtitle"));
            for (int i = 0; i < s.SkillOffers.Count; ++i)
            {
                int index = i;
                bool bought = i < s.SkillBought.Count && s.SkillBought[i];
                var b = UiKit.Button($"{SkillDesc(s.SkillOffers[i])}\n가격 {s.SkillPrices[i]}" + (bought ? " (구매함)" : ""), () =>
                {
                    var m = ClientState.Me;
                    int replace = m != null && m.Run.EnhanceSkills.Count >= Mathf.Max(1, m.Run.Stats.EnhanceSlots) ? 0 : -1;
                    NetBus.Request(new ShopBuySkillReq { Index = index, ReplaceSlot = replace });
                });
                b.style.whiteSpace = WhiteSpace.Normal;
                b.style.marginTop = 6;
                b.SetEnabled(!bought);
                body.Add(b);
            }

            if (me != null)
            {
                body.Add(UiKit.Text("내 강화 스킬 강화", "t-subtitle"));
                foreach (var row in me.Run.EnhanceSkills)
                {
                    var r = row;
                    var b = UiKit.Button($"{SkillName(row)} 강화", () => NetBus.Request(new ShopUpgradeSkillReq { SkillRow = r }));
                    b.style.marginTop = 4;
                    body.Add(b);
                }
                body.Add(UiKit.Text("유물 팔기", "t-subtitle"));
                foreach (var row in me.Run.Relics)
                {
                    var relic = GameData.FindRelicRow(row);
                    if (relic == null || !relic.CanSellInShop()) continue;
                    var r = row;
                    var b = UiKit.Button($"{relic.RelicName} 팔기 (+{relic.SellPrice_Dungeon})", () => NetBus.Request(new ShopSellRelicReq { RelicRow = r }));
                    b.style.marginTop = 4;
                    body.Add(b);
                }
            }

            body.Add(UiKit.Spacer(12));
            body.Add(UiKit.Hex("나가기", () => NetBus.Request(new ShopLeaveReq()), "secondary"));
        }

        // ------------------------------------------------------------ 이벤트 / 퀘스트 방

        void ShowEvent(ShowEventMsg m)
        {
            OpenModal("event", m.Title, out var body);
            body.Add(UiKit.Text(m.Situation, "t-body t-wrap"));
            body.Add(UiKit.Spacer(12));
            for (int i = 0; i < m.Options.Count; ++i)
            {
                int index = i;
                var o = m.Options[i];
                string extra = o.GivesSkill() ? "\n→ " + SkillName(o.SkillRow) : o.GivesRelic() ? "\n→ " + RelicName(o.RelicRow) : "";
                var b = UiKit.Button(o.Title + "\n" + o.Description + extra + (o.bAvailable ? "" : "\n(" + o.UnavailableReason + ")"), () =>
                {
                    var me = ClientState.Me;
                    int replace = me != null && o.GivesSkill() && me.Run.EnhanceSkills.Count >= Mathf.Max(1, me.Run.Stats.EnhanceSlots) ? 0 : -1;
                    string drop = "";
                    if (me != null && o.GivesRelic() && me.Run.Relics.Count >= Mathf.Max(RelicCapacity.Base, me.Run.Stats.MaxRelics))
                    {
                        drop = me.Run.Relics.FirstOrDefault(x => GameData.FindRelicRow(x)?.RelicTier != RelicTier.Basic) ?? "";
                    }
                    NetBus.Request(new ChooseEventReq { Index = index, ReplaceSlot = replace, RelicRow = drop });
                });
                b.style.whiteSpace = WhiteSpace.Normal;
                b.style.marginTop = 6;
                b.SetEnabled(o.bAvailable);
                body.Add(b);
            }
        }

        // ------------------------------------------------------------ 구출 투표

        void ShowRescue(ShowRescueMsg m)
        {
            OpenModal("rescue", "구출", out var body);
            body.Add(UiKit.Text($"{string.Join(", ", m.WipedNames)} 님이 쓰러졌습니다." + (m.bChooser ? $"\n구출하면 살아 있는 사람이 현재 체력의 {Mathf.RoundToInt(m.HealthCost * 100)}% 를 잃습니다." : "\n다른 사람의 선택을 기다리는 중..."), "t-subtitle t-wrap"));
            if (!m.bChooser) return;
            body.Add(UiKit.Hex("구출", () => { NetBus.Request(new ChooseRescueReq { bIntervene = false }); CloseModal("rescue"); }, "secondary"));
            if (m.bCanIntervene) body.Add(UiKit.Hex("난입", () => { NetBus.Request(new ChooseRescueReq { bIntervene = true }); CloseModal("rescue"); }, "primary"));
        }

        // ------------------------------------------------------------ 런 끝 -> 정산 (각자 프로필)

        void ShowRunEnded(RunEndedMsg m)
        {
            foreach (var key in _modals.Keys.ToList()) CloseModal(key);
            var profile = LocalProfile.Service;
            var pending = SettlementRules.MakeFromRun(profile, m.Run, m.Adjust, m.Message, m.RoomName, m.Floor, m.bDeath);
            profile.BeginSettlement(pending);

            OpenModal("settle", m.bDeath ? "전멸" : "정산", out var body, 900);
            body.Add(UiKit.Text(m.Message, "t-subtitle t-wrap"));
            var kept = new List<string>();
            var gold = UiKit.Text("", "t-section t-gold");

            void UpdateGold()
            {
                gold.text = $"판매 골드 {SettlementRules.SaleGold(pending, kept)}" + (pending.BonusGold > 0 ? $" + 추가 {pending.BonusGold}" : "");
            }

            if (pending.Relics.Count > 0) body.Add(UiKit.Text("창고에 보관할 유물을 고르세요. 고르지 않은 유물은 골드로 팝니다.", "t-body t-wrap"));
            for (int i = 0; i < pending.Relics.Count; ++i)
            {
                var row = pending.Relics[i];
                int price = i < pending.RelicGold.Count ? pending.RelicGold[i] : 0;
                var t = new Toggle($"{RelicName(row)}  (판매 {price})");
                t.RegisterValueChangedCallback(e =>
                {
                    SettlementRules.ToggleKept(kept, row);
                    UpdateGold();
                });
                body.Add(t);
            }
            UpdateGold();
            body.Add(gold);
            body.Add(UiKit.Spacer(12));
            body.Add(UiKit.Hex("확인", () =>
            {
                var result = SettlementRules.Confirm(profile, kept);
                Toast($"골드 {result.GoldAfter:N0}", 2f);
                NetworkRoot.LeaveToMenu();
            }, "primary"));
        }
    }
}
