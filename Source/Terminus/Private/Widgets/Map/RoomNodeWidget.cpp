// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/RoomNodeWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"

void URoomNodeWidget::SetupRoomNode(const FRoomNode& InRoomData, const TMap<ERoomType, UTexture2D*>& IconMap)
{
	RoomId = InRoomData.RoomId;

	// 1. 방 타입에 맞는 아이콘 설정
	if (UTexture2D* const* FoundTexture = IconMap.Find(InRoomData.Type))
	{
		if (*FoundTexture)
		{
			//RoomIconImage->SetBrushFromTexture(*FoundTexture);
			
			// 참고: 버튼 자체의 브러시를 바꾸고 싶다면 아래처럼 변경 가능합니다.
			FButtonStyle Style = RoomButton->GetStyle();
			Style.Normal.SetResourceObject(*FoundTexture);
			Style.Hovered.SetResourceObject(*FoundTexture);
			Style.Pressed.SetResourceObject(*FoundTexture);
			RoomButton->SetStyle(Style);
		}
	}	
}

void URoomNodeWidget::SetRoomSelectable(bool bSelectable)
{
	if (!RoomButton) return;

	// 이미 같은 상태면 건드리지 않음
	if (AppliedSelectable.IsSet() && AppliedSelectable.GetValue() == bSelectable) return;
	AppliedSelectable = bSelectable;

	RoomButton->SetIsEnabled(bSelectable);
	// 이동 불가능한 방은 투명도(Opacity)를 낮춰 어둡게 노출
	SetRenderOpacity(bSelectable ? 1.0f : 0.3f);
}

void URoomNodeWidget::RefreshSelectionState(const TArray<ATerminusPlayerState*>& AllPlayerStates,
	ATerminusPlayerState* LocalPS)
{
	if (!ProfileContainer || !SelectionBorder) return;

	// 이 방을 고른 사람 목록부터 계산
	TArray<TWeakObjectPtr<ATerminusPlayerState>> Selectors;
	bool bIsSelectedByMe = false;

	for (ATerminusPlayerState* PS : AllPlayerStates)
	{
		if (PS && PS->GetSelectedRoomId() == RoomId)
		{
			Selectors.Add(PS);

			// 내가 고른 방인가 체크
			if (PS == LocalPS)
			{
				bIsSelectedByMe = true;
			}
		}
	}

	// 이 방의 선택 상태가 그대로면 다시 그리지 않음 -> 바뀐 방만 갱신됨
	if (AppliedSelectedByMe.IsSet() && AppliedSelectedByMe.GetValue() == bIsSelectedByMe && AppliedSelectors == Selectors)
	{
		return;
	}
	AppliedSelectedByMe = bIsSelectedByMe;
	AppliedSelectors = Selectors;

	ProfileContainer->ClearChildren();

	for (int32 i = 0; i < Selectors.Num(); ++i)
	{
		// 선택한 플레이어의 스팀 아바타 초상화 추가
		UImage* ProfileImg = NewObject<UImage>(this);
		if (ProfileImg)
		{
			// TODO: Steam API를 통해 가져온 Avatar Texture 적용 (ProfileImg->SetBrushFromTexture)
			ProfileImg->SetDesiredSizeOverride(FVector2D(28.0f, 28.0f));
			ProfileContainer->AddChildToHorizontalBox(ProfileImg);
		}
	}

	// 내가 선택한 방이면 빨간 테두리 활성화
	SelectionBorder->SetVisibility(bIsSelectedByMe ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void URoomNodeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 이벤트 바인딩
	if (RoomButton)
	{
		RoomButton->OnClicked.AddDynamic(this, &URoomNodeWidget::OnButtonClicked);
	}
}

void URoomNodeWidget::OnButtonClicked()
{
	// 버튼 클릭 시 델리게이트 브로드캐스트
	OnRoomNodeClicked.Broadcast(RoomId);
}
