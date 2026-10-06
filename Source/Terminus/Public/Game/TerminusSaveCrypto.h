#pragma once

#include "CoreMinimal.h"

class USaveGame;

/**
 * 세이브 파일 암호화 + 위조 검사 (프로필 / 런 세이브 / 세이브 목록 공통)
 *
 * 저장: 언리얼 세이브 직렬화 -> AES-256 암호화 + HMAC(SHA1) 서명 -> 슬롯 파일
 *   [ "TMS1" | 원본 크기(4) | 서명(20) | 암호문 ]
 * 읽기: 서명이 안 맞으면 조작 / 손상으로 보고 읽지 않음
 *
 * 목적은 세이브 편집기로 숫자만 바꾸는 가벼운 조작을 막는 것. 실행 파일에서 키를 뽑아내는 수준까지는 못 막음
 *
 * 예전 평문 세이브: 그 슬롯에 암호화 세이브를 한 번도 안 썼을 때만 한 번 읽어 줌 (부른 쪽이 다시 저장하면 암호화됨)
 * 그 뒤로 그 슬롯에 평문이 나타나면 조작으로 봄 (암호화 파일을 지우고 편집한 평문을 넣는 우회 방지)
 */
namespace TerminusSaveCrypto
{
	enum class ELoadResult : uint8
	{
		NotFound,   // 파일 없음
		Ok,         // 정상
		Legacy,     // 예전 평문 세이브 (읽었음. 다시 저장해서 암호화할 것)
		Tampered    // 서명 불일치 / 형식 오류 -> 못 읽음
	};

	// 암호화해서 저장. 실패하면 false
	TERMINUS_API bool SaveToSlot(USaveGame* SaveGame, const FString& SlotName, int32 UserIndex);

	// 읽기. 정상 / 예전 평문이면 세이브 객체, 아니면 nullptr
	TERMINUS_API USaveGame* LoadFromSlot(const FString& SlotName, int32 UserIndex, ELoadResult* OutResult = nullptr);

	// 슬롯 파일을 다른 슬롯으로 그대로 복사 (조작된 파일 보관 / 백업)
	TERMINUS_API bool CopySlot(const FString& FromSlot, const FString& ToSlot, int32 UserIndex);
}
