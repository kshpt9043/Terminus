// 지도 화면 상단바 (언리얼 UMapTopBarWidget / WBP_MapTopBar): 지금 층 / 참가자 프로필 카드 / 내 재화 / 나가기
// 지도 화면(DungeonView) 안에 들어감. 구역에 들어갈 때 숨기는 건 지도 화면이 함. 휴식 / 이벤트 / 상점 위에도 하나 더 뜸(RoomTopBarWidget)
//
// 나가기: 확인 팝업 -> 세션을 정리하고 메인 화면으로 (기획 UI 레퍼런스: 우상단 게임 종료 -> 메인 UI)
//
// WBP 구조: Border (#313131, 안쪽 좌우 28) > Overlay
//   HorizontalBox: FloorText ("1층", #E8DFCB) / FloorSubText ("표층 · 테마", 16 #A89F8C, 왼쪽 12 위 4) / Spacer(채움) / PartyBox (오른쪽) / ExitButton (왼쪽 12)
//   Border (가운데, #23202A + 1px #1A1816, 안쪽 16 / 6): [동전 18x18 (오른쪽 10)] [CurrencyText 20 #E8DFCB]
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class MapTopBarWidget : VisualElement
    {
        // 카드 사이 간격 (PartyBox)
        public float CardSpacing = 10f;

        readonly Label _floorText;
        readonly Label _floorSubText;
        readonly VisualElement _partyBox;
        readonly Label _currencyText;
        readonly Button _exitButton;

        readonly List<ulong> _shownPlayers = new List<ulong>();
        readonly List<PartyProfileCardWidget> _cards = new List<PartyProfileCardWidget>();

        public MapTopBarWidget()
        {
            DungeonStyle.Use(this);
            name = "MapTopBar";
            pickingMode = PickingMode.Ignore;

            var bar = new VisualElement { name = "Border_0" };
            bar.style.flexGrow = 1;
            bar.style.backgroundColor = MapColor.TopBar;
            bar.style.paddingLeft = 28f;
            bar.style.paddingRight = 28f;
            Add(bar);

            var row = new VisualElement { pickingMode = PickingMode.Ignore };
            row.style.flexGrow = 1;
            row.style.flexDirection = FlexDirection.Row;
            row.style.alignItems = Align.Center;
            bar.Add(row);

            // 시안: 굵은 "2층" 옆에 흐린 "표층 · 슬라임 왕국"
            _floorText = UiStyle.Text("1층", 24, MapColor.Text, TextAnchor.MiddleLeft, true);
            row.Add(_floorText);

            _floorSubText = UiStyle.Text("", 16, MapColor.TextSub, TextAnchor.MiddleLeft);
            _floorSubText.style.marginLeft = 12f;
            _floorSubText.style.marginTop = 4f;
            row.Add(_floorSubText);

            row.Add(UiStyle.FlexSpacer());

            _partyBox = new VisualElement { name = "PartyBox", pickingMode = PickingMode.Ignore };
            _partyBox.style.flexDirection = FlexDirection.Row;
            _partyBox.style.alignItems = Align.Center;
            row.Add(_partyBox);

            _exitButton = new Button(HandleExitClicked) { name = "ExitButton", text = "나가기" };
            _exitButton.AddToClassList("dm-exit");
            row.Add(_exitButton);

            // 가운데 재화 판
            var currency = new VisualElement { name = "Border_1", pickingMode = PickingMode.Ignore };
            currency.style.position = Position.Absolute;
            currency.style.left = Length.Percent(50);
            currency.style.top = Length.Percent(50);
            currency.style.translate = new Translate(Length.Percent(-50), Length.Percent(-50));
            currency.style.flexDirection = FlexDirection.Row;
            currency.style.alignItems = Align.Center;
            DungeonStyle.Box(currency, MapColor.Plate, UiColor.Hex("1A1816"));
            currency.style.paddingLeft = 16f;
            currency.style.paddingRight = 16f;
            currency.style.paddingTop = 6f;
            currency.style.paddingBottom = 6f;
            bar.Add(currency);

            var coin = new VisualElement { pickingMode = PickingMode.Ignore };
            DungeonStyle.SetImage(coin, MapTextures.Coin);
            coin.style.width = 18f;
            coin.style.height = 18f;
            coin.style.marginRight = 10f;
            coin.style.backgroundSize = new BackgroundSize(BackgroundSizeType.Contain);
            currency.Add(coin);

            _currencyText = UiStyle.Text("0", 20, MapColor.Text, TextAnchor.MiddleLeft);
            currency.Add(_currencyText);

            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                ClientState.PartyChanged -= Refresh;
                ClientState.PartyChanged += Refresh;
                ClientState.MapChanged -= Refresh;
                ClientState.MapChanged += Refresh;
                Refresh();
            });
            RegisterCallback<DetachFromPanelEvent>(_ =>
            {
                ClientState.PartyChanged -= Refresh;
                ClientState.MapChanged -= Refresh;
            });
        }

        public void Refresh()
        {
            // 참가자 구성이 바뀌었으면 (늦게 도착, 누가 나감) 카드 다시 만들기
            var current = ClientState.Party.Players.Where(p => p.bConnected).Select(p => p.Id).ToList();
            if (!current.SequenceEqual(_shownPlayers)) RefreshParty(current);
            foreach (var card in _cards) card.Refresh();

            RefreshTexts();
        }

        void RefreshParty(List<ulong> players)
        {
            _shownPlayers.Clear();
            _shownPlayers.AddRange(players);
            _partyBox.Clear();
            _cards.Clear();
            for (int i = 0; i < players.Count; ++i)
            {
                var card = new PartyProfileCardWidget();
                card.SetPlayer(players[i]);
                card.style.marginLeft = i == 0 ? 0f : CardSpacing * 0.5f;
                card.style.marginRight = i == players.Count - 1 ? 0f : CardSpacing * 0.5f;
                _partyBox.Add(card);
                _cards.Add(card);
            }
        }

        void RefreshTexts()
        {
            var map = ClientState.Map;
            int floor = map != null ? map.Floor : 1;
            string sub = !string.IsNullOrEmpty(map?.TierName) ? map.TierName : Terminus.Map.FloorRules.GetTierName(floor);
            if (map != null && !string.IsNullOrEmpty(map.ThemeName)) sub += " · " + map.ThemeName;
            UiStyle.SetTextIfChanged(_floorText, floor + "층");
            UiStyle.SetTextIfChanged(_floorSubText, sub);

            // 숫자만 (옆에 동전 그림)
            int currency = ClientState.Me?.Run?.Currency ?? 0;
            UiStyle.SetTextIfChanged(_currencyText, currency.ToString("N0", CultureInfo.InvariantCulture));
        }

        void HandleExitClicked()
        {
            // 방장이 나가면 세션이 닫혀 다른 사람도 같이 끝남
            bool hostWithOthers = ClientState.IAmHost && ClientState.Party.Players.Count(p => p.bConnected) > 1;
            string message = hostWithOthers
                ? "메인 화면으로 돌아갑니다.\n방장이 나가면 다른 플레이어도 모두 게임이 끝납니다."
                : "메인 화면으로 돌아갑니다.\n이번 진행은 사라집니다.";
            ConfirmPopupWidget.Show("던전을 나가시겠습니까?", message, NetworkRoot.LeaveToMenu, "나가기", "취소");
        }
    }
}
