// 핑 / 퀵챗 고르기에 쓰는 작은 목록 메뉴 (언리얼 UCommsMenuWidget / WBP_CommsMenu). 마우스 자리에 뜸 (UILayer.CommsMenu = 채팅 위, 팝업 아래)
//
// - 항목 클릭 또는 숫자 키(1 ~ 9)로 고름. 숫자 키는 PartyCommsWidget 이 PickByNumber 로 넘김
// - ESC / 메뉴 밖 클릭이면 닫힘 (PartyCommsWidget 이 판단)
//
// 화면 전체를 덮는 요소지만 바탕은 클릭을 통과시키고 목록만 클릭을 받음. C++ 만으로 모양을 만든 위젯 (WBP 는 빈 캔버스)
//   판: #1B1E25 + 1px #3A3F4A, 안쪽 10 / 8
//   제목: FontSize - 3, 금, 굵게 (아래 6)
//   항목: 평평한 카드 버튼 (#262A32, 안쪽 10 / 5), 최소 폭 170, "1. 문구" FontSize 본문색 왼쪽 정렬, 사이 4
using System;
using System.Collections.Generic;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Comms
{
    public sealed class CommsMenuWidget : VisualElement
    {
        public int FontSize = 16;
        public float ButtonWidth = 170f;

        readonly VisualElement _panel;
        readonly Label _titleText;
        readonly VisualElement _list;
        readonly List<Button> _buttons = new List<Button>();
        Action<int> _onPicked;

        public bool IsOpen { get; private set; }

        public CommsMenuWidget()
        {
            name = "CommsMenuRoot";
            pickingMode = PickingMode.Ignore;

            _panel = new VisualElement { name = "Panel" };
            _panel.style.position = Position.Absolute;
            _panel.style.backgroundColor = UiColor.Board;
            UiStyle.Border(_panel, 1f, UiColor.Line);
            UiStyle.Padding(_panel, 10f, 8f);
            _panel.RegisterCallback<PointerDownEvent>(e => e.StopPropagation());
            Add(_panel);

            _titleText = UiStyle.Text("", FontSize - 3, UiColor.Gold, TextAnchor.MiddleLeft, true);
            _titleText.style.marginLeft = 2f;
            _titleText.style.marginRight = 2f;
            _titleText.style.marginBottom = 6f;
            _titleText.pickingMode = PickingMode.Ignore;
            _panel.Add(_titleText);

            _list = new VisualElement { pickingMode = PickingMode.Ignore };
            _panel.Add(_list);
        }

        /// <summary>목록을 채우고 position(이 요소 좌표 = 화면 패널 좌표) 근처에 띄움. 이미 떠 있으면 내용만 바꿔 옮김</summary>
        public void Open(string title, IReadOnlyList<string> options, Vector2 position, Action<int> onPicked)
        {
            _onPicked = onPicked;

            _titleText.text = title ?? "";
            UiStyle.SetShown(_titleText, !string.IsNullOrEmpty(title));

            _list.Clear();
            _buttons.Clear();
            for (int i = 0; i < options.Count; ++i)
            {
                int index = i;
                // 작은 평평한 버튼 (여백만 줄임)
                var button = UiStyle.FlatButton($"{i + 1}. {options[i]}", () => Pick(index), FontSize, FlatKind.Card);
                button.style.minWidth = ButtonWidth;
                button.style.paddingLeft = 10f;
                button.style.paddingRight = 10f;
                button.style.paddingTop = 5f;
                button.style.paddingBottom = 5f;
                button.style.marginTop = i == 0 ? 0f : 4f;
                button.style.unityTextAlign = TextAnchor.MiddleLeft;
                button.style.color = UiColor.TextBody;
                _list.Add(button);
                _buttons.Add(button);
            }

            if (UIRoot.Instance != null && parent == null) UIRoot.Instance.Show(UILayer.CommsMenu, this);

            // 화면 오른쪽 / 아래쪽 절반이면 마우스 반대쪽으로 펼침 (화면 밖으로 안 나가게)
            var view = parent != null ? parent.layout.size : new Vector2(1920f, 1080f);
            if (float.IsNaN(view.x) || view.x <= 0f) view = new Vector2(1920f, 1080f);
            bool right = position.x > view.x * 0.5f;
            bool down = position.y > view.y * 0.5f;
            _panel.style.left = position.x + (right ? -8f : 8f);
            _panel.style.top = position.y + (down ? -8f : 8f);
            _panel.style.translate = new Translate(Length.Percent(right ? -100f : 0f), Length.Percent(down ? -100f : 0f));

            IsOpen = true;
        }

        public void Close()
        {
            if (!IsOpen) return;
            IsOpen = false;
            _onPicked = null;
            RemoveFromHierarchy();
        }

        /// <summary>숫자 키 (1 부터). 범위 밖이면 false</summary>
        public bool PickByNumber(int number)
        {
            if (!IsOpen || number < 1 || number > _buttons.Count) return false;
            Pick(number - 1);
            return true;
        }

        /// <summary>이 패널 좌표가 목록 위인가 (메뉴 밖 클릭 판정)</summary>
        public bool IsOverPanel(Vector2 panelPosition) => IsOpen && _panel.worldBound.Contains(panelPosition);

        void Pick(int index)
        {
            // 닫기(Close) 가 콜백을 풀기 때문에 먼저 꺼내 둠
            var callback = _onPicked;
            Close();
            callback?.Invoke(index);
        }
    }
}
