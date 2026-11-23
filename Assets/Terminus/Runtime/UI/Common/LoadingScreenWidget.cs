// 로딩 화면 (언리얼 ULoadingScreenWidget + WBP_Loading / ULoadingSpinnerWidget + WBP_LoadingLight, 띄우고 끄는 건 ULoadingScreenSubsystem 자리)
//  - Full : 씬 이동 같은 큰 로딩. 거의 불투명하게 덮고 "불러오는 중" + 로딩 표시 + 안내 문구
//  - Light: 검색 / 잠깐 대기. 검정 70% 반투명 + 로딩 표시만
// 화면 전체를 덮고 아래를 못 누르게 함. 모든 레이어(팝업 포함)보다 위
//
//   LoadingScreenWidget.Show("던전으로 이동하는 중...");      // 다시 부르면 문구만 바뀜
//   LoadingScreenWidget.Show(style: LoadingStyle.Light);
//   LoadingScreenWidget.Hide();
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public enum LoadingStyle
    {
        Full,
        Light,
    }

    public sealed class LoadingScreenWidget : VisualElement
    {
        /// <summary>WBP_Loading 바탕 (선형 0.006995, 0.005605, 0.004391, 98%)</summary>
        public static readonly Color FullBackground = UiColor.FromLinear(0.006995f, 0.005605f, 0.004391f, 0.98f);

        static LoadingScreenWidget _current;

        readonly Label _titleText;
        readonly Label _messageText;

        public LoadingStyle Style { get; }

        /// <summary>지금 떠 있는 로딩 화면 (없으면 null)</summary>
        public static LoadingScreenWidget Current => _current != null && _current.panel != null ? _current : null;

        public static bool IsShowing => Current != null;

        public LoadingScreenWidget(LoadingStyle loadingStyle = LoadingStyle.Full)
        {
            Style = loadingStyle;
            UiStyle.Use(this, "Common");
            AddToClassList("tc-loading");
            AddToClassList(loadingStyle == LoadingStyle.Full ? "tc-loading--full" : "tc-loading--light");
            pickingMode = PickingMode.Position;
            if (loadingStyle == LoadingStyle.Full) style.backgroundColor = FullBackground;

            var column = new VisualElement { pickingMode = PickingMode.Ignore };
            column.AddToClassList("tc-loading__column");
            Add(column);

            _titleText = UiStyle.Title("불러오는 중");
            _titleText.AddToClassList("tc-loading__title");
            column.Add(_titleText);

            var throbber = new Throbber(28f);
            throbber.AddToClassList("tc-loading__throbber");
            column.Add(throbber);

            _messageText = UiStyle.Text("", UiStyle.Body, UiColor.TextSub, TextAnchor.MiddleCenter, true);
            _messageText.AddToClassList("tc-loading__message");
            column.Add(_messageText);

            // 가벼운 로딩은 로딩 표시만 (WBP_LoadingLight 는 제목 / 문구가 Collapsed)
            bool full = loadingStyle == LoadingStyle.Full;
            UiStyle.SetShown(_titleText, full);
            UiStyle.SetShown(_messageText, false);

            RegisterCallback<PointerDownEvent>(e => e.StopPropagation());
        }

        /// <summary>안내 문구 ("던전으로 이동하는 중..."). 가벼운 로딩은 문구를 안 띄움</summary>
        public void SetMessage(string message)
        {
            _messageText.text = message ?? "";
            UiStyle.SetShown(_messageText, Style == LoadingStyle.Full && !string.IsNullOrEmpty(message));
        }

        public void SetTitle(string title) => UiStyle.SetLine(_titleText, title);

        /// <summary>띄움 (이미 같은 종류가 떠 있으면 문구만 바꿈). 모든 레이어 위에 붙음</summary>
        public static LoadingScreenWidget Show(string message = "", LoadingStyle style = LoadingStyle.Full)
        {
            var current = Current;
            if (current != null && current.Style != style)
            {
                current.RemoveFromHierarchy();
                current = null;
            }
            if (current == null)
            {
                var root = UIRoot.Instance != null ? UIRoot.Instance.Root : null;
                if (root == null)
                {
                    Debug.LogWarning("[UI] UIRoot 가 없어 로딩 화면을 못 띄움");
                    return null;
                }
                current = new LoadingScreenWidget(style);
                root.Add(current);   // 레이어 칸들 뒤 = 맨 위
            }
            current.BringToFront();
            current.SetMessage(message);
            _current = current;
            UiTooltip.Hide();
            return current;
        }

        public static void Hide()
        {
            _current?.RemoveFromHierarchy();
            _current = null;
        }
    }
}
