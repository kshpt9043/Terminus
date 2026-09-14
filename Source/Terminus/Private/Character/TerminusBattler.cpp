// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/TerminusBattler.h"

#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"

ATerminusBattler::ATerminusBattler()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// 입력 안하고 표현만 하는 폰이니까 자동 빙의 꺼놓기
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);
	
	// PaperCharacter가 가진 속성은 복사, Movement 컴포넌트가 필요 없어서 캐릭터로 안한 것
	Sprite->AlwaysLoadOnServer = true;
	Sprite->AlwaysLoadOnClient = true;
	Sprite->PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Sprite->SetGenerateOverlapEvents(false);
	
	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	
	// 렌더 대상은 스프라이트로 설정
	Animation->InitRenderComponent(Sprite);
}

void ATerminusBattler::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	SetFacingRight(bFacingRight);
}

void ATerminusBattler::SetFacingRight(bool bRight)
{
	bFacingRight = bRight;
	
	FVector S = Sprite->GetRelativeScale3D();
	S.X = FMath::Abs(S.X) * (bRight ? 1.f : -1.f);
	Sprite->SetRelativeScale3D(S);
}
