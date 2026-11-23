// 언리얼 Paper2D 플립북(UPaperFlipbook) 대신. 스프라이트 배열 + 초당 프레임
using UnityEngine;

namespace Terminus.Art
{
    [CreateAssetMenu(menuName = "Terminus/Sprite Flipbook", fileName = "FB_New")]
    public sealed class SpriteFlipbook : ScriptableObject
    {
        public Sprite[] Frames = new Sprite[0];

        // PixelLab 몬스터는 전부 10 (언리얼 FramesPerSecond=10)
        public float FramesPerSecond = 10f;

        public int FrameCount => Frames != null ? Frames.Length : 0;

        public float Duration => FramesPerSecond > 0f ? FrameCount / FramesPerSecond : 0f;

        public Sprite GetFrame(int index)
        {
            if (FrameCount == 0) return null;
            return Frames[Mathf.Clamp(index, 0, FrameCount - 1)];
        }
    }
}
