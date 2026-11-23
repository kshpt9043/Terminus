// 테마 끝 층 보스 뒤 선택 화면 (언리얼 UFloorVoteWidget / WBP_FloorVote): 다음 층 / 탈출 / 배신. 멀티는 투표
//  - 카드마다 이름 / 설명 / "n명 선택". 누가 골랐는지는 투표가 끝나고 공개 타이머(5초) 뒤에 카드 아래에 나옴
//  - 배신은 선착순 1명 -> 누가 고르면 투표가 바로 끝남
//  - 한 번 고르면 못 바꿈
// 상태는 ClientState.Map.Vote (언리얼 MapManager VoteState 복제). DungeonView 가 바뀔 때마다 Refresh 를 부름 (UILayer.Overlay)
//
// WBP 구조: FloorVoteDim (검정 80%) + 가운데 VerticalBox
//   TitleText 40 #ECE3CF (줄바꿈) / StatusText 17 #C9A45C (위 10 아래 28) / CardBox (가로, 카드 240x300 좌우 12)
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Map;
using Terminus.UI.Common;
using Unity.Netcode;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class FloorVoteWidget : VisualElement
    {
        public Vector2 CardSize = new Vector2(240f, 300f);

        readonly Label _titleText;
        readonly Label _statusText;
        readonly VisualElement _cardBox;

        FloorVoteState _shown = new FloorVoteState();
        FloorChoice _myChoice = FloorChoice.None;

        public FloorVoteWidget()
        {
            name = "FloorVote";
            DungeonStyle.Fill(this);
            // 아래 지도를 못 누르게
            pickingMode = PickingMode.Position;
            RegisterCallback<PointerDownEvent>(e => e.StopPropagation());

            var dim = new VisualElement { name = "FloorVoteDim", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(dim);
            dim.style.backgroundColor = new Color(0f, 0f, 0f, 0.8f);
            Add(dim);

            var column = new VisualElement { name = "FloorVoteColumn", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(column);
            column.style.alignItems = Align.Center;
            column.style.justifyContent = Justify.Center;
            Add(column);

            _titleText = UiStyle.WrapText("", 40, UiColor.FromLinear(0.838799f, 0.768151f, 0.623960f), TextAnchor.MiddleCenter);
            column.Add(_titleText);

            _statusText = UiStyle.WrapText("", 17, UiColor.FromLinear(0.584078f, 0.371238f, 0.107023f), TextAnchor.MiddleCenter);
            _statusText.style.marginTop = 10f;
            _statusText.style.marginBottom = 28f;
            column.Add(_statusText);

            _cardBox = new VisualElement { name = "CardBox", pickingMode = PickingMode.Ignore };
            _cardBox.style.flexDirection = FlexDirection.Row;
            _cardBox.style.alignItems = Align.FlexStart;
            column.Add(_cardBox);

            // 남은 초 (언리얼 NativeTick)
            schedule.Execute(UpdateStatus).Every(100);
        }

        static double ServerNow
        {
            get
            {
                var nm = NetworkManager.Singleton;
                return nm != null && (nm.IsClient || nm.IsServer) ? nm.ServerTime.Time : Time.timeAsDouble;
            }
        }

        public void Refresh(FloorVoteState state)
        {
            if (state == null) return;
            // 새 투표면 내 선택 초기화 (내 표는 공개 전엔 서버가 안 알려 줌 -> 이 화면이 기억)
            if (state.VoteId != _shown.VoteId) _myChoice = FloorChoice.None;
            _shown = state.Clone();

            _titleText.text = $"{state.Floor}층 보스를 쓰러뜨렸습니다";
            RebuildCards();
            UpdateStatus();
        }

        void UpdateStatus()
        {
            int remaining = Mathf.Max(0, Mathf.CeilToInt((float)(_shown.PhaseEndTime - ServerNow)));
            string status = "";
            switch (_shown.Phase)
            {
                case FloorVotePhase.Voting:
                    status = _myChoice == FloorChoice.None
                        ? $"다음 행선지를 고르세요 · {remaining}초  ({_shown.VotedCount} / {_shown.TotalVoters}명 투표)"
                        : $"다른 사람을 기다리는 중 · {remaining}초  ({_shown.VotedCount} / {_shown.TotalVoters}명 투표)";
                    break;
                case FloorVotePhase.Revealing:
                    status = $"투표 끝! 누가 무엇을 골랐는지 {remaining}초 뒤 공개";
                    break;
                case FloorVotePhase.Done:
                    status = $"결과: {ChoiceName(_shown.Result)}";
                    break;
            }
            UiStyle.SetTextIfChanged(_statusText, status);   // 자주 불림 -> 초가 바뀔 때만 실제로 넣음
        }

        void RebuildCards()
        {
            _cardBox.Clear();

            bool voting = _shown.Phase == FloorVotePhase.Voting;
            bool revealed = _shown.Phase == FloorVotePhase.Done;

            for (int i = 0; i < _shown.Options.Count; ++i)
            {
                var choice = _shown.Options[i];
                int count = i < _shown.Counts.Count ? _shown.Counts[i] : 0;
                bool mine = _myChoice == choice;
                bool betrayTaken = choice == FloorChoice.Betray && count > 0;
                bool winner = revealed && _shown.Result == choice;

                // 표 수 (누가 골랐는지는 공개 뒤에)
                string countLabel = _shown.TotalVoters > 1 ? $"{count}명 선택" : "";
                if (mine) countLabel += "  (내 선택)";

                var card = new ChoiceCardWidget(new ChoiceCardStyle { Size = CardSize, TitleSize = 22 });
                card.SetChoiceId(choice.ToString());
                card.SetTexts(ChoiceName(choice), ChoiceDescription(choice), countLabel);
                card.SetState(winner ? ChoiceCardState.Highlight
                    : mine ? ChoiceCardState.Selected
                    : choice == FloorChoice.Betray ? ChoiceCardState.Danger
                    : ChoiceCardState.Normal);
                card.SetChoiceEnabled(voting && _myChoice == FloorChoice.None && !betrayTaken);
                card.Chosen += _ => HandleCardClicked(choice);

                // 공개: 카드 아래에 고른 사람
                var cardColumn = new VisualElement { pickingMode = PickingMode.Ignore };
                cardColumn.style.marginLeft = 12f;
                cardColumn.style.marginRight = 12f;
                cardColumn.Add(card);
                if (revealed && _shown.TotalVoters > 1)
                {
                    var names = new System.Collections.Generic.List<string>();
                    foreach (var entry in _shown.RevealedVotes)
                    {
                        if (entry.Choice == choice) names.Add(ClientState.NameOf(entry.PlayerId));
                    }
                    var who = UiStyle.WrapText(names.Count > 0 ? string.Join("\n", names) : "-", 15, Color.white, TextAnchor.UpperLeft);
                    who.style.marginTop = 10f;
                    cardColumn.Add(who);
                }
                _cardBox.Add(cardColumn);
            }
        }

        /// <summary>서버가 내 표를 거절했을 때 (배신을 누가 먼저 골랐음 등)</summary>
        public void ClearMyChoice()
        {
            _myChoice = FloorChoice.None;
            RebuildCards();
            UpdateStatus();
        }

        void HandleCardClicked(FloorChoice choice)
        {
            if (_shown.Phase != FloorVotePhase.Voting || _myChoice != FloorChoice.None) return;

            // 서버 결과를 기다리지 않고 바로 내 선택으로 표시 (서버가 거절하면 FloorVoteRejectedMsg 로 되돌림)
            _myChoice = choice;
            RebuildCards();
            UpdateStatus();
            NetBus.Request(new CastFloorVoteReq { Choice = choice });
        }

        static string ChoiceName(FloorChoice choice)
        {
            switch (choice)
            {
                case FloorChoice.NextFloor: return "다음 층";
                case FloorChoice.Escape: return "탈출";
                case FloorChoice.Betray: return "배신";
                default: return "-";
            }
        }

        string ChoiceDescription(FloorChoice choice)
        {
            switch (choice)
            {
                case FloorChoice.NextFloor:
                    return $"{_shown.Floor + 1}층({FloorRules.GetTierName(_shown.Floor + 1)})으로 내려갑니다.\n새 테마, 더 강한 적과 보상.";
                case FloorChoice.Escape:
                    return "던전을 나가 정산합니다.\n원하는 유물을 창고에 보관하고 나머지는 판매,\n장착한 픽업 스킬 하나를 가져갑니다.";
                case FloorChoice.Betray:
                    return "동료를 배신하고 홀로 싸웁니다.\n이기면 동료들의 보스 유물을 빼앗습니다.\n선착순 1명만 고를 수 있습니다.";
                default:
                    return "";
            }
        }
    }
}
