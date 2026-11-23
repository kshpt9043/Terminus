// 이벤트 화면 (언리얼 UEventWidget + WBP_EventWidget). 퀘스트 방 제안도 이 화면을 같이 씀 (서버가 ShowEventMsg 로 보냄)
//  - 선택지 카드 중 하나를 고름 (못 고르는 카드는 이유를 설명 아래에)
//  - 받을 스킬인데 강화 칸이 꽉 찼으면 바꿀 칸부터 / 받을 유물인데 유물 칸이 꽉 찼으면 버릴 유물부터 (기본 / 업그레이드 유물은 못 버림)
//  - 고르면 "..." -> 서버가 결과(EventResultMsg)를 보냄 -> 결과 글만 남김. 닫는 건 서버 (CloseEventMsg)
// WBP 배치: 화면 전체 그림(EventBackground) + 가운데 기준 제목(위 -356) / 안내(위 -260) / 카드 줄(가운데). 칸 고르기 판은 WBP 에 없어서 가운데 떠 있는 판
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    public sealed class EventWidget : VisualElement
    {
        // 선택지 카드 크기 (언리얼 CardSize 260x340)
        static readonly Vector2 CardSize = new Vector2(260f, 340f);

        readonly Label _titleText;
        readonly Label _messageText;
        readonly VisualElement _cardBox;
        readonly ItemPickPanelWidget _pickPanel;
        readonly VisualElement _pickHost;

        readonly List<EventOption> _options = new List<EventOption>();
        string _situation = "";
        int _pendingIndex = -1;   // 칸을 고르는 중인 선택지
        bool _pickingSkillSlot;
        bool _chosen;

        public EventWidget()
        {
            RoomLooks.Use(this, "Event");
            AddToClassList("rp-root");
            pickingMode = PickingMode.Position;   // 아래 화면을 못 누르게

            // [1] Image_0 (화면 전체)
            Add(RoomLooks.FullImage("/Game/JSY/Widget/Temp_Icons/EventBackground", "BackgroundImage"));

            // [2] TitleText (가운데 기준 위 -356.5, 가운데 정렬) 40 #C9A45C
            _titleText = UiStyle.Text("이벤트", 40, UiColor.Gold, TextAnchor.UpperCenter, true);
            _titleText.name = "TitleText";
            _titleText.AddToClassList("ev-title");
            Add(_titleText);

            // [3] MessageText (가운데 기준 위 -260.5) 17 Regular #C9C1AE. WBP 에 SituationText 가 없어서 상황 설명도 여기에
            _messageText = UiStyle.WrapText("", 17, UiColor.TextSub, TextAnchor.UpperCenter);
            _messageText.name = "MessageText";
            _messageText.AddToClassList("ev-message");
            Add(_messageText);

            // [4] CardBox (가운데)
            _cardBox = new VisualElement { name = "CardBox", pickingMode = PickingMode.Ignore };
            _cardBox.AddToClassList("ev-cards");
            Add(_cardBox);

            // WBP 에 PickPanel 이 없어서 C++ 이 붙인 판 (가운데 떠 있음). 유물 칸이 가득 찬 채 이벤트 유물을 고르면 멈추던 문제
            _pickPanel = ItemPickPanelWidget.CreateFloating(out _pickHost);
            _pickPanel.Picked += HandlePicked;
            _pickPanel.Cancelled += HandlePickCancel;
            Add(_pickHost);
        }

        public void Setup(string title, string situation, IReadOnlyList<EventOption> options)
        {
            _options.Clear();
            if (options != null) _options.AddRange(options);
            _situation = situation ?? "";
            _pendingIndex = -1;
            _chosen = false;

            _titleText.text = string.IsNullOrEmpty(title) ? "이벤트" : title;

            bool any = _options.Exists(o => o.bAvailable);
            SetMessage(any ? "하나를 고르세요." : "아무 일도 일어나지 않았습니다.");
            _pickPanel.Hide();
            RebuildCards();
        }

        /// <summary>서버가 알려 준 결과 ("획득: 은빛 방패")</summary>
        public void ShowResult(string result)
        {
            UiStyle.SetShown(_cardBox, false);
            _messageText.text = result ?? "";   // 결과 글만 보이게
        }

        // WBP 에 SituationText 가 없으면 상황 설명을 안내 위에 같이
        void SetMessage(string message)
        {
            _messageText.text = string.IsNullOrEmpty(_situation) ? message : _situation + "\n\n" + message;
        }

        void RebuildCards()
        {
            _cardBox.Clear();
            UiStyle.SetShown(_cardBox, _pendingIndex < 0);

            for (int i = 0; i < _options.Count; ++i)
            {
                var option = _options[i];
                int index = i;
                var card = new ChoiceCardWidget(new ChoiceCardStyle
                {
                    Size = CardSize,
                    TitleSize = 20,
                    PaddingH = 20f,
                    PaddingV = 20f,
                });
                card.SetChoiceId(option.Outcome.ToString());
                // 못 고르는 이유는 설명 아래에 (WBP 카드엔 아래 줄이 없을 수 있어서 설명에 붙임)
                var desc = UiText.FixNewlines(option.Description);
                card.SetTexts(option.Title, option.bAvailable ? desc : desc + "\n\n(" + option.UnavailableReason + ")");
                // 받을 스킬 / 유물이 정해져 있으면 카드에 올렸을 때 툴팁 (무작위 교체 유물은 미리 안 보여 줌)
                if (option.GivesSkill()) card.SetItem(false, option.SkillRow);
                else card.SetItem(true, option.Outcome == EventOutcomeType.SwapRelicSameTierRandom || option.Outcome == EventOutcomeType.SwapRelicHigherTierRandom ? "" : option.RelicRow);
                card.SetChoiceEnabled(!_chosen && option.bAvailable);
                card.Chosen += _ => HandleCardClicked(index);
                card.style.marginLeft = 10f;
                card.style.marginRight = 10f;
                _cardBox.Add(card);
            }
        }

        void HandleCardClicked(int index)
        {
            if (_chosen || index < 0 || index >= _options.Count || !_options[index].bAvailable) return;

            var option = _options[index];
            // 스킬인데 강화 칸이 꽉 찼으면 바꿀 칸부터 / 유물인데 유물 칸이 꽉 찼으면 버릴 유물부터
            bool needSkillSlot = option.GivesSkill() && RoomLooks.EnhanceSlotsFull();
            bool needRelicSlot = option.GivesRelic() && RoomLooks.RelicSlotsFull();
            if (needSkillSlot || needRelicSlot)
            {
                _pendingIndex = index;
                ShowPick(option, needSkillSlot);
                RebuildCards();
                return;
            }

            Send(index, -1, "");
        }

        void ShowPick(EventOption option, bool skillSlot)
        {
            _pickingSkillSlot = skillSlot;
            if (!skillSlot)
            {
                // 기본 / 업그레이드 유물은 버릴 수 없음
                _pickPanel.ShowRelics($"유물 칸이 가득 찼습니다. [{GameData.GetRelicName(option.RelicRow)}] 대신 버릴 유물을 고르세요.",
                    relic => relic.RelicTier != RelicTier.Basic && relic.RelicTier != RelicTier.Upgrade);
                return;
            }
            _pickPanel.ShowEnhanceSkills("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요.");
        }

        void HandlePicked(ItemSlotWidget slot)
        {
            if (_chosen || _pendingIndex < 0 || slot == null) return;
            if (_pickingSkillSlot) Send(_pendingIndex, slot.SlotIndex, "");
            else Send(_pendingIndex, -1, slot.ItemRow);
        }

        void HandlePickCancel()
        {
            _pendingIndex = -1;
            _pickPanel.Hide();
            RebuildCards();
        }

        void Send(int index, int replaceSlot, string relicRow)
        {
            _chosen = true;
            _pendingIndex = -1;
            _pickPanel.Hide();
            SetMessage("...");
            RebuildCards();

            NetBus.Request(new ChooseEventReq { Index = index, ReplaceSlot = replaceSlot, RelicRow = relicRow ?? "" });
        }
    }
}
