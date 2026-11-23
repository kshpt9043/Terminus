// SaveCrypto: 암호화 왕복 / 위조 검사 / 예전 평문 세이브 규칙
using System.Text;
using NUnit.Framework;
using Terminus.Run;

namespace Terminus.Tests
{
    public class RunSaveCryptoTests
    {
        [Test]
        public void EncryptDecrypt_RoundTrip_KeepsExactBytes()
        {
            foreach (int size in new[] { 1, 15, 16, 17, 1000 })
            {
                var plain = new byte[size];
                for (int i = 0; i < size; ++i) plain[i] = (byte)(i * 7 + 3);

                var file = SaveCrypto.Encrypt(plain);
                Assert.That(SaveCrypto.TryDecrypt(file, out var back), Is.True, $"size {size}");
                Assert.That(back, Is.EqualTo(plain));
            }
        }

        [Test]
        public void Encrypt_HeaderLayout_MatchesUnreal()
        {
            var plain = Encoding.UTF8.GetBytes("hello terminus");   // 14 바이트 -> 암호문 16
            var file = SaveCrypto.Encrypt(plain);

            Assert.That(Encoding.ASCII.GetString(file, 0, 4), Is.EqualTo("TMS1"));
            Assert.That(file[4] | (file[5] << 8) | (file[6] << 16) | (file[7] << 24), Is.EqualTo(14));
            Assert.That(file.Length, Is.EqualTo(SaveCrypto.SaveHeaderSize + 16));
            // 암호문은 평문과 달라야 함
            Assert.That(Encoding.UTF8.GetString(file, SaveCrypto.SaveHeaderSize, 14), Is.Not.EqualTo("hello terminus"));
        }

        [Test]
        public void TryDecrypt_RejectsTamperedOrBroken()
        {
            var file = SaveCrypto.Encrypt(Encoding.UTF8.GetBytes("gold=100"));

            var flipped = (byte[])file.Clone();
            flipped[flipped.Length - 1] ^= 0x01;
            Assert.That(SaveCrypto.TryDecrypt(flipped, out _), Is.False, "암호문 1비트");

            var badHash = (byte[])file.Clone();
            badHash[10] ^= 0x80;
            Assert.That(SaveCrypto.TryDecrypt(badHash, out _), Is.False, "서명");

            var badSize = (byte[])file.Clone();
            badSize[4] = 0xFF; badSize[5] = 0xFF;
            Assert.That(SaveCrypto.TryDecrypt(badSize, out _), Is.False, "크기가 암호문보다 큼");

            var truncated = new byte[file.Length - 3];
            System.Array.Copy(file, truncated, truncated.Length);
            Assert.That(SaveCrypto.TryDecrypt(truncated, out _), Is.False, "16 배수 아님");
        }

        [Test]
        public void Slot_RoundTrip_AndLegacyRules()
        {
            var storage = new MemorySaveStorage();
            var serializer = new RunXmlTestSerializer();
            var save = new ProfileSave { Gold = 1234 };
            save.StoredRelics.Add("RLC_Common_001");

            Assert.That(SaveCrypto.SaveToSlot(storage, serializer, save, "Profile"), Is.True);
            Assert.That(storage.HasEncryptedMark("Profile"), Is.True);

            var loaded = SaveCrypto.LoadFromSlot<ProfileSave>(storage, serializer, "Profile", out var result);
            Assert.That(result, Is.EqualTo(SaveLoadResult.Ok));
            Assert.That(loaded.Gold, Is.EqualTo(1234));
            Assert.That(loaded.StoredRelics, Is.EqualTo(new[] { "RLC_Common_001" }));

            // 없는 슬롯
            Assert.That(SaveCrypto.LoadFromSlot<ProfileSave>(storage, serializer, "Nope", out result), Is.Null);
            Assert.That(result, Is.EqualTo(SaveLoadResult.NotFound));

            // 암호화 세이브를 한 번도 안 쓴 슬롯의 평문 = 예전 세이브 -> 읽어 줌
            storage.Write("Old", serializer.Serialize(new ProfileSave { Gold = 7 }));
            loaded = SaveCrypto.LoadFromSlot<ProfileSave>(storage, serializer, "Old", out result);
            Assert.That(result, Is.EqualTo(SaveLoadResult.Legacy));
            Assert.That(loaded.Gold, Is.EqualTo(7));

            // 암호화 세이브를 쓴 뒤 나타난 평문 = 조작
            storage.Write("Profile", serializer.Serialize(new ProfileSave { Gold = 999999 }));
            Assert.That(SaveCrypto.LoadFromSlot<ProfileSave>(storage, serializer, "Profile", out result), Is.Null);
            Assert.That(result, Is.EqualTo(SaveLoadResult.Tampered));

            // 평문인데 읽을 수도 없는 쓰레기 = 조작
            storage.Write("Junk", new byte[] { 1, 2, 3 });
            Assert.That(SaveCrypto.LoadFromSlot<ProfileSave>(storage, serializer, "Junk", out result), Is.Null);
            Assert.That(result, Is.EqualTo(SaveLoadResult.Tampered));
        }

        [Test]
        public void CopySlot_CopiesBytes()
        {
            var storage = new MemorySaveStorage();
            storage.Write("A", new byte[] { 9, 8, 7 });
            Assert.That(SaveCrypto.CopySlot(storage, "A", "B"), Is.True);
            Assert.That(storage.TryRead("B", out var data), Is.True);
            Assert.That(data, Is.EqualTo(new byte[] { 9, 8, 7 }));
            Assert.That(SaveCrypto.CopySlot(storage, "Missing", "C"), Is.False);
        }
    }
}
