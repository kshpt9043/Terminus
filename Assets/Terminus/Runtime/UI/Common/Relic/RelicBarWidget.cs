// 내 보유 유물 줄 (언리얼 URelicBarWidget + WBP_RelicBar). 지도 화면 / 전투 HUD 의 오른쪽 아래
//              유물 3   <- CountText (13, 오른쪽)
//  [ (아이콘)(아이콘)(아이콘) ]  <- RelicTray (#23202A 판, 유물이 없으면 숨김)
// 칸은 ItemSlotWidget 을 이름 없이 40x40. 올리면 유물 툴팁
// autoBind(기본) 면 ClientState.PartyChanged 로 내 RunState.Relics 가 바뀔 때마다 다시 그림. 아니면 SetRelics 로 직접
//
//   bar = new RelicBarWidget(); hud.Add(bar);   // 끝
using System;
using System.Collections.Generic;
using Terminus.Game.Client;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class RelicBarWidget : VisualElement
    {
        /// <summary>칸 크기 / 간격</summary>
        public float IconSize = 40f;
        public float Spacing = 4f;

        readonly Label _countText;
        readonly VisualElement _tray;
        readonly VisualElement _relicBox;
        readonly List<ItemSlotWidget> _slots = new List<ItemSlotWidget>();
        readonly List<string> _shown = new List<string>();
        readonly bool _autoBind;
        bool _shownOnce;

        /// <summary>유물 칸을 누름 (보통은 안 씀)</summary>
        public event Action<ItemSlotWidget> SlotClicked;

        /// <summary>지금 그려진 유물 칸들</summary>
        public IReadOnlyList<ItemSlotWidget> Slots => _slots;

        public RelicBarWidget(bool autoBind = true)
        {
            UiStyle.Use(this, "Common", "Relic");
            AddToClassList("tc-relicbar");
            pickingMode = PickingMode.Ignore;
            _autoBind = autoBind;

            // WBP CountText (선형 0.391572, 0.346704, 0.262251) = #A89F8C
            _countText = UiStyle.Text("유물 0", UiStyle.Aux, UiColor.FromLinear(0.391572f, 0.346704f, 0.262251f), TextAnchor.MiddleRight, true);
            _countText.AddToClassList("tc-relicbar__count");
            Add(_countText);

            _tray = new VisualElement { name = "RelicTray", pickingMode = PickingMode.Ignore };
            _tray.AddToClassList("tc-relicbar__tray");
            Add(_tray);

            _relicBox = new VisualElement { name = "RelicBox", pickingMode = PickingMode.Ignore };
            _relicBox.AddToClassList("tc-relicbar__box");
            _tray.Add(_relicBox);

            if (_autoBind)
            {
                RegisterCallback<AttachToPanelEvent>(_ =>
                {
                    ClientState.PartyChanged -= OnPartyChanged;
                    ClientState.PartyChanged += OnPartyChanged;
                    OnPartyChanged();
                });
                RegisterCallback<DetachFromPanelEvent>(_ => ClientState.PartyChanged -= OnPartyChanged);
            }

            SetRelics(Array.Empty<string>());
        }

        void OnPartyChanged()
        {
            var me = ClientState.Me;
            SetRelics(me?.Run?.Relics ?? (IReadOnlyList<string>)Array.Empty<string>());
        }

        /// <summary>보여줄 유물 (DT_Relic 행 이름들). 같은 목록이면 다시 그리지 않음</summary>
        public void SetRelics(IReadOnlyList<string> relics)
        {
            relics ??= Array.Empty<string>();
            if (_shownOnce && SameAsShown(relics)) return;
            _shownOnce = true;
            _shown.Clear();
            for (int i = 0; i < relics.Count; ++i) _shown.Add(relics[i]);

            UiStyle.SetTextIfChanged(_countText, "유물 " + relics.Count);
            UiStyle.SetShown(_tray, relics.Count > 0);

            _relicBox.Clear();
            // 칸 위젯은 모자랄 때만 더 만들고 재사용
            while (_slots.Count < relics.Count)
            {
                var slot = new ItemSlotWidget(new Vector2(IconSize, IconSize));
                slot.SetShowName(false);
                slot.Clicked += s => SlotClicked?.Invoke(s);
                _slots.Add(slot);
            }
            for (int i = 0; i < relics.Count; ++i)
            {
                var slot = _slots[i];
                slot.SetSlotSize(new Vector2(IconSize, IconSize));
                slot.SlotIndex = i;
                slot.SetRelic(relics[i]);
                slot.style.marginLeft = i == 0 ? 0f : Spacing;
                _relicBox.Add(slot);
            }
        }

        bool SameAsShown(IReadOnlyList<string> relics)
        {
            if (relics.Count != _shown.Count) return false;
            for (int i = 0; i < relics.Count; ++i)
            {
                if (relics[i] != _shown[i]) return false;
            }
            return true;
        }
    }
}
