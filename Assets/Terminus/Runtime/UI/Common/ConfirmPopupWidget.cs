// 범용 팝업 (언리얼 UConfirmPopupWidget + WBP_ConfirmPopup). 뒤 화면을 막고 가운데 600x300 판에 제목 / 내용 / 버튼
//  - 확인 + 취소 : 나가기 확인 같은 것
//  - 확인 하나   : 알림 (cancelLabel 을 비우면 취소 버튼이 숨음)
// 버튼을 누르면 스스로 닫힘 (먼저 닫고 나서 알림). 키보드: Enter / Space = 확인, Esc = 취소 (취소 버튼이 없으면 확인)
//
//   ConfirmPopupWidget.Show("나가기", "정말 나가시겠습니까?", () => Leave());           // 확인 + 취소
//   ConfirmPopupWidget.Alert("알림", "방이 가득 찼습니다.");                             // 확인만
using System;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class ConfirmPopupWidget : VisualElement
    {
        /// <summary>WBP 판 바탕 (선형 0.026042 = #2D2D2D)</summary>
        public static readonly Color PanelColor = UiColor.FromLinear(0.026042f, 0.026042f, 0.026042f);

        readonly Label _titleText;
        readonly Label _messageText;
        readonly Button _confirmButton;
        readonly Button _cancelButton;
        bool _closed;
        bool _hasCancel = true;

        public event Action Confirmed;
        public event Action Cancelled;

        public ConfirmPopupWidget()
        {
            UiStyle.Use(this, "Common");
            AddToClassList("tc-popup");
            pickingMode = PickingMode.Position;   // 뒤 화면을 못 누르게
            focusable = true;

            var box = new VisualElement { name = "PopupBox" };
            box.AddToClassList("tc-popup__box");
            Add(box);

            _titleText = UiStyle.WrapText("", UiStyle.Section, UiColor.Gold, TextAnchor.UpperLeft, true);
            _titleText.AddToClassList("tc-popup__title");
            box.Add(_titleText);

            _messageText = UiStyle.WrapText("", UiStyle.Body, UiColor.TextBody, TextAnchor.MiddleCenter);
            _messageText.AddToClassList("tc-popup__message");
            box.Add(_messageText);

            var buttons = UiStyle.Row();
            buttons.AddToClassList("tc-popup__buttons");
            box.Add(buttons);

            // 확인 = 주 행동 육각 버튼, 취소 = 평평한 버튼
            _confirmButton = UiStyle.HexButton("확인", HexKind.Primary, () => Close(true));
            _confirmButton.AddToClassList("tc-popup__confirm");
            buttons.Add(_confirmButton);

            _cancelButton = UiStyle.FlatButton("취소", () => Close(false), UiStyle.Body, FlatKind.Board);
            _cancelButton.AddToClassList("tc-popup__cancel");
            buttons.Add(_cancelButton);

            RegisterCallback<KeyDownEvent>(OnKeyDown, TrickleDown.TrickleDown);
            RegisterCallback<PointerDownEvent>(e => e.StopPropagation());   // 바탕 클릭은 먹기만
            RegisterCallback<AttachToPanelEvent>(_ => schedule.Execute(() => Focus()));
        }

        /// <summary>확인 + 취소 팝업을 UILayer.Popup 에 띄움</summary>
        public static ConfirmPopupWidget Show(string title, string message, Action onConfirm = null,
            string confirmLabel = "확인", string cancelLabel = "취소", Action onCancel = null)
        {
            var popup = new ConfirmPopupWidget();
            popup.Setup(title, message, confirmLabel, cancelLabel);
            if (onConfirm != null) popup.Confirmed += onConfirm;
            if (onCancel != null) popup.Cancelled += onCancel;
            if (UIRoot.Instance != null) UIRoot.Instance.Show(UILayer.Popup, popup);
            else Debug.LogWarning("[UI] UIRoot 가 없어 팝업을 못 띄움: " + title);
            return popup;
        }

        /// <summary>확인 버튼 하나짜리 알림</summary>
        public static ConfirmPopupWidget Alert(string title, string message, Action onOk = null, string okLabel = "확인") =>
            Show(title, message, onOk, okLabel, "", null);

        /// <summary>내용 채우기. cancelLabel 이 비어 있으면 버튼 하나짜리 알림, confirmLabel 이 비면 "확인"</summary>
        public void Setup(string title, string message, string confirmLabel = "확인", string cancelLabel = "취소")
        {
            _hasCancel = !string.IsNullOrEmpty(cancelLabel);
            UiStyle.SetLine(_titleText, title);
            UiStyle.SetLine(_messageText, message);
            _confirmButton.text = string.IsNullOrEmpty(confirmLabel) ? "확인" : confirmLabel;
            _cancelButton.text = cancelLabel ?? "";
            UiStyle.SetShown(_cancelButton, _hasCancel);
        }

        void OnKeyDown(KeyDownEvent e)
        {
            switch (e.keyCode)
            {
                case KeyCode.Return:
                case KeyCode.KeypadEnter:
                case KeyCode.Space:
                    Close(true);
                    e.StopPropagation();
                    break;
                case KeyCode.Escape:
                    Close(!_hasCancel);   // ESC = 취소 (알림처럼 취소 버튼이 없으면 확인)
                    e.StopPropagation();
                    break;
            }
        }

        /// <summary>닫고 알림 (한 번만). 먼저 닫고 나서 알림 (받은 쪽이 씬 이동 같은 걸 해도 안전하게)</summary>
        public void Close(bool confirmed)
        {
            if (_closed) return;
            _closed = true;
            RemoveFromHierarchy();
            if (confirmed) Confirmed?.Invoke();
            else Cancelled?.Invoke();
        }
    }
}
