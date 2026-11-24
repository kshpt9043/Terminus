// 배경 확인용: 테마 배경 위에 전투 자리대로 직업 / 몬스터를 세워 실제 카메라(직교 3.2, 16:9)로 PNG 저장
// 메뉴 Terminus/Art/Render Backdrop Previews -> Temp/BackdropPreview/<테마키>.png
using System.IO;
using Terminus.Art;
using Terminus.Game.Client;
using UnityEditor;
using UnityEngine;

namespace Terminus.EditorTools
{
    public static class BackdropPreview
    {
        // AreaView 와 같은 자리 (언리얼 슬롯 / 1300 + 0.5, 발 높이 0.42)
        static readonly float[] PlayerX = { 0.1f, 0.2f, 0.3f };
        static readonly float[] MonsterX = { 0.631f, 0.762f, 0.892f };
        const float GroundY = AreaView.GroundViewY;

        [MenuItem("Terminus/Art/Render Backdrop Previews")]
        public static void RenderAllMenu()
        {
            foreach (var bd in Resources.LoadAll<StageBackdrop>(StageBackdrop.ResourceFolder))
                Debug.Log(Render(bd, "Temp/BackdropPreview/" + bd.name + ".png", "slime_blue", "slime_red", "slime_king"));
        }

        /// <summary>배경용 정렬 레이어 (Default 보다 먼저 = 뒤에 그려짐). 없으면 맨 앞에 추가</summary>
        [MenuItem("Terminus/Setup/Add Background Sorting Layer")]
        public static void EnsureSortingLayer()
        {
            var name = StageBackgroundView.SortingLayerName;
            var tagManager = new SerializedObject(AssetDatabase.LoadAllAssetsAtPath("ProjectSettings/TagManager.asset")[0]);
            var layers = tagManager.FindProperty("m_SortingLayers");
            SerializedProperty e = null;
            for (int i = 0; i < layers.arraySize; ++i)
                if (layers.GetArrayElementAtIndex(i).FindPropertyRelative("name").stringValue == name) e = layers.GetArrayElementAtIndex(i);
            // 이미 있고 ID 도 있으면 끝 (ID 0 은 Default 와 겹침)
            if (e != null && e.FindPropertyRelative("uniqueID").uintValue != 0) return;
            if (e == null)
            {
                layers.InsertArrayElementAtIndex(0);
                e = layers.GetArrayElementAtIndex(0);
                e.FindPropertyRelative("name").stringValue = name;
            }
            e.FindPropertyRelative("uniqueID").uintValue = (uint)(System.DateTime.Now.Ticks & 0x7fffffff) | 1u;
            e.FindPropertyRelative("locked").boolValue = false;
            tagManager.ApplyModifiedProperties();
            AssetDatabase.SaveAssets();
            Debug.Log("정렬 레이어 추가: " + name);
        }

        /// <summary>배경 + 플레이어 3 + 몬스터 3 을 1280x720 으로 그려 저장. 저장한 경로 반환</summary>
        public static string Render(StageBackdrop backdrop, string outPath, params string[] monsters)
        {
            EnsureSortingLayer();
            var root = new GameObject("__BackdropPreview") { hideFlags = HideFlags.HideAndDontSave };
            var rt = new RenderTexture(1280, 720, 24);
            try
            {
                var cam = new GameObject("Cam").AddComponent<Camera>();
                cam.transform.SetParent(root.transform, false);
                cam.transform.position = new Vector3(1000f, 0f, -10f);   // 열린 씬과 겹치지 않게 먼 곳
                cam.orthographic = true;
                cam.orthographicSize = 3.2f;
                cam.clearFlags = CameraClearFlags.SolidColor;
                cam.backgroundColor = Color.black;
                cam.aspect = 16f / 9f;
                cam.targetTexture = rt;

                var bg = new GameObject("Bg").AddComponent<StageBackgroundView>();
                bg.enabled = false;   // 테마 구독 없이 직접 깔기
                bg.transform.SetParent(root.transform, false);
                bg.Build(backdrop, cam);

                var fighter = Resources.Load<MonsterVisualSet>("Visuals/Characters/Fighter");
                for (int i = 0; i < PlayerX.Length; ++i)
                    Stand(root.transform, cam, PlayerX[i], fighter != null ? FirstFrame(fighter) : null, false);
                for (int i = 0; i < MonsterX.Length && i < monsters.Length; ++i)
                    Stand(root.transform, cam, MonsterX[i], FirstFrame(MonsterVisualSet.LoadBySource(monsters[i])), false);

                // 점광원 빛 모양 / 컬링 구는 LateUpdate 에서 만들어지는데, 여기선 바로 그리므로 먼저 만들어 둠
                const System.Reflection.BindingFlags Internal = System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic;
                var updateMesh = typeof(UnityEngine.Rendering.Universal.Light2D).GetMethod("UpdateMesh", Internal);
                var updateSphere = typeof(UnityEngine.Rendering.Universal.Light2D).GetMethod("UpdateBoundingSphere", Internal);
                foreach (var light in root.GetComponentsInChildren<UnityEngine.Rendering.Universal.Light2D>())
                {
                    updateMesh?.Invoke(light, new object[] { true });
                    updateSphere?.Invoke(light, null);
                }

                cam.Render();
                RenderTexture.active = rt;
                var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGB24, false);
                tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
                tex.Apply();
                RenderTexture.active = null;
                Directory.CreateDirectory(Path.GetDirectoryName(outPath));
                File.WriteAllBytes(outPath, tex.EncodeToPNG());
                Object.DestroyImmediate(tex);
                return Path.GetFullPath(outPath);
            }
            finally
            {
                Object.DestroyImmediate(root);
                rt.Release();
                Object.DestroyImmediate(rt);
            }
        }

        static Sprite FirstFrame(MonsterVisualSet set)
        {
            var fb = set != null ? set.Idle : null;
            return fb != null && fb.Frames != null && fb.Frames.Length > 0 ? fb.Frames[0] : null;
        }

        static void Stand(Transform parent, Camera cam, float vx, Sprite sprite, bool flip)
        {
            if (sprite == null) return;
            var sr = new GameObject(sprite.name).AddComponent<SpriteRenderer>();
            sr.transform.SetParent(parent, false);
            sr.sprite = sprite;
            sr.sortingOrder = 10;
            sr.flipX = flip;
            sr.transform.localScale = new Vector3(AreaView.ActorScale, AreaView.ActorScale, 1f);
            var feet = cam.ViewportToWorldPoint(new Vector3(vx, GroundY, 10f));
            sr.transform.position = new Vector3(feet.x, feet.y - sprite.bounds.min.y * AreaView.ActorScale, 0f);
        }
    }
}
