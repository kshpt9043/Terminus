// 창고 (언리얼 UStorageWidget + WBP_Storage). 메인 메뉴 '창고' -> UILayer.MenuWindow
//  - 첫 화면: 제목 + 카드 두 장 (유물 창고 / 스킬 창고)
//  - 목록 화면: 제목 / 개수 + 격자(6열, 최소 24칸, 빈 칸도 보임) | 상세 (이름 / 종류 / 설명)
//  - 우상단 보유 골드. ESC = 한 단계 뒤로 (목록 -> 첫 화면 -> 닫기)
using System.Collections.Generic;
using System.Globalization;
using Terminus.Data;
using Terminus.Game.Save;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class StorageView : VisualElement
    {
        enum Tab { Relic, Skill }

        const int Columns = 6;
        const int MinSlots = 24;

        // WBP GridBox 판 (0.005605,0.006512,0.008568 = #111317) / DetailBox 판 (#1B1E25)
        static readonly Color GridFill = UiColor.FromLinear(0.005605f, 0.006512f, 0.008568f);

        readonly Label _gold;
        readonly Button _closeButton;
        readonly Button _backButton;
        readonly Button _mainMenuButton;
        readonly VisualElement _hub;
        readonly VisualElement _list;
        readonly Label _listTitle;
        readonly Label _count;
        readonly VisualElement _grid;
        readonly Label _detailName;
        readonly Label _detailInfo;
        readonly Label _detailDesc;
        readonly List<ItemSlotWidget> _slots = new List<ItemSlotWidget>();

        Tab _tab = Tab.Relic;
        bool _inList;
        ItemSlotWidget _selected;

        public StorageView()
        {
            MenuStyle.Use(this, "Menu", "MenuWindows");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;

            // [1] 배경, 안쪽 48 / 32
            var background = MenuStyle.FullScreen(MenuStyle.WindowBackground);
            UiStyle.Padding(background, 48, 32);
            Add(background);
            var stage = new VisualElement();
            stage.style.flexGrow = 1;
            background.Add(stage);

            // ---- [14] 첫 화면: 제목 + 카드 두 장
            _hub = Fill(UiStyle.Column());
            _hub.style.alignItems = Align.Center;
            stage.Add(_hub);
            _hub.Add(MenuStyle.Text("창고", UiStyle.ScreenTitle, UiColor.TextBody, true, TextAnchor.MiddleCenter));
            var cards = UiStyle.Row();
            cards.style.flexGrow = 1;
            cards.style.alignItems = Align.Center;
            _hub.Add(cards);
            var relicCard = HubCard("유물 창고", () => ShowList(Tab.Relic));
            relicCard.style.marginRight = 40;
            cards.Add(relicCard);
            cards.Add(HubCard("스킬 창고", () => ShowList(Tab.Skill)));

            // ---- [25] 목록 화면: 제목 / 개수 + 격자 | 상세
            _list = Fill(UiStyle.Column());
            _list.style.alignItems = Align.Center;
            stage.Add(_list);
            _listTitle = MenuStyle.Text("", UiStyle.ScreenTitle, UiColor.TextBody, true, TextAnchor.MiddleCenter);
            _list.Add(_listTitle);
            _count = MenuStyle.Text("", 13, UiColor.TextAux, true, TextAnchor.MiddleCenter);
            _count.style.marginTop = 6;
            _count.style.marginBottom = 18;
            _list.Add(_count);

            var body = UiStyle.Row();
            body.style.flexGrow = 1;
            body.style.alignItems = Align.Stretch;
            body.style.marginBottom = 24;
            _list.Add(body);

            var gridBox = new VisualElement();
            gridBox.style.backgroundColor = GridFill;
            UiStyle.Border(gridBox, 1f, UiColor.Line);
            UiStyle.Radius(gridBox, 4f);
            UiStyle.Padding(gridBox, 16, 16);
            body.Add(gridBox);
            var scroll = new ScrollView(ScrollViewMode.Vertical);
            scroll.AddToClassList("tm-scroll");
            scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            scroll.verticalScrollerVisibility = ScrollerVisibility.AlwaysVisible;   // AlwaysShowScrollbar
            scroll.style.flexGrow = 1;
            gridBox.Add(scroll);
            _grid = new VisualElement();
            _grid.style.flexDirection = FlexDirection.Row;
            _grid.style.flexWrap = Wrap.Wrap;
            // 6열 x (칸 84 + 여백 12)
            _grid.style.width = Columns * (ItemSlotWidget.DefaultSize.x + 12f);
            scroll.Add(_grid);

            var detail = MenuStyle.RoundPanel(1f, 6f);
            detail.style.width = 280;
            detail.style.marginLeft = 16;
            UiStyle.Padding(detail, 14, 14);
            body.Add(detail);
            _detailName = MenuStyle.Wrap("", 17, UiColor.TextBody, true);
            detail.Add(_detailName);
            _detailInfo = MenuStyle.Wrap("", 13, UiColor.TextAux, true);
            _detailInfo.style.marginTop = 4;
            detail.Add(_detailInfo);
            _detailDesc = MenuStyle.Wrap("", 15, UiColor.TextBody, true);
            _detailDesc.style.marginTop = 12;
            detail.Add(_detailDesc);

            // ---- [3] 우상단: 보유 골드 + 버튼 (첫 화면 = 돌아가기(닫기), 목록 = 돌아가기 / 메인화면)
            var corner = UiStyle.Column();
            corner.style.position = Position.Absolute;
            corner.style.right = 0;
            corner.style.top = 0;
            stage.Add(corner);
            _gold = MenuStyle.Text("", 15, UiColor.Gold, true);
            corner.Add(_gold);
            _closeButton = CornerButton("돌아가기", Close);
            corner.Add(_closeButton);
            _backButton = CornerButton("돌아가기", ShowHub);
            corner.Add(_backButton);
            _mainMenuButton = CornerButton("메인화면", Close);
            corner.Add(_mainMenuButton);

            MenuStyle.OnEscape(this, HandleEscape);
            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                var profile = LocalProfile.Service;
                profile.StorageChanged -= Refresh;
                profile.StorageChanged += Refresh;
                profile.GoldChanged -= OnGoldChanged;
                profile.GoldChanged += OnGoldChanged;
            });
            RegisterCallback<DetachFromPanelEvent>(_ =>
            {
                LocalProfile.Service.StorageChanged -= Refresh;
                LocalProfile.Service.GoldChanged -= OnGoldChanged;
            });
        }

        static VisualElement Fill(VisualElement e)
        {
            e.style.position = Position.Absolute;
            e.style.left = 0;
            e.style.right = 0;
            e.style.top = 0;
            e.style.bottom = 0;
            return e;
        }

        static Button CornerButton(string label, System.Action onClick)
        {
            var b = MenuStyle.FlatButton(label, onClick);
            b.style.width = 170;
            b.style.height = 46;
            b.style.marginTop = 10;
            return b;
        }

        // 큰 카드 버튼 (시안의 회색 상자, 아래쪽에 금 글자. 안쪽 20)
        static Button HubCard(string label, System.Action onClick)
        {
            var b = MenuStyle.FlatButton("", onClick);
            b.AddToClassList("tm-hubcard");
            b.style.width = 220 + 40;
            b.style.height = 320 + 40;
            var text = MenuStyle.Wrap(label, 22, UiColor.Gold, true, TextAnchor.MiddleCenter);
            text.pickingMode = PickingMode.Ignore;
            text.style.position = Position.Absolute;
            text.style.left = 20;
            text.style.right = 20;
            text.style.bottom = 40;
            b.Add(text);
            return b;
        }

        // ------------------------------------------------------------ 열기 / 화면 전환

        public void Open()
        {
            UIRoot.Instance?.Show(UILayer.MenuWindow, this);
            RefreshGold();
            ShowHub();
            schedule.Execute(() => Focus());
        }

        public void Close()
        {
            RemoveFromHierarchy();
        }

        void HandleEscape()
        {
            if (_inList) ShowHub();
            else Close();
        }

        void ShowHub()
        {
            _inList = false;
            UiStyle.SetShown(_hub, true);
            UiStyle.SetShown(_list, false);
            UiStyle.SetShown(_closeButton, true);
            UiStyle.SetShown(_backButton, false);
            UiStyle.SetShown(_mainMenuButton, false);
        }

        void ShowList(Tab tab)
        {
            _inList = true;
            _tab = tab;
            UiStyle.SetShown(_hub, false);
            UiStyle.SetShown(_list, true);
            UiStyle.SetShown(_closeButton, false);
            UiStyle.SetShown(_backButton, true);
            UiStyle.SetShown(_mainMenuButton, true);
            _listTitle.text = tab == Tab.Relic ? "유물 창고" : "스킬 창고";
            Refresh();
        }

        // ------------------------------------------------------------ 목록 / 상세

        void Refresh()
        {
            var profile = LocalProfile.Service;
            var items = _tab == Tab.Relic ? profile.StoredRelics : profile.OwnedSkills;
            _count.text = $"{(_tab == Tab.Relic ? "보관 유물" : "보유 스킬")} {items.Count}개";

            // 칸 수: 최소 칸 수 이상, 줄 단위로 맞춤 (빈 칸도 보여서 인벤토리처럼). 칸 위젯은 모자랄 때만 더 만듦
            int needed = Mathf.Max(MinSlots, items.Count);
            int total = (needed + Columns - 1) / Columns * Columns;
            while (_slots.Count < total)
            {
                var slot = new ItemSlotWidget();
                slot.style.marginLeft = 6;
                slot.style.marginRight = 6;
                slot.style.marginTop = 6;
                slot.style.marginBottom = 6;
                slot.Clicked += OnSlotClicked;
                _slots.Add(slot);
            }

            _grid.Clear();
            for (int i = 0; i < total; ++i)
            {
                var slot = _slots[i];
                slot.SlotIndex = i;
                slot.SetSelected(false);
                if (i >= items.Count) slot.SetEmpty();
                else if (_tab == Tab.Relic) slot.SetRelic(items[i]);
                else slot.SetSkill(items[i]);
                _grid.Add(slot);
            }

            // 첫 칸을 골라 둠 (비어 있으면 상세도 비움)
            _selected = null;
            OnSlotClicked(items.Count > 0 ? _slots[0] : null);
        }

        void OnSlotClicked(ItemSlotWidget slot)
        {
            _selected?.SetSelected(false);
            _selected = slot;
            slot?.SetSelected(true);
            ShowDetail(slot);
        }

        void ShowDetail(ItemSlotWidget slot)
        {
            string name = "", info = "", desc = "";
            if (slot != null && slot.Kind == ItemSlotKind.Relic)
            {
                var relic = GameData.FindRelicRow(slot.ItemRow);
                if (relic != null)
                {
                    name = relic.RelicName;
                    info = $"유물 · {UiText.RelicTierLabel(relic.RelicTier)} · {UiText.SkillOwnerLabel(relic.OwnerClass)}";
                    if (relic.CanSellAtSettlement()) info += $"\n정산 판매가 {relic.SellPrice_Gold} 골드";
                    desc = UiText.FixNewlines(relic.RelicDesc);
                }
            }
            else if (slot != null && slot.Kind == ItemSlotKind.Skill)
            {
                var skill = GameData.FindSkillRow(slot.ItemRow);
                if (skill != null)
                {
                    name = skill.DisplayName_KR;
                    info = $"스킬 · {UiText.SkillTypeLabel(skill.SkillType)} · {UiText.SkillOwnerLabel(skill.OwnerClass)} · 스킬 에너지 {skill.SkillEnergyCost}";
                    desc = UiText.FixNewlines(skill.Description_KR);
                }
            }
            else
            {
                name = _tab == Tab.Relic ? "보관한 유물이 없습니다" : "보유한 스킬이 없습니다";
            }

            _detailName.text = name;
            _detailInfo.text = info;
            _detailDesc.text = desc;
        }

        void OnGoldChanged(int gold, int delta) => RefreshGold();

        void RefreshGold()
        {
            _gold.text = "보유 골드: " + LocalProfile.Service.Gold.ToString("N0", CultureInfo.InvariantCulture);
        }
    }
}
