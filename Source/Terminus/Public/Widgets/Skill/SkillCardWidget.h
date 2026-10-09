#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillCardWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnSkillCardClicked, FName /*SkillRow*/);

/**
 * 스킬 카드 한 장. 스킬 고르기 화면(시작 강화 스킬, 나중에 보상)에서 씀
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 두면 됨 (전부 선택 사항)
 * 하나도 없으면(C++ 클래스 그대로) C++ 이 기본 모양을 만든다
 */
UCLASS()
class TERMINUS_API USkillCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// DT_Skill 행 이름으로 카드 내용 채우기
	void SetSkill(FName InSkillRow);

	FName GetSkillRow() const { return SkillRow; }

	FOnSkillCardClicked OnCardClicked;

protected:
	virtual void NativeOnInitialized() override;

	// 카드 전체를 덮는 버튼 (누르면 이 카드 선택)
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CardButton;

	// 스킬 이름
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;

	// "공격 · 적 1명" 처럼 종류와 대상
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TypeText;

	// "스킬 에너지 1"
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CostText;

	// 설명 (DT 의 Description_KR)
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DescText;

	// 기본 모양의 카드 크기
	UPROPERTY(EditAnywhere, Category = "Skill Card")
	FVector2D DefaultCardSize = FVector2D(260.f, 340.f);

private:
	FName SkillRow;

	void BuildDefaultLayout();

	UFUNCTION()
	void HandleCardClicked();
};
