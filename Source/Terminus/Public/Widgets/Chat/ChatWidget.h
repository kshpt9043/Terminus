#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "ChatWidget.generated.h"

class UEditableTextBox;
class UScrollBox;
class UTextBlock;
struct FChatMessage;

/**
 * 멀티 채팅창. 주점 / 지도 / 전투 어디서나 떠 있음 (PC 가 만듦, 싱글이면 안 만듦)
 *
 * - Enter: 입력창 열기 (UChatSubsystem 이 Enter 를 받아 OpenInput 을 부름)
 * - 입력 중 Enter: 전송하고 입력창 닫기. 빈 칸이면 그냥 닫기
 * - 입력 중 ESC: 입력창 닫기 (UEscapeStackSubsystem)
 * - 제목줄(DragHandle)을 끌어서 위치 옮기기. 위치는 레벨이 바뀌어도 유지
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Chat) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 크기는 ChatSize 로 정함 (화면에 붙일 때 이 크기로 놓임)
 */
UCLASS()
class TERMINUS_API UChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenInput();
	void CloseInput();
	bool IsInputOpen() const { return bInputOpen; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 채팅창 바탕. 입력 중일 때 더 진하게
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UBorder> Background;

	// 잡고 끄는 제목줄
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> DragHandle;

	// 메시지 목록 (한 줄씩 TextBlock 이 추가됨)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> MessageList;

	// 입력창 (평소엔 숨김)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> InputBox;

	// 채팅창 크기 (화면 단위)
	UPROPERTY(EditAnywhere, Category = "Chat")
	FVector2D ChatSize = FVector2D(420.f, 260.f);

	// 처음 뜰 때 위치 (화면 왼쪽 위 기준 비율). 한 번 옮기면 그 자리를 기억
	UPROPERTY(EditAnywhere, Category = "Chat")
	FVector2D DefaultPositionRatio = FVector2D(0.015f, 0.4f);

	// 글자 크기 / 색
	UPROPERTY(EditAnywhere, Category = "Chat")
	int32 FontSize = 13;

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor MyMessageColor = FLinearColor(1.f, 0.85f, 0.45f);

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor OtherMessageColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor SystemMessageColor = FLinearColor(0.6f, 0.85f, 1.f);

	// 바탕 색 (평소 / 입력 중)
	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor IdleBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.3f);

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor ActiveBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.7f);

	// 한 번에 보낼 수 있는 글자 수
	UPROPERTY(EditAnywhere, Category = "Chat")
	int32 MaxMessageLength = 200;

private:
	bool bInputOpen = false;

	// 드래그 중인지, 잡은 지점(채팅창 왼쪽 위에서 마우스까지)
	bool bDragging = false;
	FVector2D DragOffset = FVector2D::ZeroVector;

	FDelegateHandle MessageAddedHandle;

	void BuildDefaultLayout();
	void ApplyBackground();

	void AddLine(const FChatMessage& Message);
	bool IsMyMessage(const FChatMessage& Message) const;
	void HandleMessageAdded(const FChatMessage& Message);

	// 화면 밖으로 안 나가게 맞춰서 옮기고 기억
	void MoveTo(FVector2D Position);
	FVector2D GetViewportSizeInSlateUnits() const;

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
