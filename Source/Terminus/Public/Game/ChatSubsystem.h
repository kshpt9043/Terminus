#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChatSubsystem.generated.h"

class UChatWidget;

UENUM(BlueprintType)
enum class EChatMessageKind : uint8
{
	Player,   // 플레이어가 보낸 말
	System    // 입장 / 퇴장 같은 안내
};

// 채팅 한 줄. 서버가 모두에게 보내는 단위
USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Sender;

	UPROPERTY(BlueprintReadOnly)
	FString Text;

	UPROPERTY(BlueprintReadOnly)
	EChatMessageKind Kind = EChatMessageKind::Player;

	// 보낸 시간 "HH:MM" (서버 시계). 화면에 [HH:MM] 으로
	UPROPERTY(BlueprintReadOnly)
	FString Time;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessageAdded, const FChatMessage&);

/**
 * 채팅 기록과 Enter 키. 화면 기록은 각 컴퓨터에만 있음 (같은 방에 다시 들어가도 되살리지 않음: 사용자 결정 10-10)
 * 보존은 서버(방장) PC 의 메모장 로그: Saved/ChatLogs/Chat_<날짜시각>_<방 이름>.txt (멀티일 때만, 증거용)
 *
 * 게임 인스턴스는 레벨 이동(주점 -> 던전)에도 살아 있어서 대화 기록과 채팅창 위치를 여기 둠
 * -> 레벨이 바뀌어 채팅창이 새로 만들어져도 이전 대화가 그대로 보임
 *
 * Enter: 채팅창이 떠 있고 입력창이 닫혀 있으면 입력창을 엶 (입력 전처리기라 포커스가 어디 있든 동작)
 * 다른 입력칸에 글을 쓰는 중이거나 팝업이 떠 있으면 가로채지 않음 (팝업의 Enter = 확인 이 그대로 동작)
 */
UCLASS()
class TERMINUS_API UChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UChatSubsystem* Get(const UObject* WorldContext);

	// 받은 메시지 추가 (PC 의 Client_ReceiveChat 이 부름)
	void AddMessage(const FChatMessage& Message);

	const TArray<FChatMessage>& GetHistory() const { return History; }

	// 서버를 거치지 않고 이 컴퓨터에서 바로 넣는 줄 (핑 / 퀵챗 / 재촉: 각자 받은 알림으로 만듦)
	// 시간을 찍고, 방장 PC 면 메모장 로그에도 남김
	void AddLocalMessage(const UWorld* World, const FChatMessage& Message);

	// 새 방(세션)을 열거나 들어갈 때 이전 대화 지움
	void ClearHistory();

	// [서버] 새 로그 파일 시작 (방을 열 때). 방 이름이 파일 이름에 들어감
	void BeginServerLog(const FString& RoomName);

	// [서버] 한 줄을 로그 파일에 덧붙임. 멀티(리슨 / 데디 서버)일 때만. 파일이 없으면 지금 방 이름으로 시작
	void WriteServerLog(const UWorld* World, const FChatMessage& Message);

	FOnChatMessageAdded OnMessageAdded;

	DECLARE_MULTICAST_DELEGATE(FOnChatHistoryReplaced);
	FOnChatHistoryReplaced OnHistoryReplaced;

	// 지금 화면에 떠 있는 채팅창 (채팅창이 스스로 등록 / 해제)
	void SetActiveWidget(UChatWidget* InWidget);
	void ClearActiveWidget(UChatWidget* InWidget);

	// 채팅창 위치 (화면 좌표, DPI 보정된 값). 레벨이 바뀌어도 같은 자리에 뜨게
	TOptional<FVector2D> SavedPosition;

	// Enter 처리. 입력창을 열었으면 true
	bool HandleEnter();

	// 기록 최대 줄 수
	static constexpr int32 MaxHistory = 100;

private:
	TArray<FChatMessage> History;

	// 지금 쓰는 로그 파일 (서버만)
	FString ServerLogPath;

	TWeakObjectPtr<UChatWidget> ActiveWidget;

	TSharedPtr<class FChatInputProcessor> Processor;

	bool IsOwnWindowActive() const;
};
