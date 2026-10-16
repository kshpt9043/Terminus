#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "Types/SlateEnums.h"
#include "Widgets/TerminusUIColors.h"
#include "ChatWidget.generated.h"

class UBorder;
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
 * 채팅 시안 (Terminus Chat):
 *  - 평소: 바탕 검정 30%, 제목줄 흐림(18%), 오래된 줄일수록 흐림 (한 줄 위로 갈 때마다 -12%, 최소 25%), 글자 그림자
 *  - 제목줄에 마우스: 제목줄 진해짐(85%) + 아래 구분선
 *  - 입력 중: 바탕 검정 70% + 1px 테두리, 모든 줄 또렷, 입력칸 + 글자 수
 *  - 새 메시지: 그 줄만 금빛 바탕이 0.25초 켜졌다 1.6초 동안 사라짐
 *  - 한 줄: [시간] (핑 태그 / 퀵챗 말풍선) 이름: 내용. 내 말 금색, 안내 하늘색, 남의 말 본문색
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Chat) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 크기는 ChatSize 로 정함 (화면에 붙일 때 이 크기로 놓임)
 */
UCLASS()
class TERMINUS_API UChatWidget : public UTerminusUserWidget
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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 채팅창 바탕 (둥근 판). 상태마다 바탕 / 테두리 색을 바꿈
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Background;

	// 잡고 끄는 제목줄. 평소엔 흐리고 마우스를 올리면 / 입력 중엔 진해짐
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> DragHandle;

	// 제목줄 아래 구분선 (마우스를 올렸을 때 / 입력 중에만)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> HeaderLine;

	// 메시지 목록 (한 줄씩 추가됨)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> MessageList;

	// 입력칸 묶음 (입력칸 + 글자 수). 평소엔 숨김. 없으면 InputBox 만 숨겼다 보임
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> InputFrame;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> InputBox;

	// "9/200 · Enter 보내기 · Esc 닫기"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CounterText;

	// 채팅창 크기 (화면 단위)
	UPROPERTY(EditAnywhere, Category = "Chat")
	FVector2D ChatSize = FVector2D(420.f, 260.f);

	// 처음 뜰 때 위치 (화면 왼쪽 위 기준 비율). 한 번 옮기면 그 자리를 기억
	UPROPERTY(EditAnywhere, Category = "Chat")
	FVector2D DefaultPositionRatio = FVector2D(0.015f, 0.4f);

	// 글자 크기 (한 줄 / 시간)
	UPROPERTY(EditAnywhere, Category = "Chat")
	int32 FontSize = 14;

	UPROPERTY(EditAnywhere, Category = "Chat")
	int32 TimeFontSize = 11;

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor MyMessageColor = TerminusUI::Hex(TEXT("FFD978"));

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor OtherMessageColor = TerminusUI::Hex(TEXT("ECE3CF"));

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor SystemMessageColor = TerminusUI::Hex(TEXT("99D9FF"));

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor TimeColor = TerminusUI::Hex(TEXT("8A8478"));

	// 바탕 / 테두리 (평소 / 입력 중)
	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor IdleBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.3f);

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor IdleOutlineColor = FLinearColor(1.f, 1.f, 1.f, 0.03f);

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor ActiveBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.7f);

	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor ActiveOutlineColor = TerminusUI::Hex(TEXT("ECE3CF"), 0.2f);

	// 제목줄 불투명도 (평소 / 마우스 올림 / 입력 중)
	UPROPERTY(EditAnywhere, Category = "Chat")
	FVector HeaderOpacity = FVector(0.18f, 0.85f, 1.f);

	// 새 메시지 금빛 (최대 진하기 / 켜진 채 있는 시간 / 사라지는 시간)
	UPROPERTY(EditAnywhere, Category = "Chat")
	FLinearColor FlashColor = TerminusUI::Hex(TEXT("FFD978"), 0.28f);

	UPROPERTY(EditAnywhere, Category = "Chat")
	float FlashHold = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Chat")
	float FlashFade = 1.6f;

	// 한 번에 보낼 수 있는 글자 수
	UPROPERTY(EditAnywhere, Category = "Chat")
	int32 MaxMessageLength = 200;

private:
	bool bInputOpen = false;

	// 제목줄에 마우스가 올라가 있는지 (마지막으로 적용한 값)
	bool bHeaderHovered = false;

	// 드래그 중인지, 잡은 지점(채팅창 왼쪽 위에서 마우스까지)
	bool bDragging = false;
	FVector2D DragOffset = FVector2D::ZeroVector;

	// 메시지 목록 맨 위 빈칸 (줄이 아래부터 쌓이게)
	UPROPERTY()
	TObjectPtr<class USpacer> TopSpacer;

	// 금빛으로 깜빡이는 줄 / 시작 시각 (실시간 초)
	TWeakObjectPtr<UBorder> FlashLine;
	double FlashStartTime = 0.0;

	FDelegateHandle MessageAddedHandle;
	FDelegateHandle HistoryReplacedHandle;

	// 채팅 기록 전체를 다시 그림 (처음 / 서버가 기록을 보냈을 때)
	void RebuildFromHistory();

	void BuildDefaultLayout();

	// 바탕 / 제목줄 / 줄 투명도를 지금 상태(입력 중 / 마우스)에 맞춤
	void ApplyBackground();
	void ApplyLineStyles();

	UBorder* AddLine(const FChatMessage& Message);
	bool IsMyMessage(const FChatMessage& Message) const;
	void HandleMessageAdded(const FChatMessage& Message);

	void UpdateCounter();

	// 화면 밖으로 안 나가게 맞춰서 옮기고 기억
	void MoveTo(FVector2D Position);
	FVector2D GetViewportSizeInSlateUnits() const;

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleTextChanged(const FText& Text);
};
