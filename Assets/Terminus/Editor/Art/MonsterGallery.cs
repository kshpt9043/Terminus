// 확인용 씬: 몬스터 전부를 한 줄로 세워 대기 동작 재생. Terminus/Art/Open Monster Gallery
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Terminus.Art.EditorTools
{
    public static class MonsterGallery
    {
        const string ScenePath = "Assets/Terminus/Scenes/Dev/Dev_MonsterGallery.unity";

        [MenuItem("Terminus/Art/Open Monster Gallery")]
        public static void OpenMenu() => Build();

        public static string Build()
        {
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

            var camGo = new GameObject("Main Camera");
            camGo.tag = "MainCamera";
            var cam = camGo.AddComponent<Camera>();
            cam.orthographic = true;
            cam.backgroundColor = new Color(0.12f, 0.12f, 0.16f);
            cam.clearFlags = CameraClearFlags.SolidColor;
            camGo.transform.position = new Vector3(0, 0, -10);

            var guids = AssetDatabase.FindAssets("t:MonsterVisualSet", new[] { "Assets/Terminus/Resources/Visuals/Monsters" });
            float spacing = 1.6f;
            float startX = -(guids.Length - 1) * spacing * 0.5f;
            for (int i = 0; i < guids.Length; ++i)
            {
                var visual = AssetDatabase.LoadAssetAtPath<MonsterVisualSet>(AssetDatabase.GUIDToAssetPath(guids[i]));
                var go = new GameObject(visual.name);
                go.transform.position = new Vector3(startX + i * spacing, 0, 0);
                var sr = go.AddComponent<SpriteRenderer>();
                sr.sprite = visual.Idle != null ? visual.Idle.GetFrame(0) : null;
                go.AddComponent<FlipbookPlayer>();
                var anim = go.AddComponent<MonsterAnimator>();
                var so = new SerializedObject(anim);
                so.FindProperty("_visual").objectReferenceValue = visual;
                so.ApplyModifiedPropertiesWithoutUndo();
            }
            cam.orthographicSize = Mathf.Max(2f, guids.Length * spacing / cam.aspect * 0.5f + 0.5f);

            System.IO.Directory.CreateDirectory("Assets/Terminus/Scenes/Dev");
            EditorSceneManager.SaveScene(scene, ScenePath);
            return $"{guids.Length}종 -> {ScenePath}";
        }
    }
}
