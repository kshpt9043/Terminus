// 세이브 파일 암호화 + 위조 검사 (언리얼 Game/TerminusSaveCrypto 이식). 프로필 / 런 세이브 / 세이브 목록 공통
//
// 저장: 세이브 직렬화 -> AES-256 암호화 + HMAC(SHA1) 서명 -> 슬롯 파일
//   [ "TMS1" | 원본 크기(4, 리틀 엔디안) | 서명(20) | 암호문 ]
// 읽기: 서명이 안 맞으면 조작 / 손상으로 보고 읽지 않음
//
// 목적은 세이브 편집기로 숫자만 바꾸는 가벼운 조작을 막는 것. 실행 파일에서 키를 뽑아내는 수준까지는 못 막음
//
// 예전 평문 세이브: 그 슬롯에 암호화 세이브를 한 번도 안 썼을 때만 한 번 읽어 줌 (부른 쪽이 다시 저장하면 암호화됨)
// 그 뒤로 그 슬롯에 평문이 나타나면 조작으로 봄 (암호화 파일을 지우고 편집한 평문을 넣는 우회 방지)
//
// 이식 메모: 언리얼 FAES 는 AES-256 ECB (IV 없음, 16바이트 블록마다), 남는 칸은 0 으로 채움. FSHA1::HMACBuffer 는 표준 HMAC-SHA1.
// 여기선 System.Security.Cryptography 의 같은 알고리즘 / 같은 키 / 같은 헤더라 봉투 형식은 언리얼과 같음.
// 다만 안의 평문이 언리얼은 USaveGame 직렬화, 유니티는 JSON 이라 서로의 세이브를 읽을 수는 없음
using System;
using System.Security.Cryptography;

namespace Terminus.Run
{
    public enum SaveLoadResult
    {
        NotFound,   // 파일 없음
        Ok,         // 정상
        Legacy,     // 예전 평문 세이브 (읽었음. 다시 저장해서 암호화할 것)
        Tampered,   // 서명 불일치 / 형식 오류 -> 못 읽음
    }

    public static class SaveCrypto
    {
        static readonly byte[] SaveMagic = { (byte)'T', (byte)'M', (byte)'S', (byte)'1' };
        public const int SaveHashSize = 20;   // SHA1
        public const int SaveHeaderSize = 4 + 4 + SaveHashSize;
        const int AesBlockSize = 16;
        const int KeySize = 32;

        // 키는 두 조각을 XOR 해서 만듦 (실행 파일에 키가 그대로 박히지 않게)
        static readonly byte[] PartA =
        {
            0x4A, 0xBB, 0x5E, 0x75, 0xA6, 0x91, 0x8A, 0xC2, 0x3F, 0xA4, 0x18, 0x7B, 0x56, 0x28, 0xDE, 0xF2,
            0xCC, 0x14, 0x15, 0x9E, 0xDE, 0x33, 0x12, 0x4F, 0x03, 0x28, 0x27, 0x6E, 0xDD, 0xA6, 0x95, 0x65,
        };
        static readonly byte[] PartB =
        {
            0xD3, 0x63, 0xF2, 0xC9, 0x8E, 0xE9, 0x82, 0x02, 0x17, 0xC1, 0x13, 0xAE, 0xDB, 0x11, 0x11, 0xBF,
            0x4C, 0xC1, 0xC4, 0x7B, 0x94, 0xB1, 0x36, 0x23, 0x5E, 0xE9, 0xB2, 0xFE, 0xA4, 0x5F, 0xD2, 0x08,
        };

        static byte[] BuildSaveKey()
        {
            var key = new byte[KeySize];
            for (int i = 0; i < KeySize; ++i) key[i] = (byte)(PartA[i] ^ PartB[i]);
            return key;
        }

        static byte[] Hmac(byte[] key, byte[] data, int count)
        {
            using (var h = new HMACSHA1(key))
            {
                return h.ComputeHash(data, 0, count);
            }
        }

        static byte[] AesEcb(byte[] key, byte[] data, bool encrypt)
        {
            using (var aes = Aes.Create())
            {
                aes.KeySize = KeySize * 8;
                aes.Key = key;
                aes.Mode = CipherMode.ECB;
                aes.Padding = PaddingMode.None;
                using (var t = encrypt ? aes.CreateEncryptor() : aes.CreateDecryptor())
                {
                    return t.TransformFinalBlock(data, 0, data.Length);
                }
            }
        }

        // ------------------------------------------------------------ 바이트 단위

        /// <summary>평문 -> 암호화 파일 바이트. 평문이 비면 null</summary>
        public static byte[] Encrypt(byte[] plain)
        {
            if (plain == null || plain.Length == 0) return null;

            var key = BuildSaveKey();

            // 서명은 원본 기준
            var hash = Hmac(key, plain, plain.Length);

            // AES 는 16 바이트 단위 -> 0 으로 채움 (원본 크기는 헤더에)
            int aligned = (plain.Length + AesBlockSize - 1) / AesBlockSize * AesBlockSize;
            var padded = new byte[aligned];
            Buffer.BlockCopy(plain, 0, padded, 0, plain.Length);
            var cipher = AesEcb(key, padded, true);

            var output = new byte[SaveHeaderSize + cipher.Length];
            Buffer.BlockCopy(SaveMagic, 0, output, 0, 4);
            int size = plain.Length;
            output[4] = (byte)size;
            output[5] = (byte)(size >> 8);
            output[6] = (byte)(size >> 16);
            output[7] = (byte)(size >> 24);
            Buffer.BlockCopy(hash, 0, output, 8, SaveHashSize);
            Buffer.BlockCopy(cipher, 0, output, SaveHeaderSize, cipher.Length);
            return output;
        }

        /// <summary>암호화 헤더("TMS1")로 시작하는가. 아니면 예전 평문 세이브</summary>
        public static bool HasMagic(byte[] data)
        {
            if (data == null || data.Length < SaveHeaderSize) return false;
            for (int i = 0; i < 4; ++i)
            {
                if (data[i] != SaveMagic[i]) return false;
            }
            return true;
        }

        /// <summary>암호화 파일 바이트 -> 평문. 형식이 깨졌거나 서명이 안 맞으면 false</summary>
        public static bool TryDecrypt(byte[] data, out byte[] plain)
        {
            plain = null;
            if (!HasMagic(data)) return false;

            int plainSize = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
            int cipherLength = data.Length - SaveHeaderSize;
            if (cipherLength == 0 || cipherLength % AesBlockSize != 0 || plainSize <= 0 || plainSize > cipherLength)
            {
                RunLog.Warn("[SaveCrypto] 형식이 깨짐");
                return false;
            }

            var key = BuildSaveKey();
            var cipher = new byte[cipherLength];
            Buffer.BlockCopy(data, SaveHeaderSize, cipher, 0, cipherLength);
            var decrypted = AesEcb(key, cipher, false);

            var hash = Hmac(key, decrypted, plainSize);
            int diff = 0;
            for (int i = 0; i < SaveHashSize; ++i) diff |= hash[i] ^ data[8 + i];
            if (diff != 0)
            {
                RunLog.Warn("[SaveCrypto] 서명이 안 맞음 (조작 / 손상)");
                return false;
            }

            plain = new byte[plainSize];
            Buffer.BlockCopy(decrypted, 0, plain, 0, plainSize);
            return true;
        }

        // ------------------------------------------------------------ 슬롯 단위 (언리얼 SaveToSlot / LoadFromSlot / CopySlot)

        /// <summary>암호화해서 저장. 실패하면 false</summary>
        public static bool SaveToSlot<T>(ISaveStorage storage, ISaveSerializer serializer, T save, string slot) where T : class
        {
            if (storage == null || serializer == null || save == null || string.IsNullOrEmpty(slot)) return false;

            var plain = serializer.Serialize(save);
            var output = Encrypt(plain);
            if (output == null) return false;

            if (!storage.Write(slot, output))
            {
                RunLog.Warn($"[SaveCrypto] {slot} 저장 실패");
                return false;
            }

            if (!storage.HasEncryptedMark(slot)) storage.SetEncryptedMark(slot);
            return true;
        }

        /// <summary>읽기. 정상 / 예전 평문이면 세이브 객체, 아니면 null</summary>
        public static T LoadFromSlot<T>(ISaveStorage storage, ISaveSerializer serializer, string slot, out SaveLoadResult result) where T : class
        {
            result = SaveLoadResult.NotFound;
            if (storage == null || serializer == null || !storage.Exists(slot) || !storage.TryRead(slot, out var data) || data == null)
            {
                return null;
            }

            // 예전 평문 세이브
            if (!HasMagic(data))
            {
                if (storage.HasEncryptedMark(slot))
                {
                    RunLog.Warn($"[SaveCrypto] {slot}: 암호화되지 않은 세이브 -> 조작으로 보고 읽지 않음");
                    result = SaveLoadResult.Tampered;
                    return null;
                }

                var legacy = SafeDeserialize<T>(serializer, data);
                result = legacy != null ? SaveLoadResult.Legacy : SaveLoadResult.Tampered;
                return legacy;
            }

            if (!TryDecrypt(data, out var plain))
            {
                RunLog.Warn($"[SaveCrypto] {slot}: 읽지 못함 (조작 / 손상)");
                result = SaveLoadResult.Tampered;
                return null;
            }

            var loaded = SafeDeserialize<T>(serializer, plain);
            result = loaded != null ? SaveLoadResult.Ok : SaveLoadResult.Tampered;
            return loaded;
        }

        /// <summary>슬롯 파일을 다른 슬롯으로 그대로 복사 (조작된 파일 보관 / 백업)</summary>
        public static bool CopySlot(ISaveStorage storage, string fromSlot, string toSlot)
        {
            return storage != null && storage.TryRead(fromSlot, out var data) && storage.Write(toSlot, data);
        }

        static T SafeDeserialize<T>(ISaveSerializer serializer, byte[] data) where T : class
        {
            try
            {
                return serializer.Deserialize<T>(data);
            }
            catch (Exception)
            {
                return null;
            }
        }
    }
}
