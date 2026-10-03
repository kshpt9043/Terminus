#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EscapeStackSubsystem.generated.h"

class UWidget;

/**
 * ESC 로 닫기. 열린 창(팝업, 주점 리스트 등)이 열릴 때 Push, 닫힐 때 Remove 하면
 * ESC 를 눌렀을 때 맨 나중에 열린(맨 위) 창 하나의 OnEscape 를 부른다
 *
 * 키 입력을 Slate 앞단(입력 전처리기)에서 받아서 포커스가 어디 있든 동작함
 * (버튼을 누른 직후처럼 키보드 포커스가 창 밖에 있어도)
 * 닫을 창이 없으면 ESC 를 먹지 않고 그대로 흘려보냄
 */
UCLASS()
class TERMINUS_API UEscapeStackSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UEscapeStackSubsystem* Get(const UObject* WorldContext);

	// 창이 열릴 때. 같은 창을 다시 넣으면 맨 위로 옮김
	void Push(UWidget* Owner, FSimpleDelegate OnEscape);

	// 창이 닫힐 때
	void Remove(UWidget* Owner);

	// ESC 처리. 닫은 창이 있으면 true
	bool HandleEscape();

	// 지금 화면에 열려 있는 창이 하나라도 있는가 (채팅 Enter 가 팝업 Enter 를 가로채지 않게)
	bool HasOpenEntry() const;

private:
	struct FEntry
	{
		TWeakObjectPtr<UWidget> Owner;
		FSimpleDelegate OnEscape;
	};
	TArray<FEntry> Stack;

	TSharedPtr<class FEscapeInputProcessor> Processor;

	// 이 게임 인스턴스의 창이 지금 활성 창인가 (PIE 창이 여러 개일 때 남의 창 ESC 는 무시)
	bool IsOwnWindowActive() const;
};
