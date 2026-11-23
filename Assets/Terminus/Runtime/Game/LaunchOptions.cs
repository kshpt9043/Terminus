// 실행 인자 (Run-Standalone.bat 이 넘김. 언리얼 -ExecCmds="t.MaxFPS 60" 자리)
//   -maxfps 60       프레임 제한
//   -name 이름       메인 메뉴 이름 미리 채움 (여러 개 띄울 때 구분)
//   -profile p2      세이브 / 프로필 폴더 나눔 (LocalProfile 이 읽음)
//   -nosteam         Steam 끄고 IP 모드 (SteamBootstrap 이 읽음)
using System;
using UnityEngine;

namespace Terminus.Game
{
    public static class LaunchOptions
    {
        public static string Get(string name)
        {
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; ++i)
            {
                if (string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase)) return args[i + 1];
            }
            return null;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Apply()
        {
            if (int.TryParse(Get("-maxfps"), out var fps) && fps > 0)
            {
                QualitySettings.vSyncCount = 0;
                Application.targetFrameRate = fps;
            }
            var name = Get("-name");
            if (!string.IsNullOrWhiteSpace(name)) Net.NetworkRoot.LocalPlayerName = name.Trim();
        }
    }
}
