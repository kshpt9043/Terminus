// Windows 빌드 (Run-Standalone.bat 이 띄우는 exe). 메뉴 Terminus/Build/Windows
// 에디터가 꺼져 있을 때 배치에서: Unity.exe -batchmode -quit -projectPath . -executeMethod Terminus.EditorTools.BuildMenu.BuildWindowsBatch
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace Terminus.EditorTools
{
    public static class BuildMenu
    {
        public const string OutputPath = "Builds/Windows/Terminus.exe";

        [MenuItem("Terminus/Build/Windows")]
        public static void BuildWindowsMenu()
        {
            var result = BuildWindows(true);
            Debug.Log(result);
            if (result.StartsWith("성공")) EditorUtility.RevealInFinder(OutputPath);
        }

        // 배치용: 실패하면 종료 코드 1
        public static void BuildWindowsBatch()
        {
            var result = BuildWindows(true);
            Debug.Log(result);
            if (!result.StartsWith("성공")) EditorApplication.Exit(1);
        }

        /// <param name="development">개발 빌드 (로그 / 디버그 콘솔). 테스트용이라 기본 켬</param>
        public static string BuildWindows(bool development)
        {
            var scenes = EditorBuildSettings.scenes.Where(s => s.enabled).Select(s => s.path).ToArray();
            if (scenes.Length == 0) return "실패: 빌드 씬 목록이 비어 있음 (메뉴 Terminus/Setup/Build Project Assets)";

            Directory.CreateDirectory(Path.GetDirectoryName(OutputPath));
            var options = new BuildPlayerOptions
            {
                scenes = scenes,
                locationPathName = OutputPath,
                target = BuildTarget.StandaloneWindows64,
                options = development ? BuildOptions.Development : BuildOptions.None,
            };
            var report = BuildPipeline.BuildPlayer(options);
            var summary = report.summary;
            return summary.result == BuildResult.Succeeded
                ? $"성공: {OutputPath} ({summary.totalSize / (1024 * 1024)} MB, {summary.totalTime.TotalSeconds:0}초)"
                : $"실패: {summary.result}, 오류 {summary.totalErrors}개 (콘솔 확인)";
        }
    }
}
