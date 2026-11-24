// [클라] 전투 무대 뒤 배경: 지금 층 테마의 StageBackdrop (베이스 + 구조물 + 환경광) 을 카메라에 맞춰 깔기
//  - 베이스는 화면을 꽉 채우게 늘림 (비율이 달라 남는 쪽은 잘림), 구조물은 화면 비율 자리에 발밑을 맞춤
//  - 배경 그림은 정렬 레이어 Background (캐릭터 / 몬스터의 Default 보다 항상 뒤)
//  - 빛: 배경 레이어에 분위기 전역광, Default 레이어에 캐릭터용 전역광 (따로 둬야 배경을 어둡게 해도 캐릭터가 잘 보임),
//        점광원은 두 레이어 모두. 블룸은 이 카메라 후처리
using Terminus.Art;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Terminus.Game.Client
{
    public sealed class StageBackgroundView : MonoBehaviour
    {
        public const string SortingLayerName = "Background";

        StageBackdrop _shown;
        float _shownAspect;
        VolumeProfile _profile;

        public static StageBackgroundView Create()
        {
            var go = new GameObject("StageBackground");
            return go.AddComponent<StageBackgroundView>();
        }

        void OnEnable()
        {
            ClientState.MapChanged += Refresh;
            Refresh();
        }

        void OnDisable() => ClientState.MapChanged -= Refresh;

        void OnDestroy()
        {
            if (_profile != null) DestroyImmediate(_profile);
        }

        // 창 크기가 바뀌면 다시 맞춤
        void LateUpdate()
        {
            var cam = Camera.main;
            if (cam != null && _shown != null && !Mathf.Approximately(cam.aspect, _shownAspect)) Build(_shown, cam);
        }

        void Refresh()
        {
            if (this == null) return;
            var backdrop = StageBackdrop.LoadByTheme(ClientState.Map != null ? ClientState.Map.ThemeName : "");
            if (backdrop != _shown) Build(backdrop, Camera.main);
        }

        /// <summary>배경을 지우고 다시 깔기 (에디터 미리보기도 씀)</summary>
        public void Build(StageBackdrop backdrop, Camera cam)
        {
            for (int i = transform.childCount - 1; i >= 0; --i)
            {
                var child = transform.GetChild(i).gameObject;
                if (Application.isPlaying) Destroy(child); else DestroyImmediate(child);
            }
            _shown = backdrop;
            if (backdrop == null || cam == null) return;
            _shownAspect = cam.aspect;

            float camH = cam.orthographicSize * 2f;
            float camW = camH * cam.aspect;
            var center = cam.transform.position;
            center.z = 0f;

            if (backdrop.Base != null)
            {
                var size = backdrop.Base.bounds.size;
                float scale = Mathf.Max(camW / size.x, camH / size.y);
                var sr = MakeSprite("Base", backdrop.Base, -100);
                var pos = center;
                if (backdrop.BaseGroundLine > 0f)
                {
                    // 바닥 줄이 발 높이에 오게. 그만큼 위아래가 모자라면 남는 쪽이 생기지 않게 더 키움
                    float lineFromCenter = (size.y * 0.5f - backdrop.BaseGroundLine / backdrop.Base.pixelsPerUnit);   // 베이스 안 (배율 1)
                    float target = (AreaView.GroundViewY - 0.5f) * camH;                                               // 카메라 가운데 기준
                    float shift = target - lineFromCenter * scale;
                    for (int i = 0; i < 4 && size.y * scale < Mathf.Abs(shift) * 2f + camH; ++i)
                    {
                        scale = (Mathf.Abs(shift) * 2f + camH) / size.y;
                        shift = target - lineFromCenter * scale;
                    }
                    pos.y += shift;
                }
                sr.transform.position = pos;
                sr.transform.localScale = new Vector3(scale, scale, 1f);
            }

            foreach (var prop in backdrop.Props)
            {
                if (prop.Sprite == null) continue;
                var sr = MakeSprite(prop.Sprite.name, prop.Sprite, prop.Order);
                sr.flipX = prop.FlipX;
                // 피벗이 가운데여도 발밑이 그 자리에 닿게
                sr.transform.position = ViewToWorld(cam, prop.ViewPos) + new Vector3(0f, -prop.Sprite.bounds.min.y, 0f);
            }

            BuildLights(backdrop, cam);
        }

        void BuildLights(StageBackdrop backdrop, Camera cam)
        {
            int bgLayer = SortingLayer.NameToID(SortingLayerName);
            if (!SortingLayer.IsValid(bgLayer) || bgLayer == 0)
            {
                // 레이어가 없으면 배경용 어두운 빛이 캐릭터까지 덮음 -> 빛 없이 (메뉴 Terminus/Setup 에서 레이어 추가)
                Debug.LogWarning("[Backdrop] 정렬 레이어 '" + SortingLayerName + "' 가 없어 환경광을 끔");
                return;
            }
            int actorLayer = SortingLayer.NameToID("Default");

            MakeLight("AmbientLight", Light2D.LightType.Global, backdrop.AmbientColor, backdrop.AmbientIntensity, new[] { bgLayer });
            MakeLight("ActorLight", Light2D.LightType.Global, backdrop.ActorLightColor, backdrop.ActorLightIntensity, new[] { actorLayer });
            foreach (var glow in backdrop.Glows)
            {
                var light = MakeLight("Glow", Light2D.LightType.Point, glow.Color, glow.Intensity, new[] { bgLayer, actorLayer });
                light.transform.position = ViewToWorld(cam, glow.ViewPos);
                light.pointLightOuterRadius = glow.Radius;
                light.pointLightInnerRadius = glow.Radius * 0.15f;
                light.falloffIntensity = 0.6f;
            }

            if (backdrop.BloomIntensity > 0f)
            {
                if (_profile == null)
                {
                    _profile = ScriptableObject.CreateInstance<VolumeProfile>();
                    _profile.hideFlags = HideFlags.DontSave;
                    _profile.Add<Bloom>(true);
                }
                _profile.TryGet(out Bloom bloom);
                bloom.intensity.Override(backdrop.BloomIntensity);
                bloom.threshold.Override(0.75f);
                bloom.scatter.Override(0.55f);
                var volGo = new GameObject("Bloom");
                volGo.transform.SetParent(transform, false);
                var vol = volGo.AddComponent<Volume>();
                vol.isGlobal = true;
                vol.sharedProfile = _profile;
                cam.GetUniversalAdditionalCameraData().renderPostProcessing = true;
            }
        }

        Light2D MakeLight(string name, Light2D.LightType type, Color color, float intensity, int[] layers)
        {
            var go = new GameObject(name);
            go.transform.SetParent(transform, false);
            var light = go.AddComponent<Light2D>();
            light.lightType = type;
            light.color = color;
            light.intensity = intensity;
            light.targetSortingLayers = layers;
            return light;
        }

        static Vector3 ViewToWorld(Camera cam, Vector2 view)
        {
            var p = cam.ViewportToWorldPoint(new Vector3(view.x, view.y, Mathf.Abs(cam.transform.position.z)));
            p.z = 0f;
            return p;
        }

        SpriteRenderer MakeSprite(string name, Sprite sprite, int order)
        {
            var go = new GameObject(name);
            go.transform.SetParent(transform, false);
            var sr = go.AddComponent<SpriteRenderer>();
            sr.sprite = sprite;
            sr.sortingLayerName = SortingLayerName;
            sr.sortingOrder = order;
            return sr;
        }
    }
}
