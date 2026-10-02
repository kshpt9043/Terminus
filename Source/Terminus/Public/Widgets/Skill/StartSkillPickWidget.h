#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartSkillPickWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class USkillCardWidget;

/**
 * 런 시작 강화 스킬 고르기. 던전에 들어와 지도를 보기 전에 화면 전체를 덮고 뜸
 * (보유 스킬이 3개 이상이면 3장, 1~2개면 그만큼)
 * 보유 스킬이 없으면 안내 문구만 띄우고 아무 키(또는 클릭)나 누르면 지도로
 *
 * 카드를 누르면 서버에 장착 요청을 보내고 스스로 닫힘 -> 아래 지도가 보임
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 카드 모양은 CardClass 에 USkillCardWidget 을 부모로 한 WBP 를 지정
 */
UCLASS()
class TERMINUS_API UStartSkillPickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 보여줄 카드 (DT_Skill 행 이름들). 비어 있으면 "보유 스킬 없음" 안내
	void SetChoices(const TArray<FName>& SkillRows);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// "강화 스킬을 하나 골라 장착하세요" / "보유한 스킬이 없습니다"
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;

	// 보유 스킬이 없을 때만 보이는 안내 ("아무 키나 누르면 지도로 이동합니다")
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MessageText;

	// 카드가 들어갈 칸 (가로 상자 추천). 있던 자식은 지우고 카드를 채움
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CardBox;

	// 카드 위젯 클래스. 비워 두면 C++ 기본 카드
	UPROPERTY(EditAnywhere, Category = "Start Skill")
	TSubclassOf<USkillCardWidget> CardClass;

	// 카드 사이 간격 (CardBox 가 가로 상자일 때)
	UPROPERTY(EditAnywhere, Category = "Start Skill")
	float CardSpacing = 24.f;

	// 보유 스킬 없음 화면에서 입력을 받기 시작하기까지(초). 직전 클릭/키가 바로 넘겨버리지 않게
	UPROPERTY(EditAnywhere, Category = "Start Skill")
	float AnyKeyDelay = 0.4f;

private:
	// 한 번 고르면 끝 (연타로 두 번 보내지 않게)
	bool bChosen = false;

	// 보유 스킬 없음 화면인지
	bool bNoSkills = false;

	// 화면이 뜬 시각 (AnyKeyDelay 계산용)
	double ShownTime = 0.0;

	void BuildDefaultLayout();
	void HandleCardClicked(FName SkillRow);

	// 보유 스킬 없음 화면에서 아무 입력이나 -> 건너뛰고 지도로. 처리했으면 true
	bool TrySkip();

	// 서버에 결과 보내고 닫기 (NAME_None = 건너뜀)
	void Finish(FName SkillRow);
};
