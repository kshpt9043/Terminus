#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonAreaSet.generated.h"

class UPaperSpriteComponent;
class UCameraComponent;
class ADungeonArea;

/**
 * 구역 세트. 던전 구역 하나의 무대(배경 + 바닥 + 소품) 템플릿.
 *
 * 테마마다 이 클래스를 상속한 BP 를 만들고 BP 뷰포트에서 스프라이트를 배치한다
 *  - Background : 맨 뒤에 세워진 배경 이미지. 스프라이트만 지정
 *                 기본으로 구역 카메라 화면을 꽉 채우게 크기/위치가 자동으로 맞춰짐. BP 에선 깊이(Y)만 정하면 됨
 *  - Floor      : 캐릭터가 서는 바닥. 눕혀서 깔리는 이미지 (이미지 위쪽 = 화면 안쪽, 아래쪽 = 카메라 쪽)
 *                 기본으로 카메라에 보이는 땅 전체(화면 아래 끝 ~ 배경)를 덮게 자동으로 깔림. 스프라이트만 지정
 *  - 소품       : BP 에서 Paper Sprite 컴포넌트를 추가해서 바닥 위 깊이(Y)를 다르게 심음 -> 2.5D 느낌
 *                 소품 스프라이트 피벗을 Bottom Center 로 두고 Z = 0 에 놓으면 바닥에 딱 붙음
 *
 * 좌표는 구역 기준. 구역 카메라는 +Y 쪽에서 -Y 를 봄
 *  - Y 가 작을수록 뒤(멀리), 클수록 앞(가까이)
 *  - 캐릭터가 서는 줄은 Y = 0, 발 높이는 Z = 0 (캐릭터 스프라이트 피벗이 발밑이라 슬롯 = 발 위치)
 *  - 소품을 Y > 0 에 두면 캐릭터 앞을 가리는 전경이 됨
 *
 * [에디터 가이드] 꾸밀 때 기준을 잡을 수 있게 에디터에서만 보이는 표시가 자동으로 생긴다 (게임에선 안 생김)
 *  - 파란 박스 P1~P4 / 빨간 박스 M1~M3 : 플레이어 / 몬스터가 서는 자리와 크기
 *  - 초록 사각형 SCREEN     : 캐릭터 줄 깊이에서 화면에 보이는 범위
 *  - 노란 사각형 BACKGROUND : 배경 깊이에서 화면에 보이는 범위. 배경 이미지가 이걸 다 덮어야 함
 *  - 주황 사각형 FLOOR      : 카메라에 보이는 바닥 범위 (발 높이)
 *  - PreviewCamera          : 실제 구역 카메라 위치 (레벨에서 선택하면 미리보기 화면이 뜸)
 * 기준값은 BP 에디터에선 PreviewAreaClass 의 기본값, 레벨에서 구역 안에 떠 있을 땐 그 구역 것을 씀
 *
 * 순수 연출용이라 복제하지 않음. 각 머신이 구역의 CurrentSetClass 를 보고 각자 띄운다
 */
UCLASS(Abstract, Blueprintable)
class TERMINUS_API ADungeonAreaSet : public AActor
{
	GENERATED_BODY()

public:
	ADungeonAreaSet();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area Set")
	TObjectPtr<USceneComponent> Root;

	// ---- 배경

	// 맨 뒤 배경. 스프라이트와 깊이(Y)는 BP 에서
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area Set")
	TObjectPtr<UPaperSpriteComponent> Background;

	// 배경을 구역 카메라 화면에 꽉 차게 자동으로 맞춤 (카메라 축 위로 옮기고, 화면을 덮는 크기로)
	// 깊이(Y)는 BP 에서 정한 값을 그대로 씀. 위치/크기를 직접 맞추고 싶으면 끌 것
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Background")
	bool bFitBackgroundToCamera = true;

	// 자동 맞춤 때 화면보다 얼마나 크게 (1 이면 딱 맞음. 가장자리 틈이 안 보이게 약간 크게)
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Background", meta = (EditCondition = "bFitBackgroundToCamera", ClampMin = "1.0"))
	float BackgroundOverscan = 1.05f;

	// ---- 바닥

	// 캐릭터가 서는 바닥. 스프라이트는 BP 에서
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area Set")
	TObjectPtr<UPaperSpriteComponent> Floor;

	// 바닥을 카메라에 보이는 땅 전체(화면 아래 끝 ~ 배경과 만나는 곳)에 자동으로 깔기
	// 가로/세로를 따로 늘려서 맞추므로 이미지 비율이 바뀔 수 있음. 직접 깔고 싶으면 끌 것
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Floor")
	bool bFitFloorToCamera = true;

	// 바닥 높이 보정. 0 = 캐릭터 발 높이(슬롯 Z). 그림자 등 때문에 살짝 띄우거나 내릴 때만
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Floor")
	float FloorHeightOffset = 0.f;

	// 자동 맞춤 때 보이는 범위보다 얼마나 넓게
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Floor", meta = (EditCondition = "bFitFloorToCamera", ClampMin = "1.0"))
	float FloorOverscan = 1.05f;

#if WITH_EDITORONLY_DATA
	// ---- [에디터 가이드 설정]

	// 슬롯 / 카메라 기준을 가져올 구역 클래스. 레이아웃을 BP_DungeonArea 에서 바꿨다면 그걸 지정
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Preview")
	TSubclassOf<ADungeonArea> PreviewAreaClass;

	// 캐릭터 박스 크기 (가로, 세로). 기본값은 지금 캐릭터 스프라이트 실측 (64 x 64 픽셀, 1 픽셀 = 1 유닛)
	// 배틀러 스케일을 바꾸면 여기도 같이 맞출 것
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Preview")
	FVector2D PreviewCharacterSize = FVector2D(64.f, 64.f);

	// 캐릭터 박스 높이 보정. 캐릭터 스프라이트 피벗이 발밑(Bottom Center)이라 슬롯 = 발 -> 기본 0
	UPROPERTY(EditDefaultsOnly, Category = "Area Set|Preview")
	float PreviewCharacterOffsetZ = 0.f;

	// 실제 구역 카메라 자리. 위치는 구역 값으로 매번 덮어써지니 여기서 옮겨도 소용없음
	UPROPERTY(VisibleAnywhere, Category = "Area Set|Preview")
	TObjectPtr<UCameraComponent> PreviewCamera;

	// 가이드로 만든 컴포넌트들. 다시 만들 때 지움
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> PreviewComponents;
#endif

private:
	// 카메라 / 슬롯 기준이 될 구역. 구역 안에 떠 있으면 그 구역, 아니면(BP 에디터) PreviewAreaClass 기본값
	// 세트 원점 = 구역 원점으로 봄 (구역이 SetDisplay 를 원점에 고정함)
	const ADungeonArea* FindReferenceArea() const;

	// 바닥 높이 (세트 기준 Z). 발 높이(0) + 보정
	float GetFloorZ() const { return FloorHeightOffset; }

	// 배경을 카메라 화면에 맞춤 + 캐릭터보다 앞에 있으면 경고
	void FitBackgroundToCamera();

	// 바닥을 카메라에 보이는 땅에 맞춰 깔기. 배경 맞춤 다음에 불러야 함 (배경 깊이까지 깔림)
	void FitFloorToCamera();

	// 에디터 가이드 표시 만들기. 게임 월드(PIE 포함)에선 아무것도 안 함
	void BuildEditorPreview();
};
