// 색 도우미 + 팔레트 (언리얼 Widgets/TerminusUIColors.h 이식)
//  - UI Toolkit 의 Color 는 sRGB 값. WBP 덤프의 SpecifiedColor / BrushColor 는 선형(linear) 값이라 FromLinear 로 바꿔서 쓸 것
//  - 팔레트 값은 Theme/Terminus.uss 의 --t-* 변수와 같음. USS 에서는 변수를, C# 에서 계산할 때만 여기를
using UnityEngine;

namespace Terminus.UI.Common
{
    public static class UiColor
    {
        // ------------------------------------------------------------ 변환

        /// <summary>선형 채널 하나 -> sRGB (정확한 식. c^(1/2.2) 근사 아님)</summary>
        public static float LinearToSrgb(float c)
        {
            if (c <= 0f) return 0f;
            if (c >= 1f) return 1f;
            return c <= 0.0031308f ? 12.92f * c : 1.055f * Mathf.Pow(c, 1f / 2.4f) - 0.055f;
        }

        /// <summary>WBP 덤프의 선형 색 (SpecifiedColor=(R=..,G=..,B=..,A=..)) -> UI Toolkit 색. 알파는 그대로</summary>
        public static Color FromLinear(float r, float g, float b, float a = 1f) =>
            new Color(LinearToSrgb(r), LinearToSrgb(g), LinearToSrgb(b), a);

        /// <summary>"C9A45C" / "#C9A45C" -> 색 (언리얼 TerminusUI::Hex). alpha 를 주면 덮어씀</summary>
        public static Color Hex(string hex, float alpha = -1f)
        {
            if (string.IsNullOrEmpty(hex)) return Color.magenta;
            ColorUtility.TryParseHtmlString(hex[0] == '#' ? hex : "#" + hex, out var c);
            if (alpha >= 0f) c.a = alpha;
            return c;
        }

        public static Color WithAlpha(Color c, float a)
        {
            c.a = a;
            return c;
        }

        public static readonly Color Clear = new Color(0f, 0f, 0f, 0f);

        // ------------------------------------------------------------ 팔레트 (스타일 가이드 보드. "이 값만 사용")

        // 배경 / 판
        public static readonly Color Inset = Hex("111317");        // 칸 안쪽 / 빈 칸
        public static readonly Color Board = Hex("1B1E25");        // 기본 판 / 툴팁
        public static readonly Color Section = Hex("23272E");      // 섹션
        public static readonly Color Card = Hex("262A32");         // 칸 / 카드
        public static readonly Color Disabled = Hex("2E333D");     // 비활성 / 호버 바탕
        public static readonly Color Line = Hex("3A3F4A");         // 테두리

        // 글자
        public static readonly Color TextBody = Hex("ECE3CF");     // 본문
        public static readonly Color TextSub = Hex("C9C1AE");      // 부제목
        public static readonly Color TextAux = Hex("A79F8E");      // 보조
        public static readonly Color TextFaint = Hex("8D8778");    // 흐림

        // 금 / 상태
        public static readonly Color GoldHover = Hex("E6C47A");    // 호버 / 선택 글자
        public static readonly Color Gold = Hex("C9A45C");         // 기본 금 (제목 / 구분선) = GoldBase
        public static readonly Color GoldPressed = Hex("A8894A");  // 눌림 / 어두운 금
        public static readonly Color Ready = Hex("6FB380");        // 준비 / 선택됨
        public static readonly Color Alert = Hex("C23B33");        // 경고 / 심층

        // 버튼 바탕 (육각 플레이트 안쪽색)
        public static readonly Color PrimaryFill = Hex("A8322C");
        public static readonly Color SecondaryFill = Hex("3F7F86");
        public static readonly Color CurrencyFill = Hex("A88100");

        // 스킬 종류 색 중 방어 (팔레트 밖 한 색. TerminusUIText.cpp)
        public static readonly Color DefenseTeal = Hex("4A929A");

        // 배경
        public static readonly Color Dim = new Color(0f, 0f, 0f, 0.7f);
        public static readonly Color DimStrong = new Color(0f, 0f, 0f, 0.8f);
        public static readonly Color Window = Hex("111317", 0.98f);
    }
}
