// "칸이 가득 찼습니다. 바꿀 것을 고르세요" 패널 (언리얼 UItemPickPanelWidget + WBP_ItemPickPanel). 보상 / 이벤트 / 상점이 같이 씀
//   안내 (16, 가운데)
//   [칸][칸][칸]
//   [  취소  ]
//  - ShowEnhanceSkills: 내 강화 스킬 칸 전부. 칸 SlotIndex = 강화 칸 번호
//  - ShowRelics: 내 유물 중 filter 를 통과한 것. 고른 유물은 slot.ItemRow
//  - 칸을 누르면 Picked(slot), 취소는 Cancelled. 패널은 스스로 닫히지 않음 (쓰는 쪽이 Hide)
//
//   var pick = ItemPickPanelWidget.CreateFloating(out var host); screen.Add(host);   // 화면 가운데 떠 있는 판 (AttachFloatingPickPanel)
//   pick.Picked += s => { NetBus.Request(new FinishRoomRewardReq { ChosenSkill = row, ReplaceSlot = s.SlotIndex }); pick.Hide(); };
//   pick.ShowEnhanceSkills("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요");
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Game.Client;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class ItemPickPanelWidget : VisualElement
    {
        /// <summary>칸 크기 (0 이면 칸 기본값)</summary>
        public Vector2 SlotSize = Vector2.zero;

        /// <summary>칸 사이 간격</summary>
        public float SlotSpacing = 12f;

        readonly Label _guideText;
        readonly VisualElement _slotBox;
        readonly Button _cancelButton;
        VisualElement _floatingHost;

        public event Action<ItemSlotWidget> Picked;
        public event Action Cancelled;

        /// <summary>지금 보이는지</summary>
        public bool IsOpen { get; private set; }

        public ItemPickPanelWidget()
        {
            UiStyle.Use(this, "Common");
            AddToClassList("tc-pickpanel");

            _guideText = UiStyle.WrapText("", UiStyle.Body, UiColor.TextSub, TextAnchor.UpperCenter, true);
            _guideText.AddToClassList("tc-pickpanel__guide");
            Add(_guideText);

            _slotBox = new VisualElement { name = "SlotBox" };
            _slotBox.AddToClassList("tc-pickpanel__slots");
            Add(_slotBox);

            _cancelButton = UiStyle.FlatButton("  취소  ", () => Cancelled?.Invoke(), UiStyle.Body, FlatKind.Board);
            _cancelButton.AddToClassList("tc-pickpanel__cancel");
            Add(_cancelButton);

            SetOpen(false);
        }

        /// <summary>
        /// 화면 가운데 떠 있는 판(#1B1E25, 여백 28/22)에 담아 만듦 (AttachFloatingPickPanel).
        /// host 를 화면에 붙여 두면 패널을 열고 닫을 때 같이 보이고 숨음. host 는 화면 전체를 덮는 투명 칸 (판 밖은 클릭이 통과)
        /// </summary>
        public static ItemPickPanelWidget CreateFloating(out VisualElement host)
        {
            var panel = new ItemPickPanelWidget();
            host = new VisualElement { name = "PickHost", pickingMode = PickingMode.Ignore };
            UiStyle.Use(host, "Common");
            host.AddToClassList("tc-pickpanel-host");
            var plate = UiStyle.Panel(28f, 22f);
            plate.AddToClassList("tc-pickpanel-host__plate");
            plate.Add(panel);
            host.Add(plate);
            panel._floatingHost = host;
            panel.SetOpen(false);
            return panel;
        }

        /// <summary>안내를 바꾸고 칸을 비운 채로 염. 그 뒤 AddSlot 으로 직접 채울 때</summary>
        public void Begin(string guide)
        {
            UiStyle.SetTextIfChanged(_guideText, guide);
            _slotBox.Clear();
            SetOpen(true);
        }

        /// <summary>칸 하나 추가 (이름 보임, 누르면 Picked)</summary>
        public ItemSlotWidget AddSlot()
        {
            var slot = SlotSize != Vector2.zero ? new ItemSlotWidget(SlotSize) : new ItemSlotWidget();
            slot.SetShowName(true);
            slot.Clicked += s => Picked?.Invoke(s);
            slot.style.marginLeft = SlotSpacing * 0.5f;
            slot.style.marginRight = SlotSpacing * 0.5f;
            _slotBox.Add(slot);
            return slot;
        }

        /// <summary>내 강화 스킬 칸 전부 (run 이 없으면 ClientState.Me.Run). 칸 SlotIndex = 강화 칸 번호</summary>
        public void ShowEnhanceSkills(string guide, RunState run = null)
        {
            Begin(guide);
            run ??= ClientState.Me?.Run;
            if (run == null) return;
            var equipped = run.EnhanceSkills;
            for (int i = 0; i < equipped.Count; ++i)
            {
                var slot = AddSlot();
                slot.SlotIndex = i;
                slot.SetSkill(equipped[i]);
            }
        }

        /// <summary>내 유물 중 filter 를 통과한 것 (filter null = 전부). 칸 SlotIndex = 보유 유물 목록 번호</summary>
        public void ShowRelics(string guide, Func<RelicRow, bool> filter = null, RunState run = null)
        {
            Begin(guide);
            run ??= ClientState.Me?.Run;
            if (run == null) return;
            for (int i = 0; i < run.Relics.Count; ++i)
            {
                var relic = GameData.FindRelicRow(run.Relics[i]);
                if (relic == null || (filter != null && !filter(relic))) continue;
                var slot = AddSlot();
                slot.SlotIndex = i;
                slot.SetRelic(run.Relics[i]);
            }
        }

        /// <summary>임의 목록 (relic = 유물 칸인지, rows = 행 이름들). 칸 SlotIndex = 목록 번호</summary>
        public void ShowItems(string guide, bool relic, IReadOnlyList<string> rows)
        {
            Begin(guide);
            if (rows == null) return;
            for (int i = 0; i < rows.Count; ++i)
            {
                var slot = AddSlot();
                slot.SlotIndex = i;
                slot.SetItem(relic, rows[i]);
            }
        }

        /// <summary>취소 버튼 보이기 (기본 보임)</summary>
        public void SetCancelVisible(bool visible) => UiStyle.SetShown(_cancelButton, visible);

        public void Hide()
        {
            SetOpen(false);
            _slotBox.Clear();
        }

        void SetOpen(bool open)
        {
            IsOpen = open;
            UiStyle.SetShown(this, open);
            if (_floatingHost != null) UiStyle.SetShown(_floatingHost, open);
        }
    }
}
