// 관전 바 (언리얼 USpectateWidget / WBP_Spectate, 기획: 한쪽 파티가 먼저 끝날 경우 관전 가능). 화면 위쪽 가운데 (UILayer.Overlay)
//  - 내 싸움이 끝났거나(승리 / 클리어 / 전멸) 내가 쓰러졌고, 다른 구역이 아직 진행 중일 때만 보임
//  - 구역 버튼을 누르면 그 구역 화면으로 (이 컴퓨터만. 서버는 모름 -> AreaView.SpectateArea). 전투 HUD 는 '관전 중' 으로 보여 주고 스킬은 못 씀
//  - 보던 구역이 끝나거나 내가 다시 싸워야 하면 내 구역으로 돌아옴
// 판단 규칙 = Core DungeonAreaManager.GetSpectatableAreas 를 스냅샷(ClientState.Areas / Party)으로 (클라엔 구역 관리자가 없음)
//
// WBP 구조: RootBox > SpectateBar (위 70, 가운데; #1B1E25 90% + 1px #3A3F4A, 안쪽 12 / 6) > BarRow (안쪽 12 / 6)
//   LabelText "관전" 16 #C9A45C (오른쪽 10) / ButtonBox (가로, 버튼 좌우 4)
using System;
using System.Collections.Generic;
using System.Linq;
using Terminus.Dungeon;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class SpectateWidget : VisualElement
    {
        readonly Label _labelText;
        readonly VisualElement _buttonBox;
        string _shownKey;

        /// <summary>보는 구역을 바꿈 (DungeonView 가 지도 숨기기를 다시 판단)</summary>
        public event Action ViewChanged;

        public SpectateWidget()
        {
            name = "Spectate";
            DungeonStyle.Fill(this);
            pickingMode = PickingMode.Ignore;   // 바깥은 클릭이 통과
            style.alignItems = Align.Center;

            var bar = new VisualElement { name = "SpectateBar" };
            bar.style.marginTop = 70f;
            DungeonStyle.Box(bar, UiColor.WithAlpha(UiColor.Board, 0.9f), UiColor.Line);
            UiStyle.Padding(bar, 12f, 6f);
            Add(bar);

            var row = new VisualElement { name = "BarRow", pickingMode = PickingMode.Ignore };
            row.style.flexDirection = FlexDirection.Row;
            row.style.alignItems = Align.Center;
            UiStyle.Padding(row, 12f, 6f);
            bar.Add(row);

            _labelText = UiStyle.Text("관전", 16, UiColor.Gold, TextAnchor.MiddleCenter);
            _labelText.style.marginRight = 10f;
            row.Add(_labelText);

            _buttonBox = new VisualElement { name = "ButtonBox", pickingMode = PickingMode.Ignore };
            _buttonBox.style.flexDirection = FlexDirection.Row;
            row.Add(_buttonBox);

            UiStyle.SetShown(this, false);

            // 숨긴 위젯도 판단해야 해서 타이머로 (언리얼 0.3초)
            schedule.Execute(Refresh).Every(300);
        }

        /// <summary>내가 아직 싸우는 중인가 (살아서 턴이 도는 전투). 언리얼 bFighting</summary>
        static bool IsFighting(PlayerInfo me, AreaInfo myArea)
        {
            if (me == null || myArea == null || myArea.bCleared) return false;
            bool alive = me.bHasBattler && me.Combat != null && me.Combat.Health > 0;
            bool turnInProgress = myArea.Phase == "PlayerTurn" || myArea.Phase == "MonsterTurn";
            return alive && turnInProgress;
        }

        /// <summary>볼 수 있는 다른 구역 (진행 중인 구역 중 내 것 빼고). 내가 싸우는 중이면 빈 목록 (DungeonAreaManager.GetSpectatableAreas)</summary>
        public static List<AreaInfo> GetSpectatableAreas()
        {
            var me = ClientState.Me;
            var myArea = ClientState.MyArea;
            var others = new List<AreaInfo>();
            if (me == null || IsFighting(me, myArea)) return others;
            foreach (var a in ClientState.Areas.Areas.OrderBy(a => a.AreaIndex))
            {
                if (a.bInUse && (myArea == null || a.AreaIndex != myArea.AreaIndex)) others.Add(a);
            }
            return others;
        }

        // 관전 버튼 글: "이름, 이름 (몬스터)" (DungeonAreaManager.GetAreaLabel)
        static string AreaLabel(AreaInfo area)
        {
            var names = area.Occupants.Select(id => ClientState.NameOf(id));
            return $"{string.Join(", ", names)} ({DungeonAreaManager.RoomTypeLabel(area.Room.Type)})";
        }

        public void Refresh()
        {
            var me = ClientState.Me;
            if (me == null)
            {
                Hide();
                return;
            }

            var myArea = ClientState.MyArea;
            int myIndex = myArea != null ? myArea.AreaIndex : -1;
            var others = GetSpectatableAreas();
            bool canSpectate = others.Count > 0;

            // 보던 구역이 끝났거나 내가 다시 싸워야 하면 내 구역으로
            int viewed = AreaView.SpectateArea;
            if (viewed >= 0 && viewed != myIndex && (!canSpectate || others.All(a => a.AreaIndex != viewed)))
            {
                SetViewed(-1);
                viewed = -1;
            }

            if (!canSpectate)
            {
                Hide();
                return;
            }

            UiStyle.SetShown(this, true);

            // 구역 구성이나 보는 구역이 바뀌었을 때만 다시 그림 (누르는 도중에 버튼이 바뀌지 않게)
            string key = $"{myIndex}|{viewed}|" + string.Join(";", others.Select(a => $"{a.AreaIndex}:{a.Occupants.Count}:{a.Room.Type}"));
            if (key == _shownKey) return;
            _shownKey = key;
            RebuildButtons(myArea, others, viewed);
        }

        void Hide()
        {
            UiStyle.SetShown(this, false);
            _shownKey = null;
        }

        void RebuildButtons(AreaInfo myArea, List<AreaInfo> others, int viewed)
        {
            _buttonBox.Clear();
            int myIndex = myArea != null ? myArea.AreaIndex : -1;
            bool viewingMine = viewed < 0 || viewed == myIndex;

            AddButton(true, -1, myArea != null ? "  내 방  " : "  지도  ", viewingMine);
            foreach (var area in others)
            {
                AddButton(false, area.AreaIndex, $"  {AreaLabel(area)}  ", !viewingMine && viewed == area.AreaIndex);
            }
        }

        void AddButton(bool mine, int areaIndex, string label, bool selected)
        {
            var card = new ChoiceCardWidget(new ChoiceCardStyle { TitleSize = 14, PaddingH = 8f, PaddingV = 2f, TitleColor = UiColor.TextBody });
            card.SetChoiceId(mine ? "Mine" : "Other");
            card.SetTexts(label, "");
            card.SetState(selected ? ChoiceCardState.Selected : ChoiceCardState.Normal);
            card.Chosen += _ => HandleAreaClicked(mine, areaIndex);
            card.style.marginLeft = 4f;
            card.style.marginRight = 4f;
            _buttonBox.Add(card);
        }

        void HandleAreaClicked(bool mine, int areaIndex)
        {
            SetViewed(mine ? -1 : areaIndex);
            _shownKey = null;
            Refresh();
        }

        void SetViewed(int areaIndex)
        {
            if (AreaView.SpectateArea == areaIndex) return;
            AreaView.SpectateArea = areaIndex;
            ViewChanged?.Invoke();
        }
    }
}
