// 테스트용 데이터 로드. 유니티 Test Runner(작업 폴더 = 프로젝트 루트) 와 dotnet test(bin 폴더) 둘 다에서 CSV 폴더를 찾음
using System;
using System.IO;
using Terminus.Data;

namespace Terminus.Tests
{
    public static class TestData
    {
        const string RelativeDataDir = "Assets/Terminus/Resources/Data";

        public static string DataDir
        {
            get
            {
                foreach (var start in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
                {
                    var dir = new DirectoryInfo(start);
                    while (dir != null)
                    {
                        var candidate = Path.Combine(dir.FullName, RelativeDataDir);
                        if (Directory.Exists(candidate)) return candidate;
                        dir = dir.Parent;
                    }
                }
                throw new DirectoryNotFoundException(RelativeDataDir + " 를 찾지 못함");
            }
        }

        /// <summary>실제 기획 데이터(DT_*.csv) 로 GameData 를 채운다</summary>
        public static void LoadGameData()
        {
            var dir = DataDir;
            GameData.Load(name =>
            {
                var path = Path.Combine(dir, name + ".csv");
                return File.Exists(path) ? File.ReadAllText(path) : null;
            });
        }
    }
}
