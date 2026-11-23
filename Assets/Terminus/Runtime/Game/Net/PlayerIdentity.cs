// 접속할 때 보내는 내 정보 (언리얼 UniqueNetId + 접속 옵션 ?pw=)
//  - OnlineId: 다시 들어왔을 때 같은 사람인지 맞춰 보는 값 (이공간 재합류 / 멀티 세이브 이어하기). Steam 이면 SteamId, 아니면 설치별 고유값
//  - 비밀번호 / 이름
using System;
using System.Text;
using UnityEngine;

namespace Terminus.Game.Net
{
    [Serializable]
    public sealed class ConnectPayload
    {
        public string Password = "";
        public string OnlineId = "";
        public string Name = "";

        public byte[] ToBytes() => Encoding.UTF8.GetBytes(JsonUtility.ToJson(this));

        public static ConnectPayload FromBytes(byte[] bytes)
        {
            if (bytes == null || bytes.Length == 0) return new ConnectPayload();
            try
            {
                return JsonUtility.FromJson<ConnectPayload>(Encoding.UTF8.GetString(bytes)) ?? new ConnectPayload();
            }
            catch
            {
                return new ConnectPayload();
            }
        }
    }

    public static class PlayerIdentity
    {
        /// <summary>내 온라인 아이디. Steam 모드면 SteamId, 아니면 이 컴퓨터(프로필)마다 한 번 만들어 둔 값</summary>
        public static string OnlineId
        {
            get
            {
                if (NetworkRoot.Mode == TransportMode.Steam && Steam.SteamBootstrap.Available)
                {
                    return "steam:" + Steamworks.SteamClient.SteamId.Value;
                }
                var key = "Terminus.OnlineId." + Save.LocalProfile.ProfileKey;
                var id = PlayerPrefs.GetString(key, "");
                if (string.IsNullOrEmpty(id))
                {
                    id = "local:" + Guid.NewGuid().ToString("N");
                    PlayerPrefs.SetString(key, id);
                    PlayerPrefs.Save();
                }
                return id;
            }
        }

        public static ConnectPayload MakePayload(string password) => new ConnectPayload
        {
            Password = password ?? "",
            OnlineId = OnlineId,
            Name = NetworkRoot.LocalPlayerName ?? "",
        };
    }
}
