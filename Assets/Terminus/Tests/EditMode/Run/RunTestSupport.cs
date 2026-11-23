// Run 테스트 공용: 테스트용 직렬화기 (XmlSerializer - 유니티 Mono 와 .NET 둘 다 있음. 진짜 게임은 JsonUtility)
using System.IO;
using System.Text;
using System.Xml.Serialization;
using Terminus.Run;

namespace Terminus.Tests
{
    public sealed class RunXmlTestSerializer : ISaveSerializer
    {
        public byte[] Serialize<T>(T data) where T : class
        {
            var serializer = new XmlSerializer(typeof(T));
            using (var ms = new MemoryStream())
            {
                using (var writer = new StreamWriter(ms, new UTF8Encoding(false)))
                {
                    serializer.Serialize(writer, data);
                }
                return ms.ToArray();
            }
        }

        public T Deserialize<T>(byte[] data) where T : class
        {
            try
            {
                var serializer = new XmlSerializer(typeof(T));
                using (var ms = new MemoryStream(data))
                {
                    return serializer.Deserialize(ms) as T;
                }
            }
            catch (System.Exception)
            {
                return null;
            }
        }
    }
}
