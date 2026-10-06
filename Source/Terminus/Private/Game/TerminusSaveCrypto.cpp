#include "Game/TerminusSaveCrypto.h"

#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AES.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusSaveCrypto, Log, All);

namespace
{
	constexpr uint8 SaveMagic[4] = { 'T', 'M', 'S', '1' };
	constexpr int32 SaveHashSize = 20;   // SHA1
	constexpr int32 SaveHeaderSize = 4 + 4 + SaveHashSize;

	// 슬롯마다 암호화 세이브를 한 번이라도 썼는지 (이후엔 그 슬롯의 평문 세이브를 조작으로 봄)
	// Saved/Config/.../GameUserSettings.ini. 슬롯마다인 이유: 한 슬롯을 옮긴 순간 아직 안 옮긴 다른 슬롯이 조작으로 몰리지 않게
	const TCHAR* CryptoConfigSection = TEXT("Terminus.Save");

	// 키는 두 조각을 XOR 해서 만듦 (실행 파일에 키가 그대로 박히지 않게)
	void BuildSaveKey(FAES::FAESKey& OutKey)
	{
		static const uint8 PartA[32] = {
			0x4A, 0xBB, 0x5E, 0x75, 0xA6, 0x91, 0x8A, 0xC2, 0x3F, 0xA4, 0x18, 0x7B, 0x56, 0x28, 0xDE, 0xF2,
			0xCC, 0x14, 0x15, 0x9E, 0xDE, 0x33, 0x12, 0x4F, 0x03, 0x28, 0x27, 0x6E, 0xDD, 0xA6, 0x95, 0x65 };
		static const uint8 PartB[32] = {
			0xD3, 0x63, 0xF2, 0xC9, 0x8E, 0xE9, 0x82, 0x02, 0x17, 0xC1, 0x13, 0xAE, 0xDB, 0x11, 0x11, 0xBF,
			0x4C, 0xC1, 0xC4, 0x7B, 0x94, 0xB1, 0x36, 0x23, 0x5E, 0xE9, 0xB2, 0xFE, 0xA4, 0x5F, 0xD2, 0x08 };

		for (int32 i = 0; i < FAES::FAESKey::KeySize; ++i)
		{
			OutKey.Key[i] = PartA[i] ^ PartB[i];
		}
	}

	FString EncryptedKeyFor(const FString& SlotName)
	{
		return FString::Printf(TEXT("Encrypted_%s"), *SlotName);
	}

	bool HasWrittenEncryptedSave(const FString& SlotName)
	{
		bool bEncrypted = false;
		GConfig->GetBool(CryptoConfigSection, *EncryptedKeyFor(SlotName), bEncrypted, GGameUserSettingsIni);
		return bEncrypted;
	}

	void MarkEncryptedSaveWritten(const FString& SlotName)
	{
		if (!HasWrittenEncryptedSave(SlotName))
		{
			GConfig->SetBool(CryptoConfigSection, *EncryptedKeyFor(SlotName), true, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
}

namespace TerminusSaveCrypto
{
	bool SaveToSlot(USaveGame* SaveGame, const FString& SlotName, int32 UserIndex)
	{
		TArray<uint8> Plain;
		if (!SaveGame || !UGameplayStatics::SaveGameToMemory(SaveGame, Plain) || Plain.Num() == 0)
		{
			return false;
		}

		FAES::FAESKey Key;
		BuildSaveKey(Key);

		// 서명은 원본 기준
		uint8 Hash[SaveHashSize];
		FSHA1::HMACBuffer(Key.Key, FAES::FAESKey::KeySize, Plain.GetData(), Plain.Num(), Hash);

		// AES 는 16 바이트 단위 -> 0 으로 채움 (원본 크기는 헤더에)
		TArray<uint8> Cipher = Plain;
		Cipher.SetNumZeroed(Align(Cipher.Num(), FAES::AESBlockSize));
		FAES::EncryptData(Cipher.GetData(), Cipher.Num(), Key);

		const int32 PlainSize = Plain.Num();
		TArray<uint8> Out;
		Out.Reserve(SaveHeaderSize + Cipher.Num());
		Out.Append(SaveMagic, 4);
		Out.Append(reinterpret_cast<const uint8*>(&PlainSize), 4);
		Out.Append(Hash, SaveHashSize);
		Out.Append(Cipher);

		if (!UGameplayStatics::SaveDataToSlot(Out, SlotName, UserIndex))
		{
			UE_LOG(LogTerminusSaveCrypto, Warning, TEXT("[SaveCrypto] %s 저장 실패"), *SlotName);
			return false;
		}

		MarkEncryptedSaveWritten(SlotName);
		return true;
	}

	USaveGame* LoadFromSlot(const FString& SlotName, int32 UserIndex, ELoadResult* OutResult)
	{
		auto Result = [OutResult](ELoadResult R) { if (OutResult) *OutResult = R; };

		TArray<uint8> Data;
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex) || !UGameplayStatics::LoadDataFromSlot(Data, SlotName, UserIndex))
		{
			Result(ELoadResult::NotFound);
			return nullptr;
		}

		// 예전 평문 세이브
		if (Data.Num() < SaveHeaderSize || FMemory::Memcmp(Data.GetData(), SaveMagic, 4) != 0)
		{
			if (HasWrittenEncryptedSave(SlotName))
			{
				UE_LOG(LogTerminusSaveCrypto, Warning, TEXT("[SaveCrypto] %s: 암호화되지 않은 세이브 -> 조작으로 보고 읽지 않음"), *SlotName);
				Result(ELoadResult::Tampered);
				return nullptr;
			}

			USaveGame* Legacy = UGameplayStatics::LoadGameFromMemory(Data);
			Result(Legacy ? ELoadResult::Legacy : ELoadResult::Tampered);
			return Legacy;
		}

		int32 PlainSize = 0;
		FMemory::Memcpy(&PlainSize, Data.GetData() + 4, 4);
		const uint8* StoredHash = Data.GetData() + 8;

		TArray<uint8> Cipher(Data.GetData() + SaveHeaderSize, Data.Num() - SaveHeaderSize);
		if (Cipher.Num() == 0 || Cipher.Num() % FAES::AESBlockSize != 0 || PlainSize <= 0 || PlainSize > Cipher.Num())
		{
			UE_LOG(LogTerminusSaveCrypto, Warning, TEXT("[SaveCrypto] %s: 형식이 깨짐"), *SlotName);
			Result(ELoadResult::Tampered);
			return nullptr;
		}

		FAES::FAESKey Key;
		BuildSaveKey(Key);
		FAES::DecryptData(Cipher.GetData(), Cipher.Num(), Key);
		Cipher.SetNum(PlainSize);

		uint8 Hash[SaveHashSize];
		FSHA1::HMACBuffer(Key.Key, FAES::FAESKey::KeySize, Cipher.GetData(), Cipher.Num(), Hash);
		if (FMemory::Memcmp(Hash, StoredHash, SaveHashSize) != 0)
		{
			UE_LOG(LogTerminusSaveCrypto, Warning, TEXT("[SaveCrypto] %s: 서명이 안 맞음 (조작 / 손상)"), *SlotName);
			Result(ELoadResult::Tampered);
			return nullptr;
		}

		USaveGame* Loaded = UGameplayStatics::LoadGameFromMemory(Cipher);
		Result(Loaded ? ELoadResult::Ok : ELoadResult::Tampered);
		return Loaded;
	}

	bool CopySlot(const FString& FromSlot, const FString& ToSlot, int32 UserIndex)
	{
		TArray<uint8> Data;
		return UGameplayStatics::LoadDataFromSlot(Data, FromSlot, UserIndex)
			&& UGameplayStatics::SaveDataToSlot(Data, ToSlot, UserIndex);
	}
}
