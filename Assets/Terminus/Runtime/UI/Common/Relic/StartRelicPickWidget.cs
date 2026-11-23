// 런 시작 유물 고르기 (언리얼 UStartRelicPickWidget + WBP_StartRelicPick, 여는 규칙은 PC::OpenStartRelicPick)
// 시작 스킬 고르기 다음, 지도 보기 전에 화면 전체를 덮고 뜸
//
//  - 창고(보관 유물)를 6열 격자로 (최소 18칸, 빈 칸 포함). 누를 때마다 고름 / 해제. 최대 RunPlayer.MaxStartRelics 개 (기획 1개, 안 골라도 됨)
//  - 1개만 고를 때는 다른 칸을 누르면 그쪽으로 바뀜
//  - 다른 직업 전용 유물은 흐리게, 못 고름 (이벤트 / 퀘스트 유물은 누구나)
//  - 같은 유물을 여러 개 보관 중이어도 하나만
//  - 하드 모드면 "죽으면 들고 간 유물이 창고에서 사라집니다" 경고 (빨강)
//  - 시작을 누르면 ChooseStartRelicsReq -> 닫힘. 창고는 도감이라 들고 가도 그대로 (하드 모드 사망 정산 때만 지움)
//
//   StartRelicPickWidget.OpenForLocalPlayer();   // 창고가 비었으면 띄우지 않고 바로 빈 요청을 보냄 (언리얼과 같음)
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.Run;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class StartRelicPickWidget : VisualElement
    {
        public int Columns = 6;
        /// <summary>최소 칸 수 (빈 칸 포함)</summary>
        public int MinSlots = 18;
        /// <summary>고를 수 있는 최대 개수 (RunPlayer.MaxStartRelics 를 넘을 수 없음)</summary>
        public int MaxSelect = 1;
        /// <summary>격자 칸 안쪽 여백 (UniformGrid SlotPadding)</summary>
        public float SlotPadding = 4f;

        /// <summary>true 면 시작 때 ChooseStartRelicsReq 를 보냄 (기본)</summary>
        public bool SendRequest = true;

        readonly Label _titleText;
        readonly Label _modeText;
        readonly Label _countText;
        readonly ScrollView _scroll;
        readonly VisualElement _grid;
        readonly Button _startButton;

        readonly List<ItemSlotWidget> _slots = new List<ItemSlotWidget>();
        readonly List<int> _selected = new List<int>();   // 고른 순서대로 칸 번호
        CharacterClass _playerClass = CharacterClass.Fighter;
        bool _hardMode;
        bool _started;

        /// <summary>시작을 누르고 닫힌 뒤 (고른 유물 행 이름들). 서버 요청은 이미 보낸 뒤</summary>
        public event Action<List<string>> Finished;

        /// <summary>고른 것이 바뀜</summary>
        public event Action SelectionChanged;

        public bool IsHardMode => _hardMode;

        public StartRelicPickWidget()
        {
            UiStyle.Use(this, "Common", "Relic");
            AddToClassList("tc-startrelic");
            pickingMode = PickingMode.Position;   // 아래 지도를 못 누르게

            var window = new VisualElement { name = "WindowSize" };
            window.AddToClassList("tc-startrelic__window");
            Add(window);

            _titleText = UiStyle.Text("", UiStyle.Section, UiColor.Gold, TextAnchor.MiddleCenter, true);
            _titleText.AddToClassList("tc-startrelic__title");
            window.Add(_titleText);

            _modeText = UiStyle.Text("", UiStyle.Aux, UiColor.TextAux, TextAnchor.MiddleCenter, true);
            _modeText.AddToClassList("tc-startrelic__mode");
            window.Add(_modeText);

            _countText = UiStyle.Text("0 / 1", UiStyle.Body, UiColor.Gold, TextAnchor.MiddleCenter, true);
            _countText.AddToClassList("tc-startrelic__count");
            window.Add(_countText);

            _scroll = new ScrollView(ScrollViewMode.Vertical) { name = "GridScroll" };
            _scroll.AddToClassList("tc-startrelic__scroll");
            window.Add(_scroll);

            _grid = new VisualElement { name = "ItemGrid" };
            _grid.AddToClassList("tc-startrelic__grid");
            _scroll.Add(_grid);

            // 시작 = 주 행동 육각 버튼
            _startButton = UiStyle.HexButton("    시작    ", HexKind.Primary, Start);
            _startButton.AddToClassList("tc-startrelic__start");
            window.Add(_startButton);
        }

        /// <summary>
        /// 내 창고 유물로 띄움 (UILayer.Pick). 창고가 비었으면 띄우지 않고 바로 '안 고름' 요청 후 null.
        /// 이미 골랐거나 던전 밖이면 null. 화면은 시작 스킬을 고른 뒤 (Me.Run.bStartRelicsChosen == false) 한 번 부르면 됨
        /// </summary>
        public static StartRelicPickWidget OpenForLocalPlayer()
        {
            var me = ClientState.Me;
            if (me == null || me.Run == null || me.Run.bStartRelicsChosen) return null;

            var stored = new List<string>();
            foreach (var row in LocalProfile.Service.StoredRelics)
            {
                if (ProfileService.IsStorableRelic(row)) stored.Add(row);
            }
            if (stored.Count == 0)
            {
                NetBus.Request(new ChooseStartRelicsReq());
                return null;
            }

            var w = new StartRelicPickWidget();
            w.SetCandidates(stored, me.Class, ClientState.Party.bHardMode || me.Run.bHardMode);
            UIRoot.Instance?.Show(UILayer.Pick, w);
            return w;
        }

        /// <summary>창고 유물 목록, 내 직업 (직업 전용 유물 판정), 하드 모드</summary>
        public void SetCandidates(IReadOnlyList<string> storedRelics, CharacterClass playerClass, bool hardMode)
        {
            storedRelics ??= Array.Empty<string>();
            _playerClass = playerClass;
            _hardMode = hardMode;
            _started = false;
            _selected.Clear();
            MaxSelect = Mathf.Clamp(MaxSelect, 1, RunPlayer.MaxStartRelics);

            // 사용자 결정 10-08: 들고 가도 창고(도감)에 남음. 하드 모드에서 죽으면 그때 창고에서 지워짐
            _modeText.text = hardMode ? "하드 모드: 죽으면 들고 간 유물이 창고에서 사라집니다" : "들고 간 유물은 창고에 그대로 남습니다";
            _modeText.style.color = hardMode ? UiColor.Alert : UiColor.TextAux;

            _titleText.text = MaxSelect == 1
                ? "던전에 들고 갈 유물을 1개 고르세요 (안 골라도 됩니다)"
                : $"던전에 들고 갈 유물을 고르세요 (최대 {MaxSelect}개)";

            _grid.Clear();
            _slots.Clear();
            int cols = Mathf.Max(1, Columns);
            int total = (Mathf.Max(MinSlots, storedRelics.Count) + cols - 1) / cols * cols;

            // 균등 격자: 칸 자리는 가장 큰 칸(이름 보이는 유물 칸) 크기로
            var cell = Vector2.Max(ItemSlotWidget.DefaultSize, ItemSlotWidget.NamedMinSize) + Vector2.one * (SlotPadding * 2f);
            VisualElement row = null;
            for (int i = 0; i < total; ++i)
            {
                if (i % cols == 0)
                {
                    row = new VisualElement();
                    row.AddToClassList("tc-startrelic__row");
                    _grid.Add(row);
                }

                var holder = new VisualElement();
                holder.AddToClassList("tc-startrelic__cell");
                holder.style.width = cell.x;
                holder.style.height = cell.y;
                row.Add(holder);

                var slot = new ItemSlotWidget { SlotIndex = i };
                slot.Clicked += HandleSlotClicked;
                if (i < storedRelics.Count) slot.SetRelic(storedRelics[i]);
                else slot.SetEmpty();
                _slots.Add(slot);
                holder.Add(slot);
            }

            RefreshStates();
        }

        /// <summary>지금 고른 유물 (고른 순서)</summary>
        public List<string> GetChosen()
        {
            var chosen = new List<string>();
            foreach (var index in _selected)
            {
                if (index >= 0 && index < _slots.Count) chosen.Add(_slots[index].ItemRow);
            }
            return chosen;
        }

        bool IsRowSelected(string row)
        {
            foreach (var index in _selected)
            {
                if (index >= 0 && index < _slots.Count && _slots[index].ItemRow == row) return true;
            }
            return false;
        }

        /// <summary>이 직업이 들고 갈 수 있는 유물인가 (이벤트 / 퀘스트 유물은 직업 상관없이 누구나)</summary>
        public bool CanEquip(string row)
        {
            var relic = GameData.FindRelicRow(row);
            if (relic == null) return false;
            bool givenOnly = relic.OwnerClass == SkillOwner.Event || relic.OwnerClass == SkillOwner.Quest;
            return givenOnly || RunPlayer.CanClassHoldRelic(_playerClass, relic);
        }

        void RefreshStates()
        {
            bool full = _selected.Count >= MaxSelect;
            for (int i = 0; i < _slots.Count; ++i)
            {
                var slot = _slots[i];
                if (slot.Kind != ItemSlotKind.Relic) continue;
                bool selected = _selected.Contains(i);
                var row = slot.ItemRow;
                // 고른 칸은 언제나 눌러서 해제 가능. 안 고른 칸은: 장착 가능 + 같은 유물을 아직 안 골랐음 + 자리가 남음 (1개만 고르면 바꾸기 가능)
                bool usable = selected || (CanEquip(row) && !IsRowSelected(row) && (!full || MaxSelect == 1));
                slot.SetSelected(selected);
                slot.SetUsable(usable);
            }
            _countText.text = _selected.Count + " / " + MaxSelect;
        }

        void HandleSlotClicked(ItemSlotWidget slot)
        {
            if (_started || slot == null) return;
            int index = slot.SlotIndex;
            if (_selected.Contains(index))
            {
                _selected.Remove(index);
            }
            else if (CanEquip(slot.ItemRow) && !IsRowSelected(slot.ItemRow))
            {
                // 1개만 고르면 바꾸기, 여러 개면 자리가 남을 때만
                if (MaxSelect == 1)
                {
                    _selected.Clear();
                    _selected.Add(index);
                }
                else if (_selected.Count < MaxSelect)
                {
                    _selected.Add(index);
                }
            }
            RefreshStates();
            SelectionChanged?.Invoke();
        }

        void Start()
        {
            if (_started) return;
            _started = true;
            var chosen = GetChosen();
            if (SendRequest) NetBus.Request(new ChooseStartRelicsReq { RelicRows = new List<string>(chosen) });
            RemoveFromHierarchy();
            Finished?.Invoke(chosen);
        }
    }
}
