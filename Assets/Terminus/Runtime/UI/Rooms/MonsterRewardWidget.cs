// 방 클리어 보상 화면 (언리얼 UMonsterRewardWidget + WBP_MonsterReward). 몬스터 / 가디언 / 보스 공통
//  - 던전 재화: 이미 받음 -> 받은 양만 보여줌
//  - 픽업 스킬 후보 중 하나 / 유물 후보 중 하나를 각각 고를 수 있음 (안 골라도 됨)
//  - 강화 칸 / 유물 칸이 꽉 찼으면 바꿀 칸을 고름 (취소하면 후보로 돌아감). 직업 기본 유물(패시브)은 못 바꿈
//  - 고른 쪽 후보는 사라지고 결과가 남음. '다음으로' 를 누르면 서버에 알리고 닫힘. 구역의 모든 플레이어가 누르면 지도로
// WBP 배치: 화면 전체 어둡게(0.75) + 가운데 세로 줄 [제목 / 재화 / (스킬 | 유물) / 바꾸기 패널 2개 / 결과 / 다음으로]
using Terminus.Data;
using Terminus.Game.Net;
using Terminus.Map;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    public sealed class MonsterRewardWidget : VisualElement
    {
        const float CardSpacing = 24f;

        readonly Label _titleText;
        readonly Label _currencyText;
        readonly VisualElement _skillSection;
        readonly VisualElement _cardBox;
        readonly VisualElement _relicSection;
        readonly VisualElement _relicBox;
        readonly ItemPickPanelWidget _replacePanel;
        readonly ItemPickPanelWidget _relicReplacePanel;
        readonly Label _resultText;
        readonly Button _nextButton;

        RoomRewardOffer _offer = new RoomRewardOffer();

        // 스킬
        string _chosenSkill = "";
        string _pendingSkill = "";   // 카드를 골랐는데 바꿀 칸을 고르는 중
        int _replaceSlot = -1;

        // 유물
        string _chosenRelic = "";
        string _pendingRelic = "";
        string _replaceRelic = "";

        bool _finished;

        public MonsterRewardWidget()
        {
            RoomLooks.Use(this, "MonsterReward");
            AddToClassList("rp-root");
            AddToClassList("mr-root");
            pickingMode = PickingMode.Position;   // 아래 화면(전투 HUD)을 못 누르게

            // [1] RewardDim (검정 0.75, 화면 전체)
            var dim = new VisualElement { name = "RewardDim" };
            dim.AddToClassList("mr-dim");
            Add(dim);

            // [2] ContentBox (가운데, 내용 크기)
            var content = new VisualElement { name = "ContentBox", pickingMode = PickingMode.Ignore };
            content.AddToClassList("mr-content");
            Add(content);

            // [3] TitleText 40 (0.584078, 0.371238, 0.107023) = #C9A45C
            _titleText = UiStyle.Text("전투 승리!", 40, UiColor.Gold, TextAnchor.MiddleCenter, true);
            _titleText.name = "TitleText";
            content.Add(_titleText);

            // [4] CurrencyText 17 (0.838799, 0.768151, 0.623960) = #ECE3CF, 위 10
            _currencyText = UiStyle.Text("던전 재화 +0", 17, UiColor.TextBody, TextAnchor.MiddleCenter, true);
            _currencyText.name = "CurrencyText";
            _currencyText.AddToClassList("mr-currency");
            content.Add(_currencyText);

            // [5] SectionsRow (위 24): 스킬 | 유물
            var sections = new VisualElement { name = "SectionsRow", pickingMode = PickingMode.Ignore };
            sections.AddToClassList("mr-sections");
            content.Add(sections);

            _skillSection = MakeSection("SkillSection", "픽업 스킬", out _cardBox);
            _cardBox.name = "CardBox";
            sections.Add(_skillSection);
            _relicSection = MakeSection("RelicSection", "유물", out _relicBox);
            _relicBox.name = "RelicBox";
            sections.Add(_relicSection);

            // [12] ReplacePanel / [13] RelicReplacePanel (위 24, 평소 숨김)
            _replacePanel = new ItemPickPanelWidget { name = "ReplacePanel" };
            _replacePanel.AddToClassList("mr-replace");
            _replacePanel.Picked += HandleReplaceSlotClicked;
            _replacePanel.Cancelled += HandleReplaceCancel;
            content.Add(_replacePanel);

            _relicReplacePanel = new ItemPickPanelWidget { name = "RelicReplacePanel" };
            _relicReplacePanel.AddToClassList("mr-replace");
            _relicReplacePanel.Picked += HandleRelicReplaceClicked;
            _relicReplacePanel.Cancelled += HandleRelicReplaceCancel;
            content.Add(_relicReplacePanel);

            // [14] ResultText 17 #ECE3CF (위 24)
            _resultText = UiStyle.Text("받을 보상을 고르세요 (안 받아도 됩니다)", 17, UiColor.TextBody, TextAnchor.MiddleCenter, true);
            _resultText.name = "ResultText";
            _resultText.AddToClassList("mr-result");
            content.Add(_resultText);

            // [15] NextButton (육각 주 행동 280x56, 위 24) "   다음으로   "
            _nextButton = UiStyle.HexButton("   다음으로   ", HexKind.Primary, HandleNextClicked);
            _nextButton.name = "NextButton";
            _nextButton.AddToClassList("mr-next");
            content.Add(_nextButton);
        }

        static VisualElement MakeSection(string name, string title, out VisualElement box)
        {
            var section = new VisualElement { name = name, pickingMode = PickingMode.Ignore };
            section.AddToClassList("mr-section");
            // 섹션 제목 17 (0.386429, 0.346704, 0.270498) = #A79F8E
            var titleText = UiStyle.Text(title, 17, UiColor.TextAux, TextAnchor.MiddleCenter, true);
            section.Add(titleText);
            box = new VisualElement { pickingMode = PickingMode.Ignore };
            box.AddToClassList("mr-cardbox");
            section.Add(box);
            return section;
        }

        // =====================================================================
        // 내용
        // =====================================================================

        public void Setup(RoomRewardOffer offer)
        {
            _offer = offer ?? new RoomRewardOffer();
            _chosenSkill = _pendingSkill = "";
            _chosenRelic = _pendingRelic = _replaceRelic = "";
            _replaceSlot = -1;
            _finished = false;

            _titleText.text = _offer.RoomType == RoomType.BOSS ? "보스 처치!"
                : _offer.RoomType == RoomType.GUARDIAN ? "가디언 처치!"
                : "전투 승리!";
            _currencyText.text = "던전 재화 +" + _offer.Currency;
            UiStyle.SetShown(_currencyText, _offer.Currency > 0);

            BuildSkillCards();
            BuildRelicCards();
            Refresh();
        }

        void BuildSkillCards()
        {
            _cardBox.Clear();
            foreach (var row in _offer.SkillOffers)
            {
                var card = new SkillCardWidget();
                card.SetSkill(row);
                card.Clicked += HandleCardClicked;
                card.style.marginLeft = CardSpacing * 0.5f;
                card.style.marginRight = CardSpacing * 0.5f;
                _cardBox.Add(card);
            }
        }

        void BuildRelicCards()
        {
            _relicBox.Clear();
            foreach (var row in _offer.RelicOffers)
            {
                var relic = GameData.FindRelicRow(row);
                if (relic == null) continue;

                var card = new ItemCardWidget();
                card.SetCaptionStyle(220f, 14, UiColor.TextBody);
                card.SetItem(true, row, new Vector2(120f, 120f));
                card.SetCaption(UiText.FixNewlines(relic.RelicDesc));
                card.SetAction("", false);
                if (card.Slot != null) card.Slot.Clicked += HandleRelicClicked;
                card.style.marginLeft = CardSpacing * 0.5f;
                card.style.marginRight = CardSpacing * 0.5f;
                card.style.alignSelf = Align.FlexStart;
                _relicBox.Add(card);
            }
        }

        // 섹션 / 바꾸기 패널 / 결과 표시
        void Refresh()
        {
            bool skillReplacing = _pendingSkill.Length > 0;
            bool relicReplacing = _pendingRelic.Length > 0;
            bool replacing = skillReplacing || relicReplacing;

            // 후보는 아직 안 골랐고 바꾸기 중이 아닐 때만
            UiStyle.SetShown(_skillSection, !replacing && _chosenSkill.Length == 0 && _offer.SkillOffers.Count > 0);
            UiStyle.SetShown(_relicSection, !replacing && _chosenRelic.Length == 0 && _offer.RelicOffers.Count > 0);

            if (!skillReplacing && _replacePanel.IsOpen) _replacePanel.Hide();
            if (!relicReplacing && _relicReplacePanel.IsOpen) _relicReplacePanel.Hide();

            var lines = "";
            if (_chosenSkill.Length > 0)
            {
                lines += "획득: " + GameData.GetSkillName(_chosenSkill) + (_replaceSlot >= 0 ? $"  (강화 칸 {_replaceSlot + 1} 교체)" : "");
            }
            if (_chosenRelic.Length > 0)
            {
                if (lines.Length > 0) lines += "\n";
                lines += "획득: " + GameData.GetRelicName(_chosenRelic) + (_replaceRelic.Length > 0 ? $"  ({GameData.GetRelicName(_replaceRelic)} 버림)" : "");
            }

            bool anyLeft = (_chosenSkill.Length == 0 && _offer.SkillOffers.Count > 0) || (_chosenRelic.Length == 0 && _offer.RelicOffers.Count > 0);
            if (replacing) lines = "";
            else if (lines.Length == 0) lines = anyLeft ? "받을 보상을 고르세요 (안 받아도 됩니다)" : "받을 수 있는 보상이 없습니다";
            _resultText.text = lines;
            UiStyle.SetShown(_resultText, lines.Length > 0);
        }

        // =====================================================================
        // 스킬
        // =====================================================================

        void HandleCardClicked(string skillRow)
        {
            if (_finished || _chosenSkill.Length > 0 || _pendingRelic.Length > 0) return;

            // 칸이 꽉 찼으면 바꿀 칸부터
            if (RoomLooks.EnhanceSlotsFull())
            {
                _pendingSkill = skillRow;
                _replacePanel.ShowEnhanceSkills("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요");
            }
            else
            {
                _chosenSkill = skillRow;
            }
            Refresh();
        }

        void HandleReplaceSlotClicked(ItemSlotWidget slot)
        {
            if (_finished || _pendingSkill.Length == 0 || slot == null) return;
            _chosenSkill = _pendingSkill;
            _pendingSkill = "";
            _replaceSlot = slot.SlotIndex;
            Refresh();
        }

        void HandleReplaceCancel()
        {
            _pendingSkill = "";
            Refresh();
        }

        // =====================================================================
        // 유물
        // =====================================================================

        void HandleRelicClicked(ItemSlotWidget slot)
        {
            if (_finished || slot == null || _chosenRelic.Length > 0 || _pendingSkill.Length > 0) return;

            var row = slot.ItemRow;
            // 칸이 꽉 찼으면 버릴 유물부터 (직업 기본 유물(패시브)은 못 버림)
            if (RoomLooks.RelicSlotsFull())
            {
                _pendingRelic = row;
                _relicReplacePanel.ShowRelics("유물 칸이 가득 찼습니다. 버릴 유물을 고르세요", relic => relic.RelicTier != RelicTier.Basic);
            }
            else
            {
                _chosenRelic = row;
            }
            Refresh();
        }

        void HandleRelicReplaceClicked(ItemSlotWidget slot)
        {
            if (_finished || _pendingRelic.Length == 0 || slot == null) return;
            _chosenRelic = _pendingRelic;
            _pendingRelic = "";
            _replaceRelic = slot.ItemRow;
            Refresh();
        }

        void HandleRelicReplaceCancel()
        {
            _pendingRelic = "";
            Refresh();
        }

        // =====================================================================
        // 마침
        // =====================================================================

        void HandleNextClicked()
        {
            if (_finished) return;
            _finished = true;

            NetBus.Request(new FinishRoomRewardReq
            {
                ChosenSkill = _chosenSkill,
                ReplaceSlot = _replaceSlot,
                ChosenRelic = _chosenRelic,
                ReplaceRelic = _replaceRelic,
            });

            UiTooltip.Hide();
            RemoveFromHierarchy();
        }
    }
}
