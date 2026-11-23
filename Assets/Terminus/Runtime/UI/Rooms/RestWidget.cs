// 휴식터 화면 (언리얼 URestWidget + WBP_RestWidget). 휴식 / 탐색 중 하나만 고름 (기획 10-07)
//  - 휴식: 싱글은 바로 나를 회복. 멀티는 회복시킬 사람(나 또는 같은 휴식터 동료)을 한 번 더 고름. 동료를 고르면 나는 회복 안 함
//  - 탐색: 소량의 던전 재화 + 낮은 확률로 유물
//  - 고르면 서버가 결과를 알려 줌 (RestResultMsg). 같은 휴식터 전원이 고르면 지도로 (CloseRestMsg)
// WBP 배치: 화면 전체 그림(RestBackground) + 제목 (485, 213) / 안내 (484, 364) / 선택지 상자 (506, 592) 고정 좌표
using System.Collections.Generic;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    public sealed class RestWidget : VisualElement
    {
        // 선택지 카드 크기 (언리얼 OptionSize 520x74, 최소 높이)
        static readonly Vector2 OptionSize = new Vector2(520f, 74f);

        enum Stage { Choose, PickTarget, Waiting, Done }

        enum Option { Rest, Explore, Target, Back }

        readonly Label _titleText;
        readonly Label _messageText;
        readonly VisualElement _optionBox;

        readonly List<ulong> _occupants = new List<ulong>();
        Stage _stage = Stage.Choose;
        float _healRatio = 0.2f;
        int _exploreMin = 5, _exploreMax = 10;
        float _exploreRelicChance = 0.05f;
        string _resultText = "";

        // 대상 고르기에서 마지막으로 그린 체력 (바뀌었을 때만 다시 그림 -> 누르는 도중에 버튼이 바뀌지 않게)
        string _shownHealth = "";

        public RestWidget()
        {
            RoomLooks.Use(this, "Rest");
            AddToClassList("rp-root");
            pickingMode = PickingMode.Position;   // 아래 화면을 못 누르게

            // [1] ArtImage (화면 전체)
            Add(RoomLooks.FullImage("/Game/JSY/Widget/Temp_Icons/RestBackground", "ArtImage"));

            // [3] MessageText (484, 364) 17 Regular (0.584078, 0.533276, 0.423268) = #C9C1AE
            _messageText = UiStyle.Text("", 17, UiColor.TextSub, TextAnchor.UpperLeft);
            _messageText.name = "MessageText";
            _messageText.AddToClassList("rest-message");
            Add(_messageText);

            // [4] TitleText (485, 213) 40 #C9A45C
            _titleText = UiStyle.Text("휴식처", 40, UiColor.Gold, TextAnchor.UpperLeft, true);
            _titleText.name = "TitleText";
            _titleText.AddToClassList("rest-title");
            Add(_titleText);

            // [2] OptionBox (506, 592) 내용 크기
            _optionBox = new VisualElement { name = "OptionBox", pickingMode = PickingMode.Ignore };
            _optionBox.AddToClassList("rest-options");
            Add(_optionBox);

            // 같은 휴식터 사람들의 체력이 바뀌면 (동료가 나를 회복) 대상 고르기를 다시 그림
            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                ClientState.PartyChanged -= HandleHealthChanged;
                ClientState.PartyChanged += HandleHealthChanged;
            });
            RegisterCallback<DetachFromPanelEvent>(_ => ClientState.PartyChanged -= HandleHealthChanged);
        }

        public void Setup(IReadOnlyList<ulong> occupants, float healRatio, int exploreMin, int exploreMax, float exploreRelicChance)
        {
            _healRatio = healRatio;
            _exploreMin = exploreMin;
            _exploreMax = exploreMax;
            _exploreRelicChance = exploreRelicChance;
            _stage = Stage.Choose;
            _resultText = "";

            // 나를 맨 앞에
            _occupants.Clear();
            var myId = ClientState.MyId;
            if (occupants != null)
            {
                foreach (var id in occupants) if (id == myId) _occupants.Add(id);
                foreach (var id in occupants) if (id != myId) _occupants.Add(id);
            }

            Rebuild();
        }

        /// <summary>서버가 알려 준 결과 ("체력을 14 회복했습니다.")</summary>
        public void ShowResult(string result)
        {
            _resultText = result ?? "";
            _stage = Stage.Done;
            Rebuild();
        }

        void HandleHealthChanged()
        {
            if (_stage == Stage.PickTarget && ReadHealth() != _shownHealth) Rebuild();
        }

        string ReadHealth()
        {
            var key = "";
            foreach (var id in _occupants)
            {
                GetHealth(id, out int health, out int max);
                key += health + "/" + max + ",";
            }
            return key;
        }

        // 배틀러의 지금 / 최대 체력
        static bool GetHealth(ulong id, out int health, out int maxHealth)
        {
            health = -1;
            maxHealth = -1;
            var p = ClientState.Party.Find(id);
            if (p == null || !p.bHasBattler) return false;
            health = p.Combat.Health;
            maxHealth = p.Stats.MaxHealth;
            return true;
        }

        void AddOption(Option option, ulong target, string title, string description, bool enabled = true)
        {
            var card = new ChoiceCardWidget(new ChoiceCardStyle
            {
                Size = OptionSize,
                FixedHeight = false,
                TitleSize = 17,
                TitleColor = UiColor.Gold,
                CenterText = false,
                PaddingH = 14f,
                PaddingV = 6f,
                NormalColor = UiColor.Card,
            });
            card.SetChoiceId(option.ToString());
            card.SetTexts(title, description);
            card.SetChoiceEnabled(enabled);
            card.Chosen += _ => HandleOption(option, target);
            card.style.marginBottom = 12f;
            _optionBox.Add(card);
        }

        void Rebuild()
        {
            _optionBox.Clear();
            _shownHealth = ReadHealth();

            int percent = Mathf.RoundToInt(_healRatio * 100f);
            bool multi = _occupants.Count > 1;
            string message = "";

            switch (_stage)
            {
                case Stage.Choose:
                {
                    message = "모닥불이 타닥거리는 조용한 쉼터입니다.\n잠시 쉬어 갈 수도, 주변을 둘러볼 수도 있습니다.";

                    bool hasHealth = _occupants.Count > 0 && GetHealth(_occupants[0], out _, out _);
                    int health = 0, maxHealth = 0;
                    if (hasHealth) GetHealth(_occupants[0], out health, out maxHealth);
                    int heal = Mathf.Max(1, Mathf.RoundToInt(maxHealth * _healRatio));

                    AddOption(Option.Rest, 0, "휴식", multi
                        ? $"나 또는 동료 한 명의 최대 체력 {percent}%를 회복합니다."
                        : hasHealth
                            ? $"최대 체력의 {percent}% (+{heal})를 회복합니다. (체력 {health} / {maxHealth})"
                            : $"최대 체력의 {percent}%를 회복합니다.");

                    AddOption(Option.Explore, 0, "탐색",
                        $"주변을 뒤져 던전 재화 {_exploreMin}~{_exploreMax} 를 얻습니다. {Mathf.RoundToInt(_exploreRelicChance * 100f)}% 확률로 유물을 발견합니다.");
                    break;
                }

                case Stage.PickTarget:
                {
                    message = $"누구를 회복시킬까요? 최대 체력의 {percent}%를 회복합니다.\n동료를 고르면 나는 회복하지 않습니다.";
                    var myId = ClientState.MyId;
                    foreach (var id in _occupants)
                    {
                        var name = ClientState.NameOf(id);
                        bool hasHealth = GetHealth(id, out int health, out int maxHealth);
                        int heal = Mathf.Max(1, Mathf.RoundToInt(maxHealth * _healRatio));
                        AddOption(Option.Target, id, id == myId ? $"나 ({name})" : name,
                            hasHealth ? $"체력 {health} / {maxHealth}  →  +{heal} 회복" : "체력 ?");
                    }
                    AddOption(Option.Back, 0, "돌아가기", "");
                    break;
                }

                case Stage.Waiting:
                    message = "...";
                    break;

                case Stage.Done:
                    message = _resultText;
                    if (multi) message += "\n\n다른 동료가 고르기를 기다리는 중...";
                    break;
            }

            _messageText.text = message;
        }

        void HandleOption(Option option, ulong target)
        {
            switch (option)
            {
                case Option.Rest:
                    if (_stage != Stage.Choose) return;
                    // 싱글(혼자 들어온 휴식터)은 바로 나를 회복
                    if (_occupants.Count <= 1)
                    {
                        _stage = Stage.Waiting;
                        Rebuild();
                        NetBus.Request(new RestTargetReq { TargetId = ClientState.MyId });
                        return;
                    }
                    _stage = Stage.PickTarget;
                    Rebuild();
                    return;

                case Option.Explore:
                    if (_stage != Stage.Choose) return;
                    _stage = Stage.Waiting;
                    Rebuild();
                    NetBus.Request(new RestExploreReq());
                    return;

                case Option.Target:
                    if (_stage != Stage.PickTarget) return;
                    _stage = Stage.Waiting;
                    Rebuild();
                    NetBus.Request(new RestTargetReq { TargetId = target });
                    return;

                case Option.Back:
                    if (_stage != Stage.PickTarget) return;
                    _stage = Stage.Choose;
                    Rebuild();
                    return;
            }
        }
    }
}
