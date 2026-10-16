#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/TerminusUIColors.h"
#include "ChoiceCardWidget.generated.h"

class UButton;
class USizeBox;
class UTextBlock;
class UItemSlotWidget;

// 카드 상태. 기본 모양은 버튼 색으로, WBP 는 OnCardUpdated 에서 State 를 보고 꾸밈
UENUM(BlueprintType)
enum class EChoiceCardState : uint8
{
	Normal,
	Selected,    // 내가 고른 것 / 지금 보는 것
	Highlight,   // 결과로 뽑힌 것
	Danger       // 배신 같은 위험한 선택
};

// C++ 기본 모양에만 쓰는 꾸밈 (WBP 면 무시)
struct FChoiceCardStyle
{
	FVector2D Size = FVector2D::ZeroVector;   // 0 이면 내용에 맞춤
	bool bFixedHeight = true;                 // false 면 Size.Y 는 최소 높이
	int32 TitleSize = 20;
	FLinearColor TitleColor = TerminusUI::Color::GoldBase;
	int32 DescSize = 14;
	ETextJustify::Type Justify = ETextJustify::Center;
	FMargin Padding = FMargin(10.f, 8.f);
	FLinearColor NormalColor = TerminusUI::Color::Card;
};

/**
 * 선택지 카드 / 버튼 하나. 제목 + 설명 + 아래 한 줄 (+ 스킬 / 유물 칸)
 * 휴식터 선택지, 이벤트 카드, 층 투표 카드, 관전 바 버튼이 같이 씀
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 *  - ChoiceButton : 누르는 버튼 (없으면 못 누름)
 *  - TitleText / DescText / FooterText : 비어 있는 줄은 숨김
 *  - ItemSlot : 스킬 / 유물 후보일 때 그 칸 (아니면 숨김)
 * 그 WBP 는 각 화면의 OptionClass / CardClass / ButtonClass 에 지정
 * ChoiceId(어떤 선택지인지)와 State 는 블루프린트에서 읽을 수 있고, 바뀔 때마다 OnCardUpdated 가 불림 (아이콘 / 색 꾸미기용)
 */
UCLASS()
class TERMINUS_API UChoiceCardWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	void SetTexts(const FString& Title, const FString& Description, const FString& Footer = FString());
	void SetItem(bool bRelic, FName Row);   // None 이면 칸 숨김
	void SetChoiceId(FName InId);
	void SetState(EChoiceCardState InState);
	void SetChoiceEnabled(bool bEnabled);

	// 기본 모양일 때만 적용
	void ApplyDefaultStyle(const FChoiceCardStyle& InStyle);

	FSimpleDelegate OnChosen;

	UFUNCTION(BlueprintPure, Category = "Choice Card") FName GetChoiceId() const { return ChoiceId; }
	UFUNCTION(BlueprintPure, Category = "Choice Card") EChoiceCardState GetState() const { return State; }

protected:
	virtual void NativeOnInitialized() override;

	// 내용 / 상태가 바뀐 뒤 불림 (WBP 에서 아이콘 / 색을 바꾸고 싶을 때)
	UFUNCTION(BlueprintImplementableEvent, Category = "Choice Card")
	void OnCardUpdated();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ChoiceButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> CardSizeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FooterText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UItemSlotWidget> ItemSlot;

	UPROPERTY(BlueprintReadOnly, Category = "Choice Card")
	FName ChoiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Choice Card")
	EChoiceCardState State = EChoiceCardState::Normal;

private:
	bool bDefaultLayout = false;
	FChoiceCardStyle Style;

	void BuildDefaultLayout();
	void ApplyStateColor();
	static void SetLine(UTextBlock* Text, const FString& Value);

	UFUNCTION()
	void HandleClicked();
};
