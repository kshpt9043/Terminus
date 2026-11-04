// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Map/RoomNodeWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Data/EventQuestTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerState.h"
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

	// 퀘스트 특수 방: 금색 + 조금 크게 (안 그러면 일반 가디언 방과 구분이 안 됨)
	const bool bSpecial = !InRoomData.SpecialRoom.IsNone();
	RoomButton->SetColorAndOpacity(bSpecial ? TerminusUI::Color::Gold : FLinearColor::White);

	// 마우스를 올리면 어떤 방인지 (11-04 피드백). 퀘스트 특수 방은 진행 중인 의뢰 설명까지
	FString Tip = DescribeRoomType(InRoomData.Type);
	if (bSpecial)
	{
		Tip = TEXT("퀘스트 특수 방\n진행 중인 의뢰의 목표 몬스터가 나옵니다.");
		const ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;
		const FName QuestRow = LocalPS ? LocalPS->GetRunState().ActiveQuest.Quest : NAME_None;
		if (const FQuestRow* Quest = QuestRow.IsNone() ? nullptr : UTerminusDataSettings::FindQuestRow(QuestRow))
		{
			Tip += FString::Printf(TEXT("\n\n[%s]\n%s"), *Quest->QuestName.ToString(), *Quest->QuestDesc.ToString());
		}
	}
	TerminusUI::SetGameTooltip(RoomButton, WidgetTree, Tip);

	// 2. 보스는 아이콘 / 동그라미를 크게 (아이콘 상자 가운데 기준이라 선 위치는 그대로)
	const float Size = bIsBoss ? BossIconSize : (bSpecial ? IconSize * 1.25f : IconSize);
	if (IconBox)
	{
		IconBox->SetWidthOverride(Size);
		IconBox->SetHeightOverride(Size);
	}
	// SetDesiredSizeOverride 는 아직 안 그려진 위젯엔 안 남아서(5.8) 브러시 크기로
	auto SetImageSize = [](UImage* Image, float Edge)
	{
		if (!Image) return;
		FSlateBrush Brush = Image->GetBrush();
		Brush.ImageSize = FVector2D(Edge, Edge);
		Image->SetBrush(Brush);
	};
	SetImageSize(VisitedMark, Size);
	// 동그라미 그림은 상자의 81% 가 고리 -> 아이콘보다 사방 12px 크게 (시안: inset -12)
	SetImageSize(CurrentRing, Size * 1.7f);
}

FString URoomNodeWidget::DescribeRoomType(ERoomType Type)
{
	switch (Type)
	{
	case ERoomType::MONSTER:  return TEXT("몬스터 방\n일반 몬스터와 싸웁니다. 이기면 던전 재화와 픽업 스킬 보상.");
	case ERoomType::GUARDIAN: return TEXT("가디언 방\n강한 가디언 몬스터와 싸웁니다. 이기면 유물 보상.");
	case ERoomType::BOSS:     return TEXT("보스 방\n이 층의 보스와 싸웁니다. 이기면 다음 층으로.");
	case ERoomType::BREAK:    return TEXT("휴식 방\n체력을 회복하거나 주변을 탐색합니다.");
	case ERoomType::STORE:    return TEXT("상점\n던전 재화로 유물과 스킬을 사고, 픽업 스킬을 강화합니다.");
	case ERoomType::EVENT:    return TEXT("이벤트 방\n무작위 사건. 고르는 선택지에 따라 보상이나 대가가 있습니다.");
	case ERoomType::QUEST:    return TEXT("의뢰 방\n세력의 의뢰(퀘스트)를 하나 고릅니다.");
	default:                  return FString();
	}
}

void URoomNodeWidget::SetRoomSelectable(bool bSelectable)
{
	if (!RoomButton) return;

	// 이미 같은 상태면 건드리지 않음
	if (AppliedSelectable.IsSet() && AppliedSelectable.GetValue() == bSelectable) return;
	AppliedSelectable = bSelectable;

	// 못 고르는 방도 마우스는 받아야 방 설명 툴팁이 뜸 (11-04 피드백) -> 버튼은 늘 보이게 두고
	// 호버 / 눌림 모양을 평소 모양으로 바꿔 효과를 없애고, 클릭은 OnButtonClicked 에서 막음
	// (SetIsEnabled(false) 를 쓰면 Slate 가 비활성 효과로 회색 처리해 버림)
	if (!bStyleSaved)
	{
		SelectableStyle = RoomButton->GetStyle();
		bStyleSaved = true;
	}
	FButtonStyle Style = SelectableStyle;
	if (!bSelectable)
	{
		Style.Hovered = Style.Normal;
		Style.Pressed = Style.Normal;
	}
	RoomButton->SetStyle(Style);
	RoomButton->SetIsEnabled(true);
	RoomButton->SetVisibility(ESlateVisibility::Visible);
}

void URoomNodeWidget::SetVisualState(ERoomNodeState NewState)
{
	if (AppliedState.IsSet() && AppliedState.GetValue() == NewState) return;
	AppliedState = NewState;

	auto Show = [](UWidget* Widget, bool bShow)
	{
		if (Widget) Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	};

	// 지나간 방도 X 대신 동그라미 (11-04 피드백). 지금 방은 진하게, 지나간 방은 조금 흐리게
	Show(GlowImage, NewState == ERoomNodeState::Available);
	Show(VisitedMark, false);
	Show(CurrentRing, NewState == ERoomNodeState::Current || NewState == ERoomNodeState::Visited);
	if (CurrentRing) CurrentRing->SetRenderOpacity(NewState == ERoomNodeState::Visited ? 0.6f : 1.f);

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
	if (!AppliedSelectable.Get(false)) return;   // 못 고르는 방 (툴팁 때문에 마우스만 받는 중)

	// 버튼 클릭 시 델리게이트 브로드캐스트
	OnRoomNodeClicked.Broadcast(RoomId);
}
