#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

class UTextBlock;

// 로딩 화면 종류
UENUM(BlueprintType)
enum class ELoadingScreenStyle : uint8
{
	Full,   // 레벨 이동 같은 큰 로딩: 불투명하게 덮고 문구 + 로딩 표시
	Light   // 검색 / 잠깐 대기: 블러 + 어둡게 반투명 + 로딩 아이콘만
};

/**
 * 전체 로딩 화면 모양. 띄우고 끄는 건 ULoadingScreenSubsystem
 *  - 화면 전체를 덮고 아래를 못 누르게 함
 *  - 레벨을 불러오는 동안(게임이 멈춤)에는 무비 플레이어가 같은 위젯을 따로 그림 -> 애니메이션도 돌아감
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Loading) 를 만들고
 * Project Settings > Game > Terminus Loading Screen 의 FullWidgetClass 에 지정할 것
 * 아래 이름의 위젯을 두면 C++ 이 채움 (전부 선택 사항). 문구가 바뀔 때 BP 에서 OnMessageChanged 로도 받을 수 있음
 * 무비 플레이어 쪽은 다른 스레드에서 그려지므로 WBP 안에서 게임 로직(타이머 / 월드 접근)은 쓰지 말 것 (애니메이션은 괜찮음)
 */
UCLASS()
class TERMINUS_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// "던전으로 이동하는 중..." 같은 안내 (가벼운 로딩은 문구를 안 띄움)
	void SetMessage(const FText& InMessage);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// WBP 에 루트가 없을 때 C++ 기본 모양
	virtual void BuildDefaultLayout();

	// 문구가 바뀔 때 (WBP 에서 직접 꾸밀 때)
	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void OnMessageChanged(const FText& NewMessage);

	// "불러오는 중"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 안내 문구
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

private:
	FText PendingMessage;
};

/**
 * 가벼운 로딩 (탐색 / 잠깐 대기). 화면을 살짝 블러 + 어둡게 반투명하게 덮고 로딩 아이콘만
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_LoadingLight) 를 만들어 LightWidgetClass 에 지정할 것
 */
UCLASS()
class TERMINUS_API ULoadingSpinnerWidget : public ULoadingScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildDefaultLayout() override;

	// 기본 모양의 블러 세기 / 어둡기
	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	float BlurStrength = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	float DimOpacity = 0.45f;
};
