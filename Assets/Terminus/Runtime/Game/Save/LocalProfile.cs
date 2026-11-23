// 이 컴퓨터의 영구 프로필 (골드 / 창고 유물 / 보유 스킬 / 거점 강화). 언리얼 UTerminusProfileSubsystem 자리
// 멀티에서도 각자 자기 프로필을 씀 -> 서버에는 강화 단계만 알려 줌 (ReportUpgradesReq)
using System;
using Terminus.Data;
using Terminus.Run;
using UnityEngine;

namespace Terminus.Game.Save
{
    public static class LocalProfile
    {
        static ProfileService _service;
        static FileSaveStorage _storage;
        static readonly JsonSaveSerializer Serializer = new JsonSaveSerializer();

        /// <summary>Multiplayer Play Mode 가상 플레이어끼리 세이브가 섞이지 않게 (명령줄 -profile 이름 또는 MPPM 태그)</summary>
        public static string ProfileKey { get; private set; } = "";

        public static ISaveStorage Storage
        {
            get
            {
                if (_storage == null) _storage = new FileSaveStorage(ProfileKey);
                return _storage;
            }
        }

        public static ISaveSerializer SaveSerializer => Serializer;

        public static ProfileService Service
        {
            get
            {
                if (_service == null)
                {
                    _service = new ProfileService(Storage, Serializer, new System.Random(), GameData.Upgrades);
                    _service.Load();
                }
                return _service;
            }
        }

        /// <summary>프로필 이름을 바꿈 (처음 쓰기 전에). 가상 플레이어마다 다른 세이브 폴더</summary>
        public static void SetProfileKey(string key)
        {
            if (key == ProfileKey) return;
            ProfileKey = key ?? "";
            _storage = null;
            _service = null;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadCommandLine()
        {
            // 빌드 여러 개를 띄울 때: Terminus.exe -profile p2
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; ++i)
            {
                if (args[i] == "-profile") SetProfileKey(args[i + 1]);
            }
            RunLog.Info -= LogInfo;
            RunLog.Info += LogInfo;
            RunLog.Warning -= LogWarn;
            RunLog.Warning += LogWarn;
        }

        static void LogInfo(string s) => Debug.Log(s);
        static void LogWarn(string s) => Debug.LogWarning(s);
    }
}
