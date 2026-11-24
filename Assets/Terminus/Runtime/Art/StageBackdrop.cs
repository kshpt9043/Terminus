// 테마 하나의 전투 배경: 베이스 한 장 + 그 위에 얹는 구조물들. Resources/Visuals/Backdrops/<테마키>.asset
// 자리는 화면 비율(0~1, 왼쪽 아래 0,0)로 적어 둠 -> 화면 비율이 달라도 같은 구도
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Terminus.Art
{
    [CreateAssetMenu(menuName = "Terminus/Stage Backdrop", fileName = "BD_New")]
    public sealed class StageBackdrop : ScriptableObject
    {
        public const string ResourceFolder = "Visuals/Backdrops";

        [Serializable]
        public struct Prop
        {
            public Sprite Sprite;
            [Tooltip("그림 발밑 가운데가 닿는 화면 비율 자리 (0~1)")]
            public Vector2 ViewPos;
            [Tooltip("뒤 -100 (베이스) ~ 앞 -1. 캐릭터는 10")]
            public int Order;
            public bool FlipX;
        }

        // 점광원 (등불 / 결정처럼 빛나는 자리). 배경과 캐릭터 모두 비춤
        [Serializable]
        public struct Glow
        {
            [Tooltip("빛 가운데 화면 비율 자리 (0~1)")]
            public Vector2 ViewPos;
            public Color Color;
            public float Intensity;
            [Tooltip("빛이 닿는 반경 (유닛, 화면 높이 6.4)")]
            public float Radius;
        }

        [Tooltip("몬스터 DT 의 Theme_KR 과 같은 글자 (예: 슬라임 왕국)")]
        public string ThemeName = "";

        [Tooltip("하늘 / 원경 / 땅까지 그린 바탕. 화면 높이에 맞춰 늘림")]
        public Sprite Base;

        [Tooltip("베이스 그림에서 발 딛는 바닥 줄 (위에서 픽셀). 0 이면 화면 가운데에 맞춤, 아니면 이 줄이 캐릭터 발 높이에 오게 올리고 내림")]
        public float BaseGroundLine;

        public List<Prop> Props = new List<Prop>();

        [Header("환경광 (Light2D)")]
        [Tooltip("배경 레이어 전체에 까는 빛 (분위기)")]
        public Color AmbientColor = Color.white;
        public float AmbientIntensity = 1f;
        [Tooltip("캐릭터 / 몬스터에 까는 빛. 배경보다 밝게 둬야 잘 보임")]
        public Color ActorLightColor = Color.white;
        public float ActorLightIntensity = 1f;
        public List<Glow> Glows = new List<Glow>();
        [Tooltip("밝은 곳 번짐 (0 = 끔)")]
        public float BloomIntensity;

        public static StageBackdrop LoadByTheme(string themeName)
        {
            if (string.IsNullOrEmpty(themeName)) return null;
            return Array.Find(Resources.LoadAll<StageBackdrop>(ResourceFolder), b => b.ThemeName == themeName);
        }
    }
}
