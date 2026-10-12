#pragma once

#include "CoreMinimal.h"

/**
 * 화면(뷰포트) 높이. 숫자가 클수록 위에 그려짐
 * 새 화면을 붙일 땐 여기서 고르고, 순서를 바꾸고 싶으면 여기만 고칠 것
 */
namespace TerminusUI::Layer
{
	constexpr int32 Map          = 0;    // 지도 (맨 아래)
	constexpr int32 CombatHUD    = 10;   // 전투 HUD
	constexpr int32 Comms        = 11;   // 핑 / 퀵챗 (전투 HUD 바로 위)
	constexpr int32 Pick         = 20;   // 시작 스킬 / 유물 고르기
	constexpr int32 RoomPanel    = 22;   // 보상 / 휴식 / 이벤트 / 상점
	constexpr int32 Rescue       = 23;   // 구출 투표 (방 패널 위)
	constexpr int32 Overlay      = 24;   // 관전 바 / 이공간 / 층 투표 (패널 위, 채팅 아래 -> 패널이 떠 있어도 누를 수 있게)
	constexpr int32 Chat         = 25;   // 채팅
	constexpr int32 CommsMenu    = 26;   // 핑 메뉴 (채팅 위)
	constexpr int32 FloorTitle   = 30;   // "n층" 타이틀
	constexpr int32 MenuWindow   = 40;   // 메인 메뉴의 거점 / 창고 / 이어하기 창
	constexpr int32 Settlement   = 45;   // 정산
	constexpr int32 Popup        = 50;   // 확인 팝업 (어떤 화면보다도 위)
}
