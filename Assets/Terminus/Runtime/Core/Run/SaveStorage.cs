// 세이브 파일을 어디에 / 어떤 형식으로 쓰는지 (언리얼 UGameplayStatics 슬롯 함수 + 세이브 직렬화 대체)
// Core 는 파일도 JsonUtility 도 모름 -> 유니티 쪽(Terminus.Game)이 이 두 인터페이스를 파일 + JsonUtility 로 구현해서 넣어 줌
// 테스트 / 서버는 MemorySaveStorage + 아무 직렬화기
using System;
using System.Collections.Generic;

namespace Terminus.Run
{
    /// <summary>
    /// 슬롯 이름으로 바이트를 읽고 쓰는 곳 (언리얼 SaveDataToSlot / LoadDataFromSlot / DoesSaveGameExist / DeleteGameInSlot).
    /// 유니티: Application.persistentDataPath/Saves/&lt;slot&gt;.sav 같은 파일
    /// </summary>
    public interface ISaveStorage
    {
        bool Exists(string slot);

        // 없거나 못 읽으면 false
        bool TryRead(string slot, out byte[] data);

        // 실패하면 false
        bool Write(string slot, byte[] data);

        // 지웠으면 true
        bool Delete(string slot);

        // 그 슬롯에 암호화 세이브를 한 번이라도 썼는지 (SaveCrypto 가 평문 세이브를 조작으로 볼지 정할 때)
        // 언리얼: GameUserSettings.ini [Terminus.Save] Encrypted_<슬롯>=True. 세이브 파일과 따로 있어야 의미가 있음
        // (유니티: PlayerPrefs 또는 별도 설정 파일)
        bool HasEncryptedMark(string slot);
        void SetEncryptedMark(string slot);
    }

    /// <summary>
    /// 세이브 객체 &lt;-&gt; 바이트 (언리얼 SaveGameToMemory / LoadGameFromMemory).
    /// 유니티: Encoding.UTF8 + JsonUtility.ToJson / FromJson. 세이브 클래스는 전부 [Serializable] + public 필드 + List (Dictionary 없음)
    /// </summary>
    public interface ISaveSerializer
    {
        byte[] Serialize<T>(T data) where T : class;

        // 못 읽으면 null (예외를 밖으로 던지지 않음)
        T Deserialize<T>(byte[] data) where T : class;
    }

    /// <summary>메모리 저장소 (테스트 / 데디케이티드 서버처럼 파일을 안 남길 때)</summary>
    public sealed class MemorySaveStorage : ISaveStorage
    {
        readonly Dictionary<string, byte[]> _slots = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        readonly HashSet<string> _marks = new HashSet<string>(StringComparer.Ordinal);

        public IEnumerable<string> Slots => _slots.Keys;

        public bool Exists(string slot) => !string.IsNullOrEmpty(slot) && _slots.ContainsKey(slot);

        public bool TryRead(string slot, out byte[] data)
        {
            data = null;
            if (!Exists(slot)) return false;
            data = (byte[])_slots[slot].Clone();
            return true;
        }

        public bool Write(string slot, byte[] data)
        {
            if (string.IsNullOrEmpty(slot) || data == null) return false;
            _slots[slot] = (byte[])data.Clone();
            return true;
        }

        public bool Delete(string slot) => !string.IsNullOrEmpty(slot) && _slots.Remove(slot);

        public bool HasEncryptedMark(string slot) => _marks.Contains(slot ?? "");
        public void SetEncryptedMark(string slot) => _marks.Add(slot ?? "");
    }

    // 언리얼 UE_LOG 대신. 유니티 쪽이 Debug.Log 로 이어 붙임 (안 붙이면 버림)
    public static class RunLog
    {
        public static event Action<string> Info;
        public static event Action<string> Warning;

        internal static void Log(string text) => Info?.Invoke(text);
        internal static void Warn(string text) => Warning?.Invoke(text);
    }
}
