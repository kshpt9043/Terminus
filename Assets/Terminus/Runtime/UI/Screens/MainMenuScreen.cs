// 메인 메뉴 (언리얼 Lv_Lobby + UMainMenuWidget 의 시작 / 방 만들기 / 참가 부분)
// 방 목록(Steam 로비 검색)은 Steam 트랜스포트가 들어오면. 지금은 IP 로 참가
using Terminus.Game.Net;
using Terminus.Game.Save;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Screens
{
    public sealed class MainMenuScreen : VisualElement
    {
        readonly TextField _name;
        readonly TextField _roomName;
        readonly TextField _address;
        readonly Label _message;

        public MainMenuScreen()
        {
            AddToClassList("t-window");
            style.alignItems = Align.Center;
            style.justifyContent = Justify.Center;

            var panel = UiKit.Column();
            panel.style.width = 520;
            panel.style.alignItems = Align.Stretch;
            Add(panel);

            var title = UiKit.Title("TERMINUS");
            title.style.fontSize = 72;
            panel.Add(title);
            panel.Add(UiKit.Spacer(24));

            var profile = LocalProfile.Service;
            var info = UiKit.Text($"골드 {profile.Gold:N0}  ·  창고 유물 {profile.StoredRelics.Count}개" +
                                  (string.IsNullOrEmpty(LocalProfile.ProfileKey) ? "" : $"  ·  프로필 {LocalProfile.ProfileKey}"), "t-aux");
            info.style.unityTextAlign = TextAnchor.MiddleCenter;
            panel.Add(info);
            panel.Add(UiKit.Spacer(16));

            _name = new TextField("이름") { value = PlayerPrefs.GetString("Terminus.Name", "모험가") };
            _name.maxLength = 16;
            panel.Add(_name);
            panel.Add(UiKit.Spacer(16));

            panel.Add(UiKit.Hex("혼자 시작", OnSingle, "primary"));
            panel.Add(UiKit.Spacer(20));

            _roomName = new TextField("방 이름") { value = "Terminus" };
            panel.Add(_roomName);
            panel.Add(UiKit.Hex("방 만들기", OnHost, "secondary"));
            panel.Add(UiKit.Spacer(20));

            _address = new TextField("방장 IP") { value = PlayerPrefs.GetString("Terminus.LastAddress", "127.0.0.1") };
            panel.Add(_address);
            panel.Add(UiKit.Hex("참가", OnJoin, "secondary"));
            panel.Add(UiKit.Spacer(20));

            _message = UiKit.Text("", "t-warning t-wrap");
            _message.style.unityTextAlign = TextAnchor.MiddleCenter;
            panel.Add(_message);

            panel.Add(UiKit.Button("나가기", Application.Quit));

            RegisterCallback<AttachToPanelEvent>(_ => NetworkRoot.ConnectionFailed += ShowError);
            RegisterCallback<DetachFromPanelEvent>(_ => NetworkRoot.ConnectionFailed -= ShowError);
        }

        void ShowError(string message)
        {
            _message.text = message;
        }

        void SaveName()
        {
            var name = (_name.value ?? "").Trim();
            if (name.Length == 0) name = "모험가";
            NetworkRoot.LocalPlayerName = name;
            PlayerPrefs.SetString("Terminus.Name", name);
        }

        void OnSingle()
        {
            SaveName();
            _message.text = "";
            NetworkRoot.StartSingle();
        }

        void OnHost()
        {
            SaveName();
            _message.text = "";
            NetworkRoot.StartHost(_roomName.value);
        }

        void OnJoin()
        {
            SaveName();
            PlayerPrefs.SetString("Terminus.LastAddress", _address.value);
            _message.text = "참가하는 중...";
            NetworkRoot.Join(_address.value);
        }
    }
}
