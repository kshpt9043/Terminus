// 이공간 (언리얼 URiftWidget / WBP_Rift). 멀티에서 누가 진행 중에 나갔을 때 화면 전체를 덮고 나간 사람이 돌아오길 기다림
//  - 그동안 세션은 주점 목록에 '재합류 대기' 로 뜸. 나간 사람이 그걸 찾아 들어오면 전원 지도로
//  - 싸우던 방은 무효 (들어가기 전으로 롤백)
//  - 기다리기 싫으면 나가기 (방장이 나가면 던전이 끝남)
// 띄우고 닫는 건 EnterRiftMsg / LeaveRiftMsg (언리얼 PC::Client_EnterRift / Client_LeaveRift). ESC 로는 안 닫힘 (UILayer.Overlay -> 채팅은 위라 기다리면서 채팅 가능)
//
// WBP 구조: RiftDim (선형 0.05, 0.02, 0.09, 0.93) + 가운데 ContentBox (폭 900)
//   TitleText 40 #E6C47A / MessageText 16 #C9C1AE (위 16) / WaitingText 17 #C9A45C (위 20) / HintText 13 #8D8778 (위 8) / LeaveButton (위 32)
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class RiftWidget : VisualElement
    {
        const string LeftMessage = "동료와의 연결이 끊겨 이공간에 빠졌습니다.\n싸우던 방은 없던 일이 되었습니다. 동료가 돌아오면 지도로 돌아갑니다.";
        const string FromSaveMessage = "세이브를 불러와 이공간에서 동료를 기다립니다.\n세이브 당시 동료가 모두 들어오면 지도로 갑니다.";

        readonly Label _titleText;
        readonly Label _messageText;
        readonly Label _waitingText;
        readonly Button _leaveButton;

        public RiftWidget()
        {
            DungeonStyle.Use(this);
            name = "Rift";
            DungeonStyle.Fill(this);
            // 아래 지도 / 전투 화면을 못 누르게
            pickingMode = PickingMode.Position;
            RegisterCallback<PointerDownEvent>(e => e.StopPropagation());

            var dim = new VisualElement { name = "RiftDim", pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(dim);
            dim.style.backgroundColor = UiColor.FromLinear(0.05f, 0.02f, 0.09f, 0.93f);
            Add(dim);

            var center = new VisualElement { pickingMode = PickingMode.Ignore };
            DungeonStyle.Fill(center);
            center.style.alignItems = Align.Center;
            center.style.justifyContent = Justify.Center;
            Add(center);

            var content = new VisualElement { name = "ContentBox", pickingMode = PickingMode.Ignore };
            content.style.width = 900f;
            content.style.alignItems = Align.Center;
            center.Add(content);

            _titleText = UiStyle.WrapText("이공간", 40, UiColor.FromLinear(0.791298f, 0.552011f, 0.194618f), TextAnchor.MiddleCenter);
            content.Add(_titleText);

            _messageText = UiStyle.WrapText(LeftMessage, 16, UiColor.FromLinear(0.584078f, 0.533276f, 0.423268f), TextAnchor.MiddleCenter);
            _messageText.style.marginTop = 16f;
            content.Add(_messageText);

            _waitingText = UiStyle.WrapText("", 17, UiColor.FromLinear(0.584078f, 0.371238f, 0.107023f), TextAnchor.MiddleCenter);
            _waitingText.style.marginTop = 20f;
            content.Add(_waitingText);

            var hint = UiStyle.WrapText("나간 동료는 주점 목록의 '재합류 대기' 방으로 돌아올 수 있습니다.", 13, UiColor.FromLinear(0.266356f, 0.242281f, 0.187821f), TextAnchor.MiddleCenter);
            hint.style.marginTop = 8f;
            content.Add(hint);

            _leaveButton = new Button(HandleLeaveClicked) { name = "LeaveButton", text = "던전 나가기" };
            _leaveButton.AddToClassList("dm-rift-leave");
            content.Add(_leaveButton);
        }

        /// <summary>돌아오길 기다리는 사람 명단</summary>
        public void SetWaiting(System.Collections.Generic.IReadOnlyList<string> waitingFor)
        {
            _waitingText.text = "기다리는 중: " + string.Join(", ", waitingFor ?? System.Array.Empty<string>());
        }

        /// <summary>멀티 세이브를 불러와 모이는 중이면 설명 문구가 다름 (누가 나간 게 아님)</summary>
        public void SetFromSave(bool fromSave)
        {
            _messageText.text = fromSave ? FromSaveMessage : LeftMessage;
        }

        void HandleLeaveClicked()
        {
            bool host = ClientState.IAmHost || NetBus.IsServer;
            ConfirmPopupWidget.Show("던전 나가기",
                host ? "방장이 나가면 던전이 끝나고 모두 메인 화면으로 돌아갑니다. 나갈까요?"
                     : "던전을 나갈까요? 나가면 다른 동료들도 이공간에서 기다리게 됩니다.",
                NetworkRoot.LeaveToMenu, "나가기", "취소");
        }
    }
}
