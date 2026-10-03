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
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessageAdded, const FChatMessage&);

/**
 * 채팅 기록과 Enter 키. 로컬(각 컴퓨터)에만 있음
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

	FOnChatMessageAdded OnMessageAdded;

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

	TWeakObjectPtr<UChatWidget> ActiveWidget;

	TSharedPtr<class FChatInputProcessor> Processor;

	bool IsOwnWindowActive() const;
};
