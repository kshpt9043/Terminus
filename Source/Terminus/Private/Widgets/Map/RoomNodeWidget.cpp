// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/RoomNodeWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"

void URoomNodeWidget::SetupRoomNode(const FRoomNode& InRoomData, const TMap<ERoomType, UTexture2D*>& IconMap)
{
	RoomId = InRoomData.RoomId;

	// 1. 방 타입에 맞는 아이콘 설정
	if (UTexture2D* const* FoundTexture = IconMap.Find(InRoomData.Type))
	{
		if (*FoundTexture && RoomIconImage)
		{
			RoomIconImage->SetBrushFromTexture(*FoundTexture);
		}
	}

	// 참고: 버튼 자체의 브러시를 바꾸고 싶다면 아래처럼 변경 가능합니다.
	// FButtonStyle Style = RoomButton->GetStyle();
	// Style.Normal.SetResourceObject(FoundTexture);
	// RoomButton->SetStyle(Style);
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
