// 프로젝트 기본 에셋을 코드로 만듦 (손으로 만들면 빠뜨리기 쉬운 것들). 메뉴 Terminus/Setup/Build Project Assets
//  - 네트워크 플레이어 프리팹 (Resources/Net/TerminusPlayer)
//  - UI 패널 설정 + 테마 (Resources/UI/TerminusPanelSettings, 1920x1080 기준)
//  - 씬 3개 (MainMenu / Tavern / Dungeon) + 빌드 씬 목록
// CLI: unity command script eval --code 'return Terminus.EditorTools.ProjectSetup.BuildAll();'
using System.Collections.Generic;
using System.IO;
using Terminus.Game.Net;
using Unity.Netcode;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.EditorTools
{
    public static class ProjectSetup
    {
        const string PlayerPrefabPath = "Assets/Terminus/Resources/Net/TerminusPlayer.prefab";
        const string PanelSettingsPath = "Assets/Terminus/Resources/UI/TerminusPanelSettings.asset";
        const string ThemePath = "Assets/Terminus/UI/Theme/TerminusRuntimeTheme.tss";
        const string ScenesDir = "Assets/Terminus/Scenes";

        static readonly string[] SceneOrder = { SceneNames.MainMenu, SceneNames.Tavern, SceneNames.Dungeon };

        [MenuItem("Terminus/Setup/Build Project Assets")]
        public static void BuildAllMenu() => Debug.Log(BuildAll());

        public static string BuildAll()
        {
            var log = new List<string>
            {
                BuildPlayerPrefab(),
                BuildPanelSettings(),
            };
            log.AddRange(BuildScenes());
            AssetDatabase.SaveAssets();
            return string.Join("\n", log);
        }

        static void EnsureFolder(string path)
        {
            if (AssetDatabase.IsValidFolder(path)) return;
            var parent = Path.GetDirectoryName(path).Replace('\\', '/');
            EnsureFolder(parent);
            AssetDatabase.CreateFolder(parent, Path.GetFileName(path));
        }

        static string BuildPlayerPrefab()
        {
            EnsureFolder(Path.GetDirectoryName(PlayerPrefabPath).Replace('\\', '/'));
            if (AssetDatabase.LoadAssetAtPath<GameObject>(PlayerPrefabPath) != null) return "플레이어 프리팹: 이미 있음";
            var go = new GameObject("TerminusPlayer");
            go.AddComponent<NetworkObject>();
            go.AddComponent<TerminusPlayer>();
            PrefabUtility.SaveAsPrefabAsset(go, PlayerPrefabPath);
            Object.DestroyImmediate(go);
            return "플레이어 프리팹: " + PlayerPrefabPath;
        }

        static string BuildPanelSettings()
        {
            EnsureFolder(Path.GetDirectoryName(PanelSettingsPath).Replace('\\', '/'));

            // 기본 런타임 테마 + 우리 테마 (USS 변수 / 공통 클래스)
            if (!File.Exists(ThemePath))
            {
                File.WriteAllText(ThemePath,
                    "@import url(\"unity-theme://default\");\n" +
                    "@import url(\"project://database/Assets/Terminus/UI/Theme/Terminus.uss\");\n");
                AssetDatabase.ImportAsset(ThemePath);
            }
            var theme = AssetDatabase.LoadAssetAtPath<ThemeStyleSheet>(ThemePath);

            var ps = AssetDatabase.LoadAssetAtPath<PanelSettings>(PanelSettingsPath);
            if (ps == null)
            {
                ps = ScriptableObject.CreateInstance<PanelSettings>();
                AssetDatabase.CreateAsset(ps, PanelSettingsPath);
            }
            ps.themeStyleSheet = theme;
            ps.scaleMode = PanelScaleMode.ScaleWithScreenSize;
            ps.referenceResolution = new Vector2Int(1920, 1080);
            ps.screenMatchMode = PanelScreenMatchMode.MatchWidthOrHeight;
            ps.match = 0.5f;
            EditorUtility.SetDirty(ps);
            return "UI 패널 설정: " + PanelSettingsPath + (theme == null ? " (테마 임포트 실패)" : "");
        }

        static IEnumerable<string> BuildScenes()
        {
            EnsureFolder(ScenesDir);
            var buildScenes = new List<EditorBuildSettingsScene>();
            foreach (var name in SceneOrder)
            {
                var path = ScenesDir + "/" + name + ".unity";
                if (!File.Exists(path))
                {
                    var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                    var camGo = new GameObject("Main Camera");
                    camGo.tag = "MainCamera";
                    var cam = camGo.AddComponent<Camera>();
                    cam.orthographic = true;
                    cam.orthographicSize = 3.2f;
                    cam.clearFlags = CameraClearFlags.SolidColor;
                    cam.backgroundColor = new Color(0.067f, 0.075f, 0.09f);
                    camGo.transform.position = new Vector3(0, 0, -10);
                    EditorSceneManager.SaveScene(scene, path);
                    yield return "씬 생성: " + path;
                }
                buildScenes.Add(new EditorBuildSettingsScene(path, true));
            }
            EditorBuildSettings.scenes = buildScenes.ToArray();
            yield return "빌드 씬 목록: " + string.Join(", ", SceneOrder);
        }
    }
}
