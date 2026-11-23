// 이어하기 목록 (언리얼 URunSaveListWidget + WBP_SaveList, 한 줄은 URunSaveEntryWidget + WBP_SaveEntry)
// 메인 메뉴 '이어하기' -> UILayer.MenuWindow. 싱글(하나) + 멀티 세이브, 최근 저장 먼저
//  - 불러오기: RunSaveService.TryBeginContinue 로 읽고 ServerSession.StartContinue 로 방을 엶 (언리얼 UTerminusSaveSubsystem::ContinueRun)
//    싱글 = 혼자 주점 -> 바로 출발, 멀티 = 세이브 이름으로 방을 열고 던전에서 세이브 플레이어를 기다림 (이공간)
//  - 삭제: 확인 팝업 후 지움
using System;
using System.Globalization;
using Terminus.Game;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class RunSaveListView : VisualElement
    {
        readonly Label _status;
        readonly Label _empty;
        readonly ScrollView _entries;
        bool _loading;

        /// <summary>방을 열기 직전 (메인 메뉴가 이름을 정하고 버튼을 막음)</summary>
        public Action BeforeContinue;

        /// <summary>방을 못 열었음 (메인 메뉴 버튼을 되살림)</summary>
        public Action ContinueFailed;

        public RunSaveListView()
        {
            MenuStyle.Use(this, "Menu", "MenuWindows");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;

            // [1] 어둡게 (0.7) + [2] 760 x 600 창
            var dim = MenuStyle.FullScreen(UiColor.Dim);
            dim.style.alignItems = Align.Center;
            dim.style.justifyContent = Justify.Center;
            Add(dim);

            var window = MenuStyle.RoundPanel(1f, 6f);
            window.style.width = 760;
            window.style.height = 600;
            UiStyle.Padding(window, 20, 20);
            dim.Add(window);

            // [5] 제목 / 닫기
            var header = UiStyle.Row();
            header.style.marginBottom = 14;
            window.Add(header);
            var title = MenuStyle.Text("이어하기", 22, UiColor.TextBody, true);
            title.style.flexGrow = 1;
            header.Add(title);
            header.Add(MenuStyle.FlatButton("닫기", Close));

            // [9] 상태 (경고색) / [10] 빈 목록 안내 / [11] 목록
            _status = MenuStyle.Wrap("", 15, UiColor.Alert, true);
            _status.style.marginBottom = 8;
            window.Add(_status);
            _empty = MenuStyle.Wrap("저장된 진행이 없습니다. 던전에서 방을 끝내면 자동으로 저장됩니다.", 16, UiColor.TextFaint, true);
            window.Add(_empty);
            _entries = new ScrollView(ScrollViewMode.Vertical);
            _entries.AddToClassList("tm-scroll");
            _entries.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _entries.style.flexGrow = 1;
            window.Add(_entries);

            MenuStyle.OnEscape(this, Close);
        }

        public bool IsOpen => panel != null;

        public void Open()
        {
            _loading = false;
            SetStatus("");
            Refresh();
            UIRoot.Instance?.Show(UILayer.MenuWindow, this);
            schedule.Execute(() => Focus());
        }

        public void Close()
        {
            RemoveFromHierarchy();
        }

        /// <summary>방을 못 열었으면 다시 누를 수 있게</summary>
        public void ResetLoading()
        {
            _loading = false;
        }

        public void Refresh()
        {
            // 싱글(하나) + 멀티
            var saves = MenuFlow.Saves.GetRunSaves();
            UiStyle.SetShown(_empty, saves.Count == 0);
            _entries.Clear();
            foreach (var summary in saves)
            {
                var entry = new RunSaveEntryView(summary);
                entry.LoadClicked += OnLoad;
                entry.DeleteClicked += OnDelete;
                entry.style.marginBottom = 8;
                _entries.Add(entry);
            }
        }

        void SetStatus(string text) => UiStyle.SetLine(_status, text);

        // ------------------------------------------------------------ 불러오기 / 삭제

        void OnLoad(string slotName)
        {
            if (_loading) return;

            var saves = MenuFlow.Saves;
            if (!saves.TryBeginContinue(slotName, null, out var error))
            {
                SetStatus(error);
                Refresh();
                return;
            }
            var save = saves.PendingLoad;
            saves.ClearPendingLoad();

            BeforeContinue?.Invoke();
            _loading = true;
            SetStatus("불러오는 중...");
            MenuFlow.ShowLoading("세이브를 불러오는 중...");
            if (!ServerSession.StartContinue(save, out error))
            {
                MenuFlow.HideLoading();
                _loading = false;
                SetStatus(error);
                ContinueFailed?.Invoke();
                return;
            }
            // 주점으로 넘어가는 중. 멀티는 방 열기에 실패하면 메인 메뉴가 팝업을 띄움
            Close();
        }

        void OnDelete(string slotName)
        {
            ConfirmPopupWidget.Show("세이브 삭제", "이 진행을 삭제할까요? 되돌릴 수 없습니다.", () =>
            {
                MenuFlow.Saves.DeleteRunSave(slotName);
                if (panel != null) Refresh();
            }, "삭제", "취소");
        }
    }

    /// <summary>세이브 한 줄 (WBP_SaveEntry): 던전 이름 / 요약 / 플레이어 / 저장 시각 + 불러오기 / 삭제</summary>
    public sealed class RunSaveEntryView : VisualElement
    {
        public string SlotName { get; }

        public event Action<string> LoadClicked;
        public event Action<string> DeleteClicked;

        public RunSaveEntryView(RunSaveSummary summary)
        {
            SlotName = summary.SlotName;
            MenuStyle.Use(this, "Menu", "MenuWindows");

            // [0] 판 #262A32 + 1px #3A3F4A, 6px 모서리, 안쪽 14 / 10
            style.flexDirection = FlexDirection.Row;
            style.alignItems = Align.Center;
            style.backgroundColor = UiColor.Card;
            UiStyle.Border(this, 1f, UiColor.Line);
            UiStyle.Radius(this, 6f);
            UiStyle.Padding(this, 14, 10);

            var info = UiStyle.Column();
            info.style.flexGrow = 1;
            info.style.flexShrink = 1;
            Add(info);

            var name = string.IsNullOrEmpty(summary.RoomName) ? "이름 없는 던전" : summary.RoomName;
            info.Add(MenuStyle.Text(name, 17, UiColor.Gold, true));
            info.Add(Line(RunSaveService.DescribeTitle(summary), 15, UiColor.TextSub));
            info.Add(Line(RunSaveService.DescribePlayers(summary), 15, UiColor.TextBody));
            info.Add(Line(summary.SavedAtTime.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture), 13, UiColor.TextFaint));

            // [7] 불러오기 = 붉은 육각 / [9] 삭제 = 평평한 버튼
            var load = UiStyle.HexButton("불러오기", HexKind.Primary, () => LoadClicked?.Invoke(SlotName), UiStyle.Body);
            load.style.minWidth = 160;
            load.style.marginLeft = 12;
            load.style.marginRight = 6;
            Add(load);
            Add(MenuStyle.FlatButton("삭제", () => DeleteClicked?.Invoke(SlotName)));
        }

        static Label Line(string text, int size, Color color)
        {
            var l = MenuStyle.Wrap(text, size, color, true);
            l.style.marginTop = 4;
            return l;
        }
    }
}
