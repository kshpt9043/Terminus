// 상점 화면 (언리얼 UShopWidget + WBP_ShopWidget, 사용자 결정 2026-10-09). 내 진열은 서버가 ShopState 로 보내 줌 (사람마다 따로)
//  - 유물 구매 / 픽업 스킬 구매 (강화 칸이 꽉 차면 바꿀 칸을 고름)
//  - 장착한 픽업 스킬 강화 (공격 / 방어만, 최대 단계까지)
//  - 보유 유물 판매 (직업 기본 유물 / 판매가 없는 유물은 못 팜)
//  - 나가기: 같은 방 전원이 나가면 지도로
// 던전 재화 / 보유 유물 / 장착 스킬 / 강화 단계가 바뀌면 (스냅샷) 다시 그림
// WBP 배치: 화면 전체 그림(ShopBackground) + 나가기 버튼 (앵커 0.2445~0.3406 x 0.375~0.6458) + 가운데 기준 왼쪽 -220 세로 줄
//  [유물][RelicBuyBox][스킬][SkillBuyBox][강화][UpgradeBox][판매][SellBox]. WBP 에 제목 / 재화 / 안내 글이 없어서 안 보임 (언리얼과 같음)
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    public sealed class ShopWidget : VisualElement
    {
        // 진열 칸 72x72, 아이콘만 (이름 / 테두리 없이, 정보는 마우스를 올리면 툴팁) (사용자 결정 2026-10-12)
        static readonly Vector2 SlotSize = new Vector2(72f, 72f);
        const bool IconOnlySlots = true;

        enum Action { BuyRelic, BuySkill, Upgrade, Sell }

        readonly VisualElement _relicBuyBox;
        readonly VisualElement _skillBuyBox;
        readonly VisualElement _upgradeBox;
        readonly VisualElement _sellBox;
        readonly ItemPickPanelWidget _replacePanel;
        readonly VisualElement _replaceHost;
        readonly Button _leaveButton;

        ShopState _state = new ShopState();
        int _pendingSkillIndex = -1;   // 바꿀 칸을 고르는 중인 스킬 진열 번호
        string _shownKey;              // 마지막으로 그린 내 상태 (재화 / 유물 / 스킬 / 강화 단계)

        public ShopWidget()
        {
            RoomLooks.Use(this, "Shop");
            AddToClassList("rp-root");
            pickingMode = PickingMode.Position;   // 아래 화면을 못 누르게

            // [1] Image_0 (화면 전체)
            Add(RoomLooks.FullImage("/Game/JSY/Widget/Temp_Icons/ShopBackground", "BackgroundImage"));

            // [2] LeaveButton (앵커 영역 그대로) "나가기" 16 Regular
            _leaveButton = new Button(HandleLeave) { name = "LeaveButton", text = "나가기" };
            _leaveButton.AddToClassList("shop-leave");
            Add(_leaveButton);

            // [4] VerticalBox_0 (가운데 기준 왼쪽 -220, 세로 가운데 정렬)
            var column = new VisualElement { name = "VerticalBox_0", pickingMode = PickingMode.Ignore };
            column.AddToClassList("shop-column");
            Add(column);

            _relicBuyBox = AddSection(column, "유물", "RelicBuyBox");
            _skillBuyBox = AddSection(column, "스킬", "SkillBuyBox");
            _upgradeBox = AddSection(column, "강화", "UpgradeBox");
            _sellBox = AddSection(column, "판매", "SellBox");

            // WBP 에 ReplacePanel 이 없어서 C++ 이 붙인 판 (가운데 떠 있음)
            _replacePanel = ItemPickPanelWidget.CreateFloating(out _replaceHost);
            _replacePanel.SlotSize = SlotSize;
            _replacePanel.Picked += HandleReplacePicked;
            _replacePanel.Cancelled += HandleReplaceCancel;
            Add(_replaceHost);

            // 내 상태(재화 / 유물 / 스킬 / 강화)가 바뀌면 다시 그림 (언리얼 OnRunStateChanged)
            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                ClientState.PartyChanged -= HandleRunStateChanged;
                ClientState.PartyChanged += HandleRunStateChanged;
            });
            RegisterCallback<DetachFromPanelEvent>(_ => ClientState.PartyChanged -= HandleRunStateChanged);
        }

        // 제목(22, #C9A45C) + 랩 박스
        static VisualElement AddSection(VisualElement column, string title, string boxName)
        {
            var titleText = UiStyle.Text(title, 22, UiColor.Gold, TextAnchor.MiddleLeft, true);
            titleText.AddToClassList("shop-sectiontitle");
            column.Add(titleText);
            var box = new VisualElement { name = boxName, pickingMode = PickingMode.Ignore };
            box.AddToClassList("shop-box");
            column.Add(box);
            return box;
        }

        public void SetShopState(ShopState state)
        {
            _state = state ?? new ShopState();
            _pendingSkillIndex = -1;
            _replacePanel.Hide();
            Rebuild();
        }

        void HandleRunStateChanged()
        {
            // 방문 기록처럼 상점과 상관없는 변화면 그대로
            if (MakeStateKey() != _shownKey) Rebuild();
        }

        static string MakeStateKey()
        {
            var run = RoomLooks.MyRun();
            if (run == null) return "";
            var state = run.RunState;
            var key = state.Currency + "|";
            foreach (var row in state.Relics) key += row + ",";
            key += "|";
            foreach (var row in state.EnhanceSkills) key += row + ":" + run.GetPickupSkillLevel(row) + ",";
            return key;
        }

        void AddCard(VisualElement box, bool relic, string row, string caption, string buttonLabel, bool enabled, Action action, int index, bool unaffordable = false, int upgradeLevel = 0)
        {
            var card = new ItemCardWidget();

            // 강화 카드: 툴팁 수치도 지금 단계만큼 (BaseValue +단계)
            SkillRow upgraded = null;
            if (!relic && upgradeLevel > 0)
            {
                var skill = GameData.FindSkillRow(row);
                if (skill != null)
                {
                    upgraded = skill.Clone();
                    upgraded.BaseValue += upgradeLevel;
                }
            }
            card.SetItem(relic, row, SlotSize, true, upgraded);
            card.Slot?.SetIconOnly(IconOnlySlots);
            card.SetCaption(caption);
            card.SetAction(buttonLabel, enabled, CardActionStyle.Currency);
            if (unaffordable) card.SetActionLabelColor(UiColor.Alert);   // 재화 부족 = 비용 경고색
            card.ActionClicked += () => HandleAction(action, index, row);
            card.AddToClassList("shop-card");
            box.Add(card);
        }

        static Label EmptyLine(string text)
        {
            var line = UiStyle.Text(text, 13, UiColor.TextFaint, TextAnchor.MiddleLeft);
            line.AddToClassList("shop-empty");
            return line;
        }

        void Rebuild()
        {
            _shownKey = MakeStateKey();

            var run = RoomLooks.MyRun();
            var state = run != null ? run.RunState : new RunState();
            int currency = state.Currency;

            // 유물 구매
            _relicBuyBox.Clear();
            for (int i = 0; i < _state.RelicOffers.Count; ++i)
            {
                bool bought = i < _state.RelicBought.Count && _state.RelicBought[i];
                int price = i < _state.RelicPrices.Count ? _state.RelicPrices[i] : 0;
                AddCard(_relicBuyBox, true, _state.RelicOffers[i], "",
                    bought ? " 구매함 " : $" 구매 {price} ",
                    !bought && currency >= price, Action.BuyRelic, i, !bought && currency < price);
            }
            if (_state.RelicOffers.Count == 0) _relicBuyBox.Add(EmptyLine("살 수 있는 유물이 없습니다."));

            // 픽업 스킬 구매
            _skillBuyBox.Clear();
            for (int i = 0; i < _state.SkillOffers.Count; ++i)
            {
                bool bought = i < _state.SkillBought.Count && _state.SkillBought[i];
                int price = i < _state.SkillPrices.Count ? _state.SkillPrices[i] : 0;
                AddCard(_skillBuyBox, false, _state.SkillOffers[i], "",
                    bought ? " 구매함 " : $" 구매 {price} ",
                    !bought && currency >= price, Action.BuySkill, i, !bought && currency < price);
            }
            if (_state.SkillOffers.Count == 0) _skillBuyBox.Add(EmptyLine("살 수 있는 스킬이 없습니다."));

            // 장착한 픽업 스킬 강화 (특수 스킬은 강화가 없음 -> 안 띄움)
            _upgradeBox.Clear();
            int shown = 0;
            foreach (var skillRow in state.EnhanceSkills)
            {
                var skill = GameData.FindSkillRow(skillRow);
                if (skill == null || !RunPlayer.IsUpgradableSkill(skill)) continue;
                ++shown;

                int level = run != null ? run.GetPickupSkillLevel(skillRow) : 0;
                bool max = level >= RunPlayer.MaxPickupSkillLevel;
                int cost = _state.UpgradeCostPerLevel * (level + 1);
                AddCard(_upgradeBox, false, skillRow, "+" + level,
                    max ? " 최대 " : $" 강화 {cost} ",
                    !max && currency >= cost, Action.Upgrade, -1, !max && currency < cost, level);
            }
            if (shown == 0) _upgradeBox.Add(EmptyLine("강화할 수 있는 픽업 스킬(공격 / 방어)이 없습니다."));

            // 보유 유물 판매
            _sellBox.Clear();
            foreach (var relicRow in state.Relics)
            {
                var relic = GameData.FindRelicRow(relicRow);
                bool canSell = relic != null && relic.RelicTier != RelicTier.Basic && relic.CanSellInShop();
                AddCard(_sellBox, true, relicRow, "",
                    canSell ? $" 판매 {relic.SellPrice_Dungeon} " : " 판매 불가 ",
                    canSell, Action.Sell, -1);
            }

            if (_pendingSkillIndex >= 0) ShowReplace();
        }

        void HandleAction(Action action, int index, string row)
        {
            switch (action)
            {
                case Action.BuyRelic:
                    NetBus.Request(new ShopBuyRelicReq { Index = index });
                    break;

                case Action.BuySkill:
                    // 강화 칸이 꽉 찼으면 바꿀 칸부터
                    if (RoomLooks.EnhanceSlotsFull())
                    {
                        _pendingSkillIndex = index;
                        ShowReplace();
                        return;
                    }
                    NetBus.Request(new ShopBuySkillReq { Index = index, ReplaceSlot = -1 });
                    break;

                case Action.Upgrade:
                    NetBus.Request(new ShopUpgradeSkillReq { SkillRow = row });
                    break;

                case Action.Sell:
                    NetBus.Request(new ShopSellRelicReq { RelicRow = row });
                    break;
            }
        }

        void ShowReplace()
        {
            _replacePanel.ShowEnhanceSkills("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요.");
        }

        void HandleReplacePicked(ItemSlotWidget slot)
        {
            if (slot == null || _pendingSkillIndex < 0) return;
            NetBus.Request(new ShopBuySkillReq { Index = _pendingSkillIndex, ReplaceSlot = slot.SlotIndex });
            HandleReplaceCancel();
        }

        void HandleReplaceCancel()
        {
            _pendingSkillIndex = -1;
            _replacePanel.Hide();
        }

        void HandleLeave()
        {
            NetBus.Request(new ShopLeaveReq());
        }
    }
}
