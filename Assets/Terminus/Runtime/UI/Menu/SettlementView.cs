// 정산 (언리얼 USettlementWidget + WBP_Settlement). UILayer.Settlement
// 런이 끝나면 MenuFlow 가 정산 대기(PendingSettlement)를 바로 내 프로필에 저장하고 메인 메뉴로 -> 메인 메뉴가 이 화면을 띄움
// (방장이 먼저 나가 끊기거나 게임이 꺼져도 다음 메인 화면에서 이어서 정산)
//  1. 유물 보관 / 판매: 누를 때마다 보관 <-> 판매 (여러 개 가능, 하나도 안 골라도 됨 = 전부 판매)
//  2. 완료된 퀘스트 (언리얼도 글만)
//  3. 가져가는 스킬 (전멸이면 없음)
//  정산 완료 -> SettlementRules.Confirm -> 최종 골드 요약 -> 확인으로 닫기
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class SettlementView : VisualElement
    {
        // 유물 칸 크기 (USettlementWidget RelicSlotSize)
        static readonly Vector2 RelicSlotSize = new Vector2(84f, 84f);

        readonly PendingSettlement _settlement;
        readonly List<string> _kept = new List<string>();

        readonly Label _title;
        readonly VisualElement _mainPanel;
        readonly VisualElement _relicList;
        readonly Label _relicGuide;
        readonly Button _confirm;
        readonly VisualElement _summaryPanel;
        readonly Label _summaryText;

        /// <summary>정산을 끝내고 닫았음 (다시 띄우려면 새로 만듦)</summary>
        public bool IsFinished { get; private set; }

        public SettlementView()
        {
            MenuStyle.Use(this, "Menu", "MenuWindows");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;   // 아래 메인 메뉴를 못 누르게

            _settlement = LocalProfile.Service.GetPendingSettlement();

            // [1] 어둡게 (0.75) + [2] 가운데 판 (안쪽 30 / 24, 폭 760)
            var dim = MenuStyle.FullScreen(new Color(0f, 0f, 0f, 0.75f));
            dim.style.alignItems = Align.Center;
            dim.style.justifyContent = Justify.Center;
            Add(dim);
            var panel = MenuStyle.RoundPanel(1f, 6f);
            UiStyle.Padding(panel, 30, 24);
            dim.Add(panel);
            var root = UiStyle.Column();
            root.style.width = 760;
            panel.Add(root);

            // [5] 제목 / [6] 부제 (이유 + 던전 이름 · 층 + 보너스 골드)
            _title = MenuStyle.Wrap(_settlement.bDeath ? "사망 정산" : "정산", UiStyle.ScreenTitle, UiColor.Gold, true, TextAnchor.UpperCenter);
            _title.style.letterSpacing = 3f;
            root.Add(_title);

            var sub = _settlement.Reason ?? "";
            if (!string.IsNullOrEmpty(_settlement.RoomName) || _settlement.Floor > 0)
            {
                sub += $"  ({_settlement.RoomName}{(string.IsNullOrEmpty(_settlement.RoomName) ? "" : " · ")}{_settlement.Floor}층)";
            }
            if (_settlement.BonusGold > 0) sub += $"\n{_settlement.BonusReason}  +{UiText.GoldText(_settlement.BonusGold)}";
            var subtitle = MenuStyle.Wrap(sub, 16, UiColor.TextAux, true, TextAnchor.UpperCenter);
            subtitle.style.marginTop = 6;
            root.Add(subtitle);

            // ---- [7] 정산 목록
            _mainPanel = UiStyle.Column();
            _mainPanel.style.marginTop = 18;
            root.Add(_mainPanel);

            _mainPanel.Add(Header("1. 유물 보관 / 판매", 0));
            var help = "보관할 유물을 눌러 고르세요 (여러 개 가능). 고르지 않은 유물은 자동으로 판매되어 골드가 됩니다.";
            if (_settlement.LostStoredRelics.Count > 0)
            {
                var names = new List<string>();
                foreach (var row in _settlement.LostStoredRelics) names.Add(GameData.GetRelicName(row));
                help += $"\n하드 모드: 들고 간 [{string.Join(", ", names)}] 이(가) 창고에서 사라집니다.";
            }
            var helpText = MenuStyle.Wrap(help, 13, UiColor.TextFaint, true);
            helpText.style.marginTop = 4;
            _mainPanel.Add(helpText);

            var scroll = new ScrollView(ScrollViewMode.Vertical);
            scroll.AddToClassList("tm-scroll");
            scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            scroll.style.maxHeight = 300;
            scroll.style.marginTop = 10;
            _mainPanel.Add(scroll);
            _relicList = new VisualElement();
            _relicList.style.flexDirection = FlexDirection.Row;
            _relicList.style.flexWrap = Wrap.Wrap;
            scroll.Add(_relicList);

            _relicGuide = MenuStyle.Wrap("", 16, UiColor.Gold, true);
            _relicGuide.style.marginTop = 8;
            _mainPanel.Add(_relicGuide);

            _mainPanel.Add(Header("2. 완료된 퀘스트", 18));
            var questText = MenuStyle.Wrap("완료한 퀘스트가 없습니다.", 15, UiColor.TextSub, true);
            questText.style.marginTop = 4;
            _mainPanel.Add(questText);

            _mainPanel.Add(Header("3. 가져가는 스킬", 18));
            var skill = Ids.IsNone(_settlement.KeptSkill) ? null : GameData.FindSkillRow(_settlement.KeptSkill);
            var skillText = MenuStyle.Wrap(_settlement.bDeath
                    ? "전멸해서 강화 스킬과 던전 재화는 모두 사라졌습니다."
                    : skill != null
                        ? $"[{skill.DisplayName_KR}] 을(를) 보유 스킬로 가져갑니다."
                        : "장착 중인 픽업 스킬을 이미 모두 보유하고 있어 건너뜁니다.",
                15, UiColor.TextSub, true);
            skillText.style.marginTop = 4;
            _mainPanel.Add(skillText);

            _confirm = UiStyle.HexButton("정산 완료", HexKind.Primary, OnConfirm, UiStyle.Body);
            _confirm.style.marginTop = 22;
            _confirm.style.alignSelf = Align.FlexEnd;
            _mainPanel.Add(_confirm);

            // ---- [20] 최종 골드
            _summaryPanel = UiStyle.Column();
            _summaryPanel.style.marginTop = 18;
            _summaryPanel.style.display = DisplayStyle.None;
            root.Add(_summaryPanel);
            _summaryPanel.Add(MenuStyle.Wrap("최종 골드", 22, UiColor.TextBody, true, TextAnchor.UpperCenter));
            _summaryText = MenuStyle.Wrap("", 17, UiColor.TextBody, true, TextAnchor.UpperCenter);
            _summaryText.style.marginTop = 10;
            _summaryPanel.Add(_summaryText);
            var close = MenuStyle.FlatButton("확인", OnClose);
            close.style.marginTop = 18;
            close.style.alignSelf = Align.Center;
            _summaryPanel.Add(close);

            RebuildRelics();
            RefreshGuide();
        }

        static Label Header(string text, float top)
        {
            var l = MenuStyle.Wrap(text, 22, UiColor.TextBody, true);
            l.style.marginTop = top;
            return l;
        }

        // ------------------------------------------------------------ 유물

        void RebuildRelics()
        {
            _relicList.Clear();
            for (int i = 0; i < _settlement.Relics.Count; ++i)
            {
                var slot = new ItemSlotWidget(RelicSlotSize) { SlotIndex = i };
                slot.SetShowName(true);
                slot.SetRelic(_settlement.Relics[i]);
                slot.SetSelected(_kept.Contains(_settlement.Relics[i]));
                slot.style.marginRight = 8;
                slot.style.marginBottom = 8;
                slot.Clicked += OnRelicClicked;
                _relicList.Add(slot);
            }
        }

        void OnRelicClicked(ItemSlotWidget slot)
        {
            if (slot == null || slot.SlotIndex < 0 || slot.SlotIndex >= _settlement.Relics.Count) return;
            // 누를 때마다 보관 <-> 판매
            SettlementRules.ToggleKept(_kept, _settlement.Relics[slot.SlotIndex]);
            RebuildRelics();
            RefreshGuide();
        }

        void RefreshGuide()
        {
            string guide;
            if (_settlement.Relics.Count == 0)
            {
                guide = "던전에서 얻은 유물이 없습니다.";
            }
            else
            {
                guide = $"보관 {_kept.Count}개 · 판매 {_settlement.Relics.Count - _kept.Count}개  +{UiText.GoldText(SettlementRules.SaleGold(_settlement, _kept))}";

                // 창고에 이미 있는 걸 보관하면 아무 일도 없음 -> 파는 게 이득
                var dup = SettlementRules.AlreadyStored(LocalProfile.Service, _kept);
                if (dup.Count > 0)
                {
                    var names = dup.ConvertAll(GameData.GetRelicName);
                    guide += $"\n[{string.Join(", ", names)}] 은(는) 창고에 이미 있어 보관해도 아무 일도 없습니다. 판매하면 골드를 받습니다.";
                }
            }
            _relicGuide.text = guide;
            // 하나도 안 골라도 됨 (전부 판매)
            _confirm.SetEnabled(true);
        }

        // ------------------------------------------------------------ 완료

        void OnConfirm()
        {
            var profile = LocalProfile.Service;
            var bonusReason = _settlement.BonusReason;
            var result = SettlementRules.Confirm(profile, _kept);
            if (result == null)
            {
                OnClose();
                return;
            }

            var summary = $"유물 판매  +{UiText.GoldText(result.RelicGold)}";
            if (result.BonusGold > 0) summary = $"{bonusReason}  +{UiText.GoldText(result.BonusGold)}\n" + summary;
            summary += $"\n\n보유 골드  {UiText.GoldText(result.GoldAfter)}";
            if (result.NewRelics.Count > 0) summary += $"\n\n창고에 보관: {string.Join(", ", result.NewRelics.ConvertAll(GameData.GetRelicName))}";
            if (!string.IsNullOrEmpty(result.NewSkill)) summary += $"\n새 보유 스킬: {GameData.GetSkillName(result.NewSkill)}";
            _summaryText.text = summary;

            _mainPanel.style.display = DisplayStyle.None;
            _summaryPanel.style.display = DisplayStyle.Flex;
        }

        void OnClose()
        {
            IsFinished = true;
            RemoveFromHierarchy();
            // 정산은 메인 메뉴에서 함. 혹시 방 안에서 띄웠으면 나감
            if (NetBus.IsConnected) MenuFlow.LeaveToMenu();
        }
    }
}
