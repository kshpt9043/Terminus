// 다른 방 동료가 전멸했을 때 (언리얼 URescueWidget + WBP_Rescue, 기획 사망 순서도). 살아남은 사람은 구출 / 난입 투표, 전멸한 사람은 기다림
//  - 구출: 내 체력이 HealthCost 만큼 줄고, 쓰러진 동료를 체력 1 로 데리고 다음 방으로 (보상 없음)
//  - 난입: 전멸한 방에 들어가 남은 몬스터와 싸움 (몬스터 체력 그대로, 이기면 보상 + 쓰러진 동료 체력 1)
//  - 다수결, 동점이면 랜덤. 시간 안에 안 고르면 기권. 결과가 나오면 서버가 닫음 (CloseRescueMsg)
// WBP 배치: 화면 전체 어둡게(0.7) + 가운데 판 [제목 / 안내 / 남은 시간 / 선택지 2개(520 폭, 최소 74)]
using System.Collections.Generic;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    public sealed class RescueWidget : VisualElement
    {
        readonly Label _titleText;
        readonly Label _messageText;
        readonly Label _timerText;
        readonly VisualElement _optionBox;
        readonly Button _rescueButton;
        readonly Label _rescueDescText;
        readonly Button _interveneButton;
        readonly Label _interveneDescText;

        bool _chooser;
        bool _chosen;
        bool _canIntervene = true;
        float _timeLeft;
        float _lastTick = -1f;

        public RescueWidget()
        {
            RoomLooks.Use(this, "Rescue");
            AddToClassList("rp-root");
            pickingMode = PickingMode.Position;   // 아래 화면을 못 누르게

            // [1] RescueDim (검정 0.7)
            var dim = new VisualElement { name = "RescueDim" };
            dim.AddToClassList("rescue-dim");
            Add(dim);

            // [2] RescuePanel (가운데, 여백 30 / 24, #1B1E25 + #3A3F4A 테두리, 모서리 6)
            var panel = new VisualElement { name = "RescuePanel" };
            panel.AddToClassList("rescue-panel");
            Add(panel);

            // [4] TitleText 40 (0.539479, 0.043735, 0.033105) = #C23B33
            _titleText = UiStyle.WrapText("동료 전멸", 40, UiColor.Alert, TextAnchor.UpperCenter, true);
            _titleText.name = "TitleText";
            panel.Add(_titleText);

            // [5] MessageText 16 #ECE3CF (위 10)
            _messageText = UiStyle.WrapText("", 16, UiColor.TextBody, TextAnchor.UpperCenter, true);
            _messageText.name = "MessageText";
            _messageText.AddToClassList("rescue-message");
            panel.Add(_messageText);

            // [6] TimerText 16 (0.386429, 0.346704, 0.270498) = #A79F8E (위 6)
            _timerText = UiStyle.WrapText("", 16, UiColor.TextAux, TextAnchor.UpperCenter, true);
            _timerText.name = "TimerText";
            _timerText.AddToClassList("rescue-timer");
            panel.Add(_timerText);

            // [7] OptionBox (위 22)
            _optionBox = new VisualElement { name = "OptionBox", pickingMode = PickingMode.Ignore };
            _optionBox.AddToClassList("rescue-options");
            panel.Add(_optionBox);

            _rescueButton = MakeOption("RescueButton", "구출", out _rescueDescText, () => Choose(false));
            _rescueButton.AddToClassList("rescue-option--first");
            _optionBox.Add(_rescueButton);
            _interveneButton = MakeOption("InterveneButton", "난입", out _interveneDescText, () => Choose(true));
            _optionBox.Add(_interveneButton);

            schedule.Execute(Tick).Every(100);
        }

        // 제목 + 설명 두 줄짜리 선택지 버튼 (SizeBox 520 폭 / 최소 74)
        static Button MakeOption(string name, string title, out Label desc, System.Action onClick)
        {
            var button = new Button(onClick) { name = name };
            button.AddToClassList("rescue-option");

            var inside = new VisualElement { pickingMode = PickingMode.Ignore };
            inside.AddToClassList("rescue-option__inside");
            button.Add(inside);

            // 제목 17 #C9A45C (14 / 6 / 14)
            var titleLine = UiStyle.WrapText(title, 17, UiColor.Gold, TextAnchor.UpperLeft, true);
            titleLine.pickingMode = PickingMode.Ignore;
            titleLine.AddToClassList("rescue-option__title");
            inside.Add(titleLine);

            // 설명 15 #ECE3CF (14 / 2 / 14 / 6)
            desc = UiStyle.WrapText("", 15, UiColor.TextBody, TextAnchor.UpperLeft, true);
            desc.pickingMode = PickingMode.Ignore;
            desc.AddToClassList("rescue-option__desc");
            inside.Add(desc);
            return button;
        }

        public void Setup(bool chooser, IReadOnlyList<string> wipedNames, bool canIntervene, float healthCost, float seconds)
        {
            _chooser = chooser;
            _canIntervene = canIntervene;
            _chosen = false;
            _timeLeft = seconds;
            _lastTick = Time.realtimeSinceStartup;

            var names = wipedNames != null ? string.Join(", ", wipedNames) : "";
            int costPercent = Mathf.RoundToInt(healthCost * 100f);

            _titleText.text = chooser ? "동료 전멸" : "전멸";
            _messageText.text = chooser
                ? $"{names} 님이 쓰러졌습니다. 어떻게 할까요?"
                : "쓰러졌습니다...\n다른 방의 동료가 구출 / 난입을 고르는 중입니다.";

            _rescueDescText.text = $"내 현재 체력의 {costPercent}% 를 잃고, 쓰러진 동료를 체력 1 로 일으킵니다. 다음 방은 동료가 쓰러진 방에서 고릅니다. (보상 없음)";
            _interveneDescText.text = canIntervene
                ? "전멸한 방에 들어가 남은 몬스터와 싸웁니다. 몬스터 체력은 그대로, 이기면 보상을 받고 동료는 체력 1 로 일어납니다. 다음 방은 그 방에서 고릅니다."
                : "전멸한 방이 여러 곳이라 난입할 수 없습니다.";

            UiStyle.SetShown(_optionBox, chooser);
            _rescueButton.SetEnabled(true);
            _interveneButton.SetEnabled(canIntervene);
            UpdateTimer();
        }

        void Tick()
        {
            float now = Time.realtimeSinceStartup;
            if (_lastTick >= 0f) _timeLeft = Mathf.Max(0f, _timeLeft - (now - _lastTick));
            _lastTick = now;
            UpdateTimer();
        }

        // 초가 바뀔 때만
        void UpdateTimer() => UiStyle.SetTextIfChanged(_timerText, $"남은 시간 {Mathf.CeilToInt(_timeLeft)}초");

        void Choose(bool intervene)
        {
            if (!_chooser || _chosen) return;
            if (intervene && !_canIntervene) return;

            _chosen = true;
            _rescueButton.SetEnabled(false);
            _interveneButton.SetEnabled(false);
            _messageText.text = $"{(intervene ? "난입" : "구출")} 을(를) 골랐습니다. 다른 동료를 기다리는 중...";

            NetBus.Request(new ChooseRescueReq { bIntervene = intervene });
        }
    }
}
