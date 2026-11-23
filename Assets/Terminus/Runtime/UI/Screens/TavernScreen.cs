// 주점 (언리얼 Lv_Tavern + UTavernWidget): 직업 고르기 / 준비 / 하드 모드(방장) / 시작(방장)
using System.Linq;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Screens
{
    public sealed class TavernScreen : VisualElement
    {
        readonly VisualElement _party;
        readonly VisualElement _classes;
        readonly Label _classInfo;
        readonly Button _ready;
        readonly Toggle _hardMode;
        readonly Button _start;
        readonly Label _roomName;
        bool _sentHello;

        public TavernScreen()
        {
            AddToClassList("t-window");
            style.flexDirection = FlexDirection.Row;
            style.paddingLeft = 80;
            style.paddingRight = 80;
            style.paddingTop = 60;
            style.paddingBottom = 60;

            // 왼쪽: 파티
            var left = UiKit.Panel();
            left.style.width = 520;
            left.style.marginRight = 40;
            Add(left);
            _roomName = UiKit.Text("주점", "t-section t-gold");
            left.Add(_roomName);
            left.Add(UiKit.Text("파티", "t-subtitle"));
            _party = UiKit.Column();
            left.Add(_party);

            // 오른쪽: 직업 / 준비
            var right = UiKit.Column();
            right.style.flexGrow = 1;
            Add(right);

            right.Add(UiKit.Title("주점"));
            right.Add(UiKit.Spacer(16));
            right.Add(UiKit.Text("직업", "t-section"));
            _classes = UiKit.Row();
            right.Add(_classes);
            foreach (CharacterClass c in System.Enum.GetValues(typeof(CharacterClass)))
            {
                var cls = c;
                var b = UiKit.Button(CharacterNames.GetClassName(c), () => NetBus.Request(new SelectCharacterReq { Class = cls }));
                b.style.width = 200;
                b.style.height = 64;
                b.style.marginRight = 12;
                b.userData = c;
                _classes.Add(b);
            }
            _classInfo = UiKit.Text("", "t-body t-wrap");
            _classInfo.style.marginTop = 12;
            _classInfo.style.minHeight = 120;
            right.Add(_classInfo);

            right.Add(UiKit.Spacer(24));
            _hardMode = new Toggle("하드 모드 (들고 간 창고 유물이 창고에서 사라짐)");
            _hardMode.RegisterValueChangedCallback(e =>
            {
                if (ClientState.IAmHost && e.newValue != ClientState.Party.bHardMode) NetBus.Request(new SetHardModeReq { bHardMode = e.newValue });
            });
            right.Add(_hardMode);
            right.Add(UiKit.Spacer(24));

            var buttons = UiKit.Row();
            _ready = UiKit.Hex("준비", () =>
            {
                var me = ClientState.Me;
                NetBus.Request(new SetReadyReq { bReady = me == null || !me.bReady });
            }, "secondary");
            buttons.Add(_ready);
            buttons.Add(UiKit.Spacer(16));
            _start = UiKit.Hex("출발", () => NetBus.Request(new StartGameReq()), "primary");
            buttons.Add(_start);
            buttons.Add(UiKit.Spacer(16));
            buttons.Add(UiKit.Button("나가기", NetworkRoot.LeaveToMenu));
            right.Add(buttons);

            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                ClientState.PartyChanged += Refresh;
                schedule.Execute(SayHello).Every(300).Until(() => _sentHello);
                Refresh();
            });
            RegisterCallback<DetachFromPanelEvent>(_ => ClientState.PartyChanged -= Refresh);
        }

        // 내 플레이어가 생기면 이름과 거점 강화를 서버에 알림 (언리얼 Server_ReportUpgrades)
        void SayHello()
        {
            if (_sentHello || TerminusPlayer.Local == null) return;
            _sentHello = true;
            NetBus.Request(new SetNameReq { Name = NetworkRoot.LocalPlayerName });
            NetBus.Request(new ReportUpgradesReq { Upgrades = LocalProfile.Service.GetAllUpgrades() });
        }

        void Refresh()
        {
            var party = ClientState.Party;
            var me = ClientState.Me;
            _roomName.text = string.IsNullOrEmpty(party.RoomName) ? "혼자 하는 게임" : party.RoomName;

            _party.Clear();
            foreach (var p in party.Players)
            {
                var row = UiKit.Row();
                row.AddToClassList("t-card");
                row.style.paddingLeft = 12;
                row.style.paddingTop = 8;
                row.style.paddingBottom = 8;
                row.style.marginBottom = 6;
                var name = UiKit.Text((p.bHost ? "★ " : "") + p.Name + (p.Id == ClientState.MyId ? " (나)" : ""), "t-body");
                name.style.width = 220;
                row.Add(name);
                var cls = UiKit.Text(CharacterNames.GetClassName(p.Class), "t-subtitle");
                cls.style.width = 140;
                row.Add(cls);
                var ready = UiKit.Text(p.bReady ? "준비 완료" : "대기", p.bReady ? "t-body" : "t-faint");
                if (p.bReady) ready.style.color = UiKit.Hex("6FB380");
                row.Add(ready);
                _party.Add(row);
            }

            var myClass = me != null ? me.Class : CharacterClass.Fighter;
            foreach (var child in _classes.Children())
            {
                bool selected = child.userData is CharacterClass c && c == myClass;
                child.style.borderTopColor = child.style.borderBottomColor = child.style.borderLeftColor = child.style.borderRightColor =
                    selected ? UiKit.Hex("C9A45C") : UiKit.Hex("3A3F4A");
                child.style.borderTopWidth = child.style.borderBottomWidth = child.style.borderLeftWidth = child.style.borderRightWidth = selected ? 3 : 1;
            }

            var row2 = GameData.FindCharacterClassRow(myClass);
            if (row2 != null)
            {
                var s = row2.BaseStats;
                var basics = GameData.FindBasicSkills(myClass);
                _classInfo.text = $"{row2.DisplayName} · {CharacterNames.GetFactionName(row2.Faction)}\n" +
                                  $"체력 {s.MaxHealth}  공격 {s.Attack}  방어 {s.Defense}  회피 {s.Evasion}  에너지 {s.MaxEnergy}\n" +
                                  (string.IsNullOrEmpty(row2.PassiveText) ? "" : row2.PassiveText + "\n") +
                                  "기본 스킬: " + string.Join(", ", basics.Select(b => b.DisplayName_KR));
            }

            bool host = me != null && me.bHost;
            _hardMode.SetValueWithoutNotify(party.bHardMode);
            _hardMode.SetEnabled(host);
            _ready.text = me != null && me.bReady ? "준비 취소" : "준비";
            _start.style.display = host ? DisplayStyle.Flex : DisplayStyle.None;
            _start.SetEnabled(party.Players.Count > 0 && party.Players.All(p => p.bReady || !p.bConnected));
        }
    }
}
