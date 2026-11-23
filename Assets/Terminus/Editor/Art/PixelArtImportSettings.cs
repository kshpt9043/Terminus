// Assets/Terminus/Art 아래 PNG 는 픽셀아트 스프라이트로 임포트
// 언리얼 설정과 같은 뜻: Filter=Nearest, 압축 없음, 밉맵 없음 (PixelLab/PIPELINE.md 4단계)
using UnityEditor;
using UnityEngine;

namespace Terminus.Art.EditorTools
{
    public sealed class PixelArtImportSettings : AssetPostprocessor
    {
        public const string ArtRoot = "Assets/Terminus/Art/";
        // 유물 / 스킬 아이콘. 이름으로 찾아야 해서 Resources 아래 (GameIcons)
        public const string IconRoot = "Assets/Terminus/Resources/Icons/";

        // 64px 몬스터 = 1 유닛. 보스(128px)는 2 유닛
        public const float PixelsPerUnit = 64f;

        // 언리얼 UI 텍스처 (육각 버튼 판 / 지도 종이 / 전투 띠 / 배경). 픽셀아트가 아니라 부드러운 필터
        public const string UiTextureRoot = "Assets/Terminus/Resources/UI/Textures/";

        void OnPreprocessTexture()
        {
            if (assetPath.StartsWith(UiTextureRoot))
            {
                var ui = (TextureImporter)assetImporter;
                ui.textureType = TextureImporterType.Sprite;
                ui.spriteImportMode = SpriteImportMode.Single;
                ui.filterMode = FilterMode.Bilinear;
                ui.textureCompression = TextureImporterCompression.Uncompressed;
                ui.mipmapEnabled = false;
                ui.alphaIsTransparency = true;
                ui.wrapMode = TextureWrapMode.Clamp;
                ui.npotScale = TextureImporterNPOTScale.None;
                return;
            }
            if (!assetPath.StartsWith(ArtRoot) && !assetPath.StartsWith(IconRoot)) return;

            var importer = (TextureImporter)assetImporter;
            importer.textureType = TextureImporterType.Sprite;
            importer.spriteImportMode = SpriteImportMode.Single;
            importer.spritePixelsPerUnit = PixelsPerUnit;
            importer.filterMode = FilterMode.Point;
            importer.textureCompression = TextureImporterCompression.Uncompressed;
            importer.mipmapEnabled = false;
            importer.alphaIsTransparency = true;
            importer.wrapMode = TextureWrapMode.Clamp;

            var settings = new TextureImporterSettings();
            importer.ReadTextureSettings(settings);
            settings.spriteAlignment = (int)SpriteAlignment.Center;
            settings.spriteMeshType = SpriteMeshType.FullRect;
            settings.spriteExtrude = 0;
            importer.SetTextureSettings(settings);
        }
    }
}
