#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RestWidget.generated.h"

class APlayerState;
class UButton;
class UImage;
class UPanelWidget;
class UTextBlock;
class UTexture2D;
class URestWidget;

// 선택지 버튼마다 무엇을 골랐는지 넘기는 중계 (UButton 의 OnClicked 는 인자가 없음)
UENUM()
enum class ERestOption : uint8
{
	Rest,       // 휴식 (멀티면 다음에 회복 대상 고르기)
	Explore,    // 탐색
	Target,     // 회복 대상 (Target)
	Back        // 대상 고르기에서 처음으로
};

UCLASS()
class TERMINUS_API URestClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<URestWidget> Owner;
	ERestOption Option = ERestOption::Rest;
	TWeakObjectPtr<APlayerState> Target;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 휴식터 화면. 이벤트 선택지처럼 왼쪽 그림 + 오른쪽 제목 / 설명 / 선택지 (사용자 결정 2026-10-08, 슬더스 이벤트 화면 참고)
 *  - 휴식 / 탐색 중 하나만 고름 (기획 10-07)
 *  - 휴식: 싱글은 바로 나를 회복. 멀티는 회복시킬 사람(나 또는 같은 휴식터 동료)을 한 번 더 고름. 동료를 고르면 나는 회복 안 함
 *  - 탐색: 소량의 던전 재화 + 낮은 확률로 유물
 *  - 고르면 서버가 결과를 알려 줌. 같은 휴식터 전원이 고르면 지도로 (화면은 구역이 닫힐 때 PC 가 닫음)
 * 띄우는 건 PC 의 Client_ShowRest
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 RestClass 에 지정 (아래 이름 전부 선택 사항)
 * 선택지 버튼은 OptionBox 안에 C++ 이 채움. 그림은 ArtImage (또는 기본 모양에서 ArtTexture)
 */
UCLASS()
class TERMINUS_API URestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const TArray<APlayerState*>& InOccupants, float InHealRatio, FIntPoint InExploreCurrency, float InExploreRelicChance);

	// 서버가 알려 준 결과 ("체력을 14 회복했습니다.")
	void ShowResult(const FText& Result);

	void HandleOption(ERestOption Option, APlayerState* Target);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 왼쪽 그림
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ArtImage;

	// "휴식터"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 설명 / 결과 / 기다리는 중 안내
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	// 선택지 버튼이 들어갈 곳 (세로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> OptionBox;

	// 기본 모양에서 왼쪽에 넣을 그림. 비워 두면 빈 칸
	UPROPERTY(EditAnywhere, Category = "Rest")
	TObjectPtr<UTexture2D> ArtTexture;

	UPROPERTY(EditAnywhere, Category = "Rest")
	FVector2D ArtSize = FVector2D(440.f, 440.f);

	UPROPERTY(EditAnywhere, Category = "Rest")
	FVector2D OptionSize = FVector2D(520.f, 74.f);

private:
	enum class EStage : uint8 { Choose, PickTarget, Waiting, Done };

	TArray<TWeakObjectPtr<APlayerState>> Occupants;
	EStage Stage = EStage::Choose;
	float HealRatio = 0.2f;
	FIntPoint ExploreCurrency = FIntPoint(5, 10);
	float ExploreRelicChance = 0.05f;
	float RefreshTimer = 0.f;
	FText ResultText;

	// 대상 고르기에서 마지막으로 그린 체력 (바뀌었을 때만 다시 그림 -> 누르는 도중에 버튼이 바뀌지 않게)
	TArray<int32> ShownHealth;
	TArray<int32> ReadHealth() const;

	UPROPERTY()
	TArray<TObjectPtr<URestClickRelay>> Relays;

	void BuildDefaultLayout();
	void Rebuild();
	void AddOption(ERestOption Option, APlayerState* Target, const FString& Title, const FString& Description, bool bEnabled = true);
};
