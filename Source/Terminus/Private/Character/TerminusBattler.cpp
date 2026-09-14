// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/TerminusBattler.h"

#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "Combat/CombatStatsComponent.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerState.h"
#include "PaperZDAnimInstance.h"

ATerminusBattler::ATerminusBattler()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// 입력 안하고 표현만 하는 폰이니까 자동 빙의 꺼놓기
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Visual);        // ← Root 가 아니라 Visual 에
	
	// PaperCharacter가 가진 속성은 복사, Movement 컴포넌트가 필요 없어서 캐릭터로 안한 것
	Sprite->AlwaysLoadOnServer = true;
	Sprite->AlwaysLoadOnClient = true;
	Sprite->PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Sprite->SetGenerateOverlapEvents(false);
	
	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	
	// 렌더 대상은 스프라이트로 설정
	Animation->InitRenderComponent(Sprite);
	
	// 전투용 컴포넌트
	CombatStats = CreateDefaultSubobject<UCombatStatsComponent>(TEXT("CombatStats"));
}

void ATerminusBattler::DebugDamage(int32 Amount)
{
	CombatStats->ApplyDamage(Amount);
}

void ATerminusBattler::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	SetFacingRight(bFacingRight);
}

void ATerminusBattler::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		InitAsClass(PS->GetCharacterClass());
	}
}

void ATerminusBattler::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	// 클라에서는 PossessedBy가 안와서 복제되는 시점에 전파해야함
	if (const ATerminusPlayerState* PS = GetPlayerState<ATerminusPlayerState>())
	{
		InitAsClass(PS->GetCharacterClass());
	}
}

void ATerminusBattler::BeginPlay()
{
	Super::BeginPlay();
	
	if (bUsePreviewClass)
	{
		InitAsClass(PreviewClass);
	}
}

void ATerminusBattler::InitAsClass(ECharacterClass InClass)
{
	const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(InClass);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Battler: %s 행이 없음. Project Settings > Game > Terminus Data 확인할 것"),
			*UEnum::GetValueAsString(InClass));
		return;
	}

	// 스텟은 서버만 정한다(InitFrom 안에 가드가 있음). 클라는 복제로 받음
	CombatStats->InitFrom(Row->BaseStats);

	// 외형은 복제가 안 되니 각 PC 가 스스로 적용해야 한다
	if (!Row->AnimInstanceClass.IsNull())
	{
		if (UClass* ABPClass = Row->AnimInstanceClass.LoadSynchronous())
		{
			Animation->SetAnimInstanceClass(ABPClass);
		}
	}
}

void ATerminusBattler::SetFacingRight(bool bRight)
{
	bFacingRight = bRight;

	// Sprite 를 뒤집으면 PaperZD 의 ApplyFrameMirroring 이 매 프레임 +1 로 되돌린다
	// Root 를 뒤집으면 레벨 배치 스케일과 충돌한다 -> 그래서 중간의 Visual
	FVector S = Visual->GetRelativeScale3D();
	S.X = FMath::Abs(S.X) * (bRight ? 1.f : -1.f);
	Visual->SetRelativeScale3D(S);
}
