// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/RoomNodeWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

void URoomNodeWidget::SetupRoomNode(const FRoomNode& InRoomData, const TMap<ERoomType, UTexture2D*>& IconMap)
{
	RoomId = InRoomData.RoomId;
	Row = InRoomData.Row;
	bIsBoss = InRoomData.Type == ERoomType::BOSS;

	// 1. 방 타입에 맞는 아이콘 설정
	if (UTexture2D* const* FoundTexture = IconMap.Find(InRoomData.Type))
	{
		if (*FoundTexture)
		{
			// 버튼 브러시 자체를 아이콘으로 (호버 / 눌림은 WBP 에서 정한 틴트만 다름)
			FButtonStyle Style = RoomButton->GetStyle();
			Style.Normal.SetResourceObject(*FoundTexture);
			Style.Hovered.SetResourceObject(*FoundTexture);
			Style.Pressed.SetResourceObject(*FoundTexture);
			Style.Disabled.SetResourceObject(*FoundTexture);   // 혹시 비활성이 돼도 아이콘이 바뀌지 않게
			RoomButton->SetStyle(Style);
		}
	}

	// 퀘스트 특수 방: 금색 + 조금 크게 + 툴팁 (안 그러면 일반 가디언 방과 구분이 안 됨)
	const bool bSpecial = !InRoomData.SpecialRoom.IsNone();
	RoomButton->SetColorAndOpacity(bSpecial ? TerminusUI::Color::Gold : FLinearColor::White);
	RoomButton->SetToolTipText(bSpecial ? FText::FromString(TEXT("퀘스트 특수 방")) : FText::GetEmpty());

	// 2. 보스는 아이콘 / 동그라미를 크게 (아이콘 상자 가운데 기준이라 선 위치는 그대로)
	const float Size = bIsBoss ? BossIconSize : (bSpecial ? IconSize * 1.25f : IconSize);
	if (IconBox)
	{
		IconBox->SetWidthOverride(Size);
		IconBox->SetHeightOverride(Size);
	}
	if (VisitedMark) VisitedMark->SetDesiredSizeOverride(FVector2D(Size, Size));
	// 동그라미 그림은 상자의 81% 가 고리 -> 아이콘보다 사방 12px 크게 (시안: inset -12)
	if (CurrentRing) CurrentRing->SetDesiredSizeOverride(FVector2D(Size * 1.7f, Size * 1.7f));
}

void URoomNodeWidget::SetRoomSelectable(bool bSelectable)
{
	if (!RoomButton) return;

	// 이미 같은 상태면 건드리지 않음
	if (AppliedSelectable.IsSet() && AppliedSelectable.GetValue() == bSelectable) return;
	AppliedSelectable = bSelectable;

	// 마우스만 통과시켜서 호버 효과 / 클릭이 안 생기게 함
	// (SetIsEnabled(false) 를 쓰면 Slate 가 비활성 효과로 회색 처리해 버림)
	RoomButton->SetIsEnabled(true);
	RoomButton->SetVisibility(bSelectable ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
}

void URoomNodeWidget::SetVisualState(ERoomNodeState NewState)
{
	if (AppliedState.IsSet() && AppliedState.GetValue() == NewState) return;
	AppliedState = NewState;

	auto Show = [](UWidget* Widget, bool bShow)
	{
		if (Widget) Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	};

	Show(GlowImage, NewState == ERoomNodeState::Available);
	Show(VisitedMark, NewState == ERoomNodeState::Visited);
	Show(CurrentRing, NewState == ERoomNodeState::Current);

	if (RoomButton)
	{
		RoomButton->SetRenderOpacity(NewState == ERoomNodeState::Unreachable ? UnreachableOpacity : IconOpacity);
	}
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
		// 동료 칩: 2px 파티 판 테두리 + 초상화 자리
		// TODO: Steam API 로 가져온 아바타 텍스처를 Chip 에 (Chip->SetBrushFromTexture)
		UBorder* Frame = NewObject<UBorder>(this);
		UImage* Chip = NewObject<UImage>(this);
		if (!Frame || !Chip) continue;

		Frame->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::MapColor::PartyPlate, TerminusUI::MapColor::PartyPlate, 0.f, 0.f));
		Frame->SetPadding(FMargin(2.f));
		Chip->SetColorAndOpacity(TerminusUI::MapColor::ChipFill);
		Chip->SetDesiredSizeOverride(FVector2D(ChipSize - 4.f, ChipSize - 4.f));
		Frame->SetContent(Chip);

		if (UHorizontalBoxSlot* ChipSlot = ProfileContainer->AddChildToHorizontalBox(Frame))
		{
			ChipSlot->SetPadding(FMargin(i == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f));
			ChipSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 내가 선택한 방이면 붉은 잉크 동그라미
	SelectionBorder->SetVisibility(bIsSelectedByMe ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void URoomNodeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 이벤트 바인딩
	if (RoomButton)
	{
		RoomButton->OnClicked.AddUniqueDynamic(this, &URoomNodeWidget::OnButtonClicked);
	}
}

void URoomNodeWidget::OnButtonClicked()
{
	// 버튼 클릭 시 델리게이트 브로드캐스트
	OnRoomNodeClicked.Broadcast(RoomId);
}
