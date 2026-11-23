// 지도의 방 하나 (언리얼 URoomNodeWidget / WBP_RoomNodeWidget). [아이콘 상자 150x150][동료 칩]
// 아이콘 상자 가운데에 아이콘(64, 보스 96). 그 위로 등불 빛 / X / 먹 동그라미 / 내 선택(붉은 잉크)
//
// WBP 구조 그대로:
//   HorizontalBox
//     SizeBox 150x150 > Overlay
//       GlowImage (T_Map_Glow, 채움, 숨김)
//       IconBox 64 (가운데) > RoomButton (브러시 = 방 아이콘)
//       VisitedMark (T_Map_VisitedX 64, 숨김)
//       CurrentRing (T_Map_CurrentRing 96, 숨김)
//       SelectionBorder (T_Map_SelectCircle, 채움, 숨김)
//     ProfileContainer (왼쪽 -25, 가운데)
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Map;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    // 지도 시안의 방 아이콘 상태 4종 + 현재 위치
    public enum RoomNodeState
    {
        Default,      // 기본: 아직 갈 수 있는 앞쪽 방
        Available,    // 갈 수 있음: 지금 고를 수 있는 방 (등불 빛)
        Visited,      // 지나옴
        Current,      // 현재 위치: 먹 동그라미
        Unreachable,  // 못 감: 흐리게
    }

    public sealed class RoomNodeWidget : VisualElement
    {
        // 아이콘 상자 (WBP SizeBox 150x150). 지도 캔버스의 RoomIconCenter = 이 값의 절반
        public const float BoxSize = 150f;

        // 아이콘 크기 (보스 / 그 외)
        public float IconSize = 64f;
        public float BossIconSize = 96f;

        // 동료 칩 한 변 (테두리 포함)
        public float ChipSize = 28f;

        // 아이콘 불투명도: 보통 / 못 감
        public float IconOpacity = 0.94f;
        public float UnreachableOpacity = 0.32f;

        public int RoomId { get; private set; } = -1;
        public int Row { get; private set; }

        /// <summary>고를 수 있는 방을 누름 (RoomId)</summary>
        public event Action<int> Clicked;

        readonly VisualElement _glow;
        readonly VisualElement _iconBox;
        readonly VisualElement _roomButton;
        readonly VisualElement _visitedMark;
        readonly VisualElement _currentRing;
        readonly VisualElement _selectionBorder;
        readonly VisualElement _profileContainer;

        bool? _appliedSelectable;
        bool? _appliedSelectedByMe;
        RoomNodeState? _appliedState;
        readonly List<ulong> _appliedSelectors = new List<ulong>();

        public RoomNodeWidget()
        {
            DungeonStyle.Use(this);
            AddToClassList("dm-room");
            pickingMode = PickingMode.Ignore;
            style.flexDirection = FlexDirection.Row;

            var sizeBox = new VisualElement { name = "IconOverlay", pickingMode = PickingMode.Ignore };
            sizeBox.style.width = BoxSize;
            sizeBox.style.height = BoxSize;
            sizeBox.style.flexShrink = 0;
            sizeBox.style.alignItems = Align.Center;
            sizeBox.style.justifyContent = Justify.Center;
            Add(sizeBox);

            _glow = Image("GlowImage", MapTextures.Glow);
            DungeonStyle.Fill(_glow);
            sizeBox.Add(_glow);

            _iconBox = new VisualElement { name = "IconBox", pickingMode = PickingMode.Ignore };
            _iconBox.style.position = Position.Absolute;
            sizeBox.Add(_iconBox);

            _roomButton = new VisualElement { name = "RoomButton" };
            _roomButton.AddToClassList("dm-room__icon");
            _roomButton.style.flexGrow = 1;
            _roomButton.style.backgroundSize = new BackgroundSize(BackgroundSizeType.Contain);
            _roomButton.AddManipulator(new Clickable(OnButtonClicked));   // Clickable 이 :active (눌림) 상태도 켬
            _iconBox.Add(_roomButton);

            _visitedMark = Image("VisitedMark", MapTextures.VisitedX);
            _visitedMark.style.position = Position.Absolute;
            sizeBox.Add(_visitedMark);

            _currentRing = Image("CurrentRing", MapTextures.CurrentRing);
            _currentRing.style.position = Position.Absolute;
            sizeBox.Add(_currentRing);

            _selectionBorder = Image("SelectionBorder", MapTextures.SelectCircle);
            DungeonStyle.Fill(_selectionBorder);
            sizeBox.Add(_selectionBorder);

            _profileContainer = new VisualElement { name = "ProfileContainer", pickingMode = PickingMode.Ignore };
            _profileContainer.style.flexDirection = FlexDirection.Row;
            _profileContainer.style.alignSelf = Align.Center;
            _profileContainer.style.marginLeft = -25f;
            Add(_profileContainer);

            SetIconBoxSize(IconSize);
            Show(_glow, false);
            Show(_visitedMark, false);
            Show(_currentRing, false);
            Show(_selectionBorder, false);
        }

        static VisualElement Image(string name, string unrealPath)
        {
            var e = new VisualElement { name = name, pickingMode = PickingMode.Ignore };
            DungeonStyle.SetImage(e, unrealPath);
            e.style.backgroundSize = new BackgroundSize(BackgroundSizeType.Contain);
            return e;
        }

        // 언리얼 Hidden: 자리는 남기고 안 보임
        static void Show(VisualElement e, bool show) => e.style.visibility = show ? Visibility.Visible : Visibility.Hidden;

        void SetIconBoxSize(float size)
        {
            _iconBox.style.width = size;
            _iconBox.style.height = size;
            _visitedMark.style.width = size;
            _visitedMark.style.height = size;
            // 동그라미 그림은 상자의 81% 가 고리 -> 아이콘보다 사방 12px 크게 (시안: inset -12)
            _currentRing.style.width = size * 1.7f;
            _currentRing.style.height = size * 1.7f;
        }

        public void SetupRoomNode(RoomNode room)
        {
            RoomId = room.RoomId;
            Row = room.Row;
            bool boss = room.Type == RoomType.BOSS;

            // 1. 방 종류 아이콘 (버튼 브러시 자체)
            DungeonStyle.SetImage(_roomButton, MapTextures.RoomIcon(room.Type));

            // 퀘스트 특수 방: 금색 + 조금 크게 (안 그러면 일반 가디언 방과 구분이 안 됨)
            bool special = !Ids.IsNone(room.SpecialRoom);
            EnableInClassList("dm-room--special", special);

            // 마우스를 올리면 어떤 방인지 (11-04 피드백). 퀘스트 특수 방은 진행 중인 의뢰 설명까지
            string tip = DescribeRoomType(room.Type);
            if (special)
            {
                tip = "퀘스트 특수 방\n진행 중인 의뢰의 목표 몬스터가 나옵니다.";
                var questRow = ClientState.Me?.Run?.ActiveQuest?.Quest ?? "";
                var quest = Ids.IsNone(questRow) ? null : GameData.FindQuestRow(questRow);
                if (quest != null) tip += $"\n\n[{quest.QuestName}]\n{UiText.FixNewlines(quest.QuestDesc)}";
            }
            UiTooltip.SetText(_roomButton, tip);

            // 2. 보스는 아이콘 / 동그라미를 크게 (아이콘 상자 가운데 기준이라 선 위치는 그대로)
            SetIconBoxSize(boss ? BossIconSize : special ? IconSize * 1.25f : IconSize);
        }

        /// <summary>방 종류 설명 (마우스를 올리면 뜨는 툴팁)</summary>
        public static string DescribeRoomType(RoomType type)
        {
            switch (type)
            {
                case RoomType.MONSTER: return "몬스터 방\n일반 몬스터와 싸웁니다. 이기면 던전 재화와 픽업 스킬 보상.";
                case RoomType.GUARDIAN: return "가디언 방\n강한 가디언 몬스터와 싸웁니다. 이기면 유물 보상.";
                case RoomType.BOSS: return "보스 방\n이 층의 보스와 싸웁니다. 이기면 다음 층으로.";
                case RoomType.BREAK: return "휴식 방\n체력을 회복하거나 주변을 탐색합니다.";
                case RoomType.STORE: return "상점\n던전 재화로 유물과 스킬을 사고, 픽업 스킬을 강화합니다.";
                case RoomType.EVENT: return "이벤트 방\n무작위 사건. 고르는 선택지에 따라 보상이나 대가가 있습니다.";
                case RoomType.QUEST: return "의뢰 방\n세력의 의뢰(퀘스트)를 하나 고릅니다.";
                default: return "";
            }
        }

        /// <summary>누를 수 있는 방인가. 못 누르는 방은 호버 효과 / 클릭만 막힘 (툴팁은 뜸)</summary>
        public void SetRoomSelectable(bool selectable)
        {
            if (_appliedSelectable == selectable) return;
            _appliedSelectable = selectable;
            EnableInClassList("dm-room--selectable", selectable);
        }

        /// <summary>지도 시안 상태 (빛 / 동그라미 / 흐림)</summary>
        public void SetVisualState(RoomNodeState state)
        {
            if (_appliedState == state) return;
            _appliedState = state;

            // 지나간 방도 X 대신 동그라미 (11-04 피드백). 지금 방은 진하게, 지나간 방은 조금 흐리게
            Show(_glow, state == RoomNodeState.Available);
            Show(_visitedMark, false);
            Show(_currentRing, state == RoomNodeState.Current || state == RoomNodeState.Visited);
            _currentRing.style.opacity = state == RoomNodeState.Visited ? 0.6f : 1f;

            _roomButton.style.opacity = state == RoomNodeState.Unreachable ? UnreachableOpacity : IconOpacity;
        }

        /// <summary>이 방을 고른 사람 칩 + 내 선택 붉은 잉크 동그라미</summary>
        public void RefreshSelectionState(IReadOnlyList<PlayerInfo> players, ulong localId)
        {
            var selectors = new List<ulong>();
            bool selectedByMe = false;
            foreach (var p in players)
            {
                if (p == null || p.Run == null || p.Run.SelectedRoomId != RoomId) continue;
                selectors.Add(p.Id);
                if (p.Id == localId) selectedByMe = true;
            }

            // 이 방의 선택 상태가 그대로면 다시 그리지 않음 -> 바뀐 방만 갱신됨
            if (_appliedSelectedByMe == selectedByMe && SameList(_appliedSelectors, selectors)) return;
            _appliedSelectedByMe = selectedByMe;
            _appliedSelectors.Clear();
            _appliedSelectors.AddRange(selectors);

            _profileContainer.Clear();
            for (int i = 0; i < selectors.Count; ++i)
            {
                // 동료 칩: 2px 파티 판 테두리 + 초상화 자리
                // TODO: Steam 아바타를 칩에 (언리얼도 TODO)
                var frame = new VisualElement { pickingMode = PickingMode.Ignore };
                frame.style.backgroundColor = MapColor.PartyPlate;
                UiStyle.Padding(frame, 2f, 2f);
                frame.style.marginLeft = i == 0 ? 0f : 4f;
                frame.style.alignSelf = Align.Center;
                var chip = new VisualElement { pickingMode = PickingMode.Ignore };
                chip.style.backgroundColor = MapColor.ChipFill;
                chip.style.width = ChipSize - 4f;
                chip.style.height = ChipSize - 4f;
                frame.Add(chip);
                _profileContainer.Add(frame);
            }

            Show(_selectionBorder, selectedByMe);
        }

        static bool SameList(List<ulong> a, List<ulong> b)
        {
            if (a.Count != b.Count) return false;
            for (int i = 0; i < a.Count; ++i)
            {
                if (a[i] != b[i]) return false;
            }
            return true;
        }

        void OnButtonClicked()
        {
            if (_appliedSelectable != true) return;   // 못 고르는 방 (툴팁 때문에 마우스만 받는 중)
            Clicked?.Invoke(RoomId);
        }
    }
}
