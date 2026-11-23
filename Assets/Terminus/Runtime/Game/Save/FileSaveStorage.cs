// 세이브 파일 (언리얼 SaveGame 슬롯 자리). Application.persistentDataPath/Saves/<슬롯>.sav
// 암호화 표시는 PlayerPrefs (언리얼 GameUserSettings.ini 의 Encrypted_<슬롯>) -> 세이브 파일과 따로 있어야
// "평문으로 바꿔치기" 를 조작으로 잡을 수 있음
using System;
using System.IO;
using System.Text;
using Terminus.Run;
using UnityEngine;

namespace Terminus.Game.Save
{
    public sealed class FileSaveStorage : ISaveStorage
    {
        readonly string _dir;
        readonly string _markPrefix;

        /// <param name="profileKey">같은 PC 에서 여러 인스턴스(Multiplayer Play Mode)가 서로 세이브를 덮어쓰지 않게 나누는 이름</param>
        public FileSaveStorage(string profileKey = "")
        {
            var sub = string.IsNullOrEmpty(profileKey) ? "Saves" : Path.Combine("Saves", Sanitize(profileKey));
            _dir = Path.Combine(Application.persistentDataPath, sub);
            _markPrefix = "Terminus.Save.Encrypted." + (string.IsNullOrEmpty(profileKey) ? "" : Sanitize(profileKey) + ".");
            Directory.CreateDirectory(_dir);
        }

        public string Directory_ => _dir;

        string PathOf(string slot) => Path.Combine(_dir, Sanitize(slot) + ".sav");

        static string Sanitize(string s)
        {
            var sb = new StringBuilder();
            foreach (var c in s ?? "") sb.Append(char.IsLetterOrDigit(c) || c == '_' || c == '-' ? c : '_');
            return sb.ToString();
        }

        public bool Exists(string slot) => File.Exists(PathOf(slot));

        public bool TryRead(string slot, out byte[] data)
        {
            data = null;
            try
            {
                var path = PathOf(slot);
                if (!File.Exists(path)) return false;
                data = File.ReadAllBytes(path);
                return true;
            }
            catch (Exception e)
            {
                Debug.LogWarning($"[Save] {slot} 읽기 실패: {e.Message}");
                return false;
            }
        }

        public bool Write(string slot, byte[] data)
        {
            try
            {
                // 쓰다가 꺼져도 예전 파일이 남게 임시 파일에 쓰고 바꿔치기
                var path = PathOf(slot);
                var tmp = path + ".tmp";
                File.WriteAllBytes(tmp, data);
                if (File.Exists(path)) File.Delete(path);
                File.Move(tmp, path);
                return true;
            }
            catch (Exception e)
            {
                Debug.LogWarning($"[Save] {slot} 쓰기 실패: {e.Message}");
                return false;
            }
        }

        public bool Delete(string slot)
        {
            try
            {
                var path = PathOf(slot);
                if (!File.Exists(path)) return false;
                File.Delete(path);
                return true;
            }
            catch (Exception e)
            {
                Debug.LogWarning($"[Save] {slot} 지우기 실패: {e.Message}");
                return false;
            }
        }

        public bool HasEncryptedMark(string slot) => PlayerPrefs.GetInt(_markPrefix + Sanitize(slot), 0) == 1;

        public void SetEncryptedMark(string slot)
        {
            PlayerPrefs.SetInt(_markPrefix + Sanitize(slot), 1);
            PlayerPrefs.Save();
        }
    }

    /// <summary>JsonUtility + UTF-8. 세이브 클래스는 전부 [Serializable] / public 필드 / List</summary>
    public sealed class JsonSaveSerializer : ISaveSerializer
    {
        public byte[] Serialize<T>(T data) where T : class
        {
            return data == null ? null : Encoding.UTF8.GetBytes(JsonUtility.ToJson(data));
        }

        public T Deserialize<T>(byte[] data) where T : class
        {
            if (data == null || data.Length == 0) return null;
            try
            {
                return JsonUtility.FromJson<T>(Encoding.UTF8.GetString(data));
            }
            catch (Exception e)
            {
                Debug.LogWarning($"[Save] {typeof(T).Name} 해석 실패: {e.Message}");
                return null;
            }
        }
    }
}
