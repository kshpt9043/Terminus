#include "Dungeon/DungeonAreaSet.h"

#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dungeon/DungeonArea.h"

namespace
{
	// 카메라에서 Dist 만큼 떨어진 평면에서 화면에 보이는 가로/세로 절반 크기
	FVector2D GetVisibleHalfSize(const UCameraComponent* Cam, float Dist)
	{
		const float HalfWidth = (Cam->ProjectionMode == ECameraProjectionMode::Orthographic)
			? Cam->OrthoWidth * 0.5f
			: Dist * FMath::Tan(FMath::DegreesToRadians(Cam->FieldOfView * 0.5f));

		const float Aspect = Cam->AspectRatio > KINDA_SMALL_NUMBER ? Cam->AspectRatio : 16.f / 9.f;
		return FVector2D(HalfWidth, HalfWidth / Aspect);
	}

	// 바닥 평면에서 카메라에 보이는 영역 (세트 기준 좌표)
	struct FFloorArea
	{
		FVector Center;
		FQuat Rotation;     // 눕힌 스프라이트 회전: 스프라이트 X = 카메라 오른쪽, 스프라이트 위쪽 = 화면 안쪽, 면 = 위
		float HalfWidth;
		float HalfDepth;
	};

	// FarPoint: 바닥이 끝나는 깊이(배경 위치). 원근 카메라만 지원
	bool ComputeVisibleFloor(const UCameraComponent* Cam, float FloorZ, const FVector& FarPoint, float Overscan, FFloorArea& Out)
	{
		if (Cam->ProjectionMode != ECameraProjectionMode::Perspective) return false;

		const FTransform CamT = Cam->GetRelativeTransform();
		const FVector CamLoc = CamT.GetLocation();
		const FQuat CamRot = CamT.GetRotation();

		const float Height = CamLoc.Z - FloorZ;
		if (Height <= 1.f) return false;   // 카메라가 바닥보다 낮으면 바닥이 안 보임

		// 수평 방향 기준 (카메라가 살짝 숙여져 있어도 바닥은 수평)
		FVector Forward = CamRot.GetForwardVector();
		Forward.Z = 0.f;
		if (!Forward.Normalize()) return false;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

		// 화면 아래 끝 가운데로 나가는 시선이 바닥에 닿는 곳 = 바닥이 보이기 시작하는 곳
		const float Aspect = Cam->AspectRatio > KINDA_SMALL_NUMBER ? Cam->AspectRatio : 16.f / 9.f;
		const float TanHalfV = FMath::Tan(FMath::DegreesToRadians(Cam->FieldOfView * 0.5f)) / Aspect;
		const FVector BottomRay = CamRot.RotateVector(FVector(1.f, 0.f, -TanHalfV));

		float NearDist = 1.f;
		if (BottomRay.Z < -KINDA_SMALL_NUMBER)
		{
			const float T = -Height / BottomRay.Z;
			NearDist = FMath::Max(1.f, FVector::DotProduct(BottomRay * T, Forward));
		}

		const float FarDist = FVector::DotProduct(FarPoint - CamLoc, Forward);
		NearDist = FMath::Max(1.f, NearDist / Overscan);   // 화면 아래 끝 틈 방지로 약간 더 가까이
		if (FarDist <= NearDist) return false;

		// 가장 넓게 보이는 곳은 먼 쪽 끝
		const float HalfWidthFar = GetVisibleHalfSize(Cam, FarDist).X * Overscan;

		const float MidDist = (NearDist + FarDist) * 0.5f;
		Out.Center = FVector(CamLoc.X, CamLoc.Y, FloorZ) + Forward * MidDist;
		Out.Rotation = FRotationMatrix::MakeFromXZ(Right, Forward).ToQuat();   // 면(Y) = Forward x Right = 위
		Out.HalfWidth = HalfWidthFar;
		Out.HalfDepth = (FarDist - NearDist) * 0.5f;
		return true;
	}
}

ADungeonAreaSet::ADungeonAreaSet()
{
	PrimaryActorTick.bCanEverTick = false;

	// 연출 전용. 각 머신이 로컬로 띄우므로 복제하지 않음
	// (복제 클래스면 ChildActorComponent 가 클라에서 안 만들고 서버 복제를 기다림)
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Background = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Background"));
	Background->SetupAttachment(Root);

	// 캐릭터 줄(Y = 0)보다 뒤. 크기/높이는 자동 맞춤이 카메라 화면에 맞게 정함
	Background->SetRelativeLocation(FVector(0.f, -450.f, 0.f));

	Floor = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Floor"));
	Floor->SetupAttachment(Root);

	// 눕혀 둠 (자동 맞춤을 꺼도 바닥 모양이 되게). 이미지 위쪽 = 화면 안쪽(-Y), 면 = 위
	Floor->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f)).Rotator());

#if WITH_EDITORONLY_DATA
	PreviewAreaClass = ADungeonArea::StaticClass();

	// 에디터 전용 카메라. 레벨에서 세트를 선택하면 이 카메라로 미리보기 미니창이 뜸
	// (엔진은 활성화된 카메라만 미리보기에 씀 -> 끄면 안 됨)
	// 게임 시점과는 무관: 시점 대상은 구역 액터라 구역 자신의 AreaCamera 만 쓰임
	PreviewCamera = CreateEditorOnlyDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	if (PreviewCamera)
	{
		PreviewCamera->SetupAttachment(Root);
	}
#endif
}

void ADungeonAreaSet::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 배경 -> 바닥 순서 (바닥은 배경 깊이까지 깔림). 가이드는 맞춘 결과 기준으로 그림
	FitBackgroundToCamera();
	FitFloorToCamera();
	BuildEditorPreview();

	// 무대 소품은 부딪힐 일이 없음. BP 에서 추가한 스프라이트까지 한 번에 충돌을 끔
	// (디자이너가 소품마다 충돌 설정을 챙기지 않아도 되게)
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetCanEverAffectNavigation(false);
	}
}

const ADungeonArea* ADungeonAreaSet::FindReferenceArea() const
{
	if (const ADungeonArea* ParentArea = Cast<ADungeonArea>(GetParentActor()))
	{
		return ParentArea;
	}

#if WITH_EDITORONLY_DATA
	UClass* AreaClass = PreviewAreaClass ? PreviewAreaClass.Get() : ADungeonArea::StaticClass();
	return AreaClass->GetDefaultObject<ADungeonArea>();
#else
	return GetDefault<ADungeonArea>();
#endif
}

void ADungeonAreaSet::FitBackgroundToCamera()
{
	const ADungeonArea* Area = FindReferenceArea();
	const UCameraComponent* Cam = Area ? Area->GetAreaCamera() : nullptr;
	if (!Background || !Cam) return;

	// 세트 원점 = 구역 원점이라 구역 카메라의 상대 좌표를 그대로 씀
	const FTransform CamT = Cam->GetRelativeTransform();
	const FVector CamLoc = CamT.GetLocation();
	const FQuat CamRot = CamT.GetRotation();
	const FVector Forward = CamRot.GetForwardVector();

	const float BackgroundDist = FVector::DotProduct(Background->GetRelativeLocation() - CamLoc, Forward);
	const float CharacterDist = FVector::DotProduct(FVector::ZeroVector - CamLoc, Forward);

	// 배경이 캐릭터 줄보다 카메라 쪽에 있으면 캐릭터가 통째로 가려짐 (불투명 재질)
	if (BackgroundDist <= CharacterDist)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaSet] %s: 배경이 캐릭터보다 앞에 있어서 캐릭터를 가림. 배경 Y 를 캐릭터 줄(0)보다 작게 할 것 (지금 카메라에서 배경 %.0f, 캐릭터 %.0f)"),
			*GetClass()->GetName(), BackgroundDist, CharacterDist);
	}

	if (!bFitBackgroundToCamera || BackgroundDist <= 1.f) return;

	UPaperSprite* Sprite = Background->GetSprite();
	if (!Sprite) return;

	// 스프라이트 로컬: X 가 가로, Z 가 세로 (Paper2D 기본 축)
	const FBoxSphereBounds SpriteBounds = Sprite->GetRenderBounds();
	if (SpriteBounds.BoxExtent.X <= KINDA_SMALL_NUMBER || SpriteBounds.BoxExtent.Z <= KINDA_SMALL_NUMBER) return;

	// 화면을 가로 세로 모두 덮는 균일 배율 (비율이 화면과 다르면 넘치는 쪽은 잘림)
	const FVector2D Half = GetVisibleHalfSize(Cam, BackgroundDist);
	const float Scale = FMath::Max(Half.X / SpriteBounds.BoxExtent.X, Half.Y / SpriteBounds.BoxExtent.Z) * BackgroundOverscan;

	// 스프라이트 면이 카메라를 보게. 기본 카메라(Yaw -90)면 회전 0
	const FQuat SpriteRot = CamRot * FQuat(FRotator(0.f, 90.f, 0.f));

	// 피벗이 이미지 가운데가 아니어도 이미지 중심이 카메라 축에 오도록
	const FVector AxisPoint = CamLoc + Forward * BackgroundDist;
	const FVector Location = AxisPoint - SpriteRot.RotateVector(SpriteBounds.Origin * Scale);

	Background->SetRelativeLocationAndRotation(Location, SpriteRot);
	Background->SetRelativeScale3D(FVector(Scale));
}

void ADungeonAreaSet::FitFloorToCamera()
{
	if (!bFitFloorToCamera || !Floor || !Background) return;

	UPaperSprite* Sprite = Floor->GetSprite();
	if (!Sprite) return;

	const ADungeonArea* Area = FindReferenceArea();
	const UCameraComponent* Cam = Area ? Area->GetAreaCamera() : nullptr;
	if (!Cam) return;

	FFloorArea FloorArea;
	if (!ComputeVisibleFloor(Cam, GetFloorZ(), Background->GetRelativeLocation(), FloorOverscan, FloorArea))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaSet] %s: 바닥 자동 맞춤 불가 (원근 카메라가 바닥보다 높고, 배경이 캐릭터 뒤에 있어야 함)"),
			*GetClass()->GetName());
		return;
	}

	const FBoxSphereBounds SpriteBounds = Sprite->GetRenderBounds();
	if (SpriteBounds.BoxExtent.X <= KINDA_SMALL_NUMBER || SpriteBounds.BoxExtent.Z <= KINDA_SMALL_NUMBER) return;

	// 가로 = 보이는 폭, 세로(이미지 위아래) = 보이는 깊이. 따로 늘림
	const FVector Scale(
		FloorArea.HalfWidth / SpriteBounds.BoxExtent.X,
		1.f,
		FloorArea.HalfDepth / SpriteBounds.BoxExtent.Z);

	// 피벗이 이미지 가운데가 아니어도 이미지 중심이 영역 중심에 오도록
	const FVector Location = FloorArea.Center - FloorArea.Rotation.RotateVector(SpriteBounds.Origin * Scale);

	Floor->SetRelativeLocationAndRotation(Location, FloorArea.Rotation);
	Floor->SetRelativeScale3D(Scale);
}

void ADungeonAreaSet::BuildEditorPreview()
{
#if WITH_EDITORONLY_DATA
	// 이전 가이드 지우기
	for (USceneComponent* Old : PreviewComponents)
	{
		if (IsValid(Old)) Old->DestroyComponent();
	}
	PreviewComponents.Reset();

	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld()) return;    // 게임(PIE 포함)에서는 안 만듦

	const ADungeonArea* Area = FindReferenceArea();
	const UCameraComponent* AreaCam = Area ? Area->GetAreaCamera() : nullptr;
	if (!AreaCam) return;

	// 세트 원점 = 구역 원점
	const FTransform CamInSet = AreaCam->GetRelativeTransform();
	const FVector CamLoc = CamInSet.GetLocation();
	const FQuat CamRot = CamInSet.GetRotation();
	const FVector Forward = CamRot.GetForwardVector();
	const FVector Up = CamRot.GetUpVector();

	// 글자는 카메라 쪽을 보게
	const FRotator FacingCamera = FRotationMatrix::MakeFromXZ(-Forward, Up).Rotator();

	if (PreviewCamera)
	{
		PreviewCamera->SetRelativeTransform(CamInSet);
		PreviewCamera->SetProjectionMode(AreaCam->ProjectionMode);
		PreviewCamera->SetFieldOfView(AreaCam->FieldOfView);
		PreviewCamera->SetOrthoWidth(AreaCam->OrthoWidth);
		PreviewCamera->SetAspectRatio(AreaCam->AspectRatio);
	}

	// ------------------------------------------------------------------
	// 가이드 컴포넌트 생성 도우미
	// UCS 생성으로 표시 -> 구성 스크립트가 다시 돌 때 엔진이 정리. Transient -> 저장 안 됨
	// ------------------------------------------------------------------
	auto Register = [this](USceneComponent* Comp)
	{
		Comp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Comp->SetIsVisualizationComponent(true);   // 에디터 전용 + 액터 크기(bounds) 계산에서 빠짐
		Comp->SetHiddenInGame(true);
		Comp->SetupAttachment(Root);
		Comp->RegisterComponent();
		PreviewComponents.Add(Comp);
	};

	// Extent 는 Rotation 기준 축으로
	auto AddBox = [&](const FVector& Center, const FQuat& Rotation, const FVector& Extent, const FColor& Color, float Thickness)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		Box->ShapeColor = Color;
		Box->SetLineThickness(Thickness);
		Box->SetBoxExtent(Extent, false);
		Box->SetRelativeLocationAndRotation(Center, Rotation);
		Register(Box);
	};

	auto AddLabel = [&](const FVector& Location, const FString& Text, const FColor& Color, float Size)
	{
		// 기본 폰트가 한글을 못 그려서 영문 표기
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
		Label->SetText(FText::FromString(Text));
		Label->SetTextRenderColor(Color);
		Label->SetWorldSize(Size);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextBottom);
		Label->SetRelativeLocationAndRotation(Location, FacingCamera);
		Register(Label);
	};

	// ------------------------------------------------------------------
	// 화면 범위 사각형 (카메라를 마주보는 면)
	// ------------------------------------------------------------------
	auto AddScreenFrame = [&](const FVector& PointOnPlane, const FColor& Color, const FString& Text)
	{
		const float Dist = FVector::DotProduct(PointOnPlane - CamLoc, Forward);
		if (Dist <= 1.f) return;   // 카메라 뒤

		const FVector2D Half = GetVisibleHalfSize(AreaCam, Dist);
		const FVector Center = CamLoc + Forward * Dist;
		AddBox(Center, CamRot, FVector(1.f, Half.X, Half.Y), Color, 3.f);   // 카메라 기준: X 앞, Y 오른쪽, Z 위
		AddLabel(Center + Up * (Half.Y + 5.f), Text, Color, FMath::Max(20.f, Half.X * 0.05f));
	};

	// 캐릭터 줄(Y = 0)에서 보이는 범위
	AddScreenFrame(FVector::ZeroVector, FColor(60, 220, 90), TEXT("SCREEN (character line)"));

	// 배경 깊이에서 보이는 범위. 배경 이미지가 이걸 다 덮어야 함
	if (Background)
	{
		AddScreenFrame(Background->GetRelativeLocation(), FColor(255, 210, 40), TEXT("BACKGROUND must cover"));
	}

	// ------------------------------------------------------------------
	// 바닥 범위 (눕힌 사각형). 스프라이트를 넣기 전에도 어디에 깔리는지 보이게
	// ------------------------------------------------------------------
	if (Background)
	{
		FFloorArea FloorArea;
		if (ComputeVisibleFloor(AreaCam, GetFloorZ(), Background->GetRelativeLocation(), 1.f, FloorArea))
		{
			const FColor FloorColor(255, 140, 30);
			AddBox(FloorArea.Center, FloorArea.Rotation, FVector(FloorArea.HalfWidth, 1.f, FloorArea.HalfDepth), FloorColor, 3.f);

			// 먼 쪽 끝에 이름
			const FVector FarEdge = FloorArea.Center + FloorArea.Rotation.GetAxisZ() * FloorArea.HalfDepth;
			AddLabel(FarEdge + FVector(0.f, 0.f, 5.f), TEXT("FLOOR (feet level)"), FloorColor, 40.f);
		}
	}

	// ------------------------------------------------------------------
	// 캐릭터 자리. 박스 아래 끝 = 발 (캐릭터 스프라이트 피벗이 발밑)
	// ------------------------------------------------------------------
	const FVector CharacterExtent(2.f, PreviewCharacterSize.X * 0.5f, PreviewCharacterSize.Y * 0.5f);

	auto AddCharacter = [&](const FVector& Slot, const FColor& Color, const FString& Text)
	{
		const FVector Center = Slot + Up * (CharacterExtent.Z + PreviewCharacterOffsetZ);
		AddBox(Center, CamRot, CharacterExtent, Color, 2.f);
		AddLabel(Center + Up * (CharacterExtent.Z + 5.f), Text, Color, 24.f);
	};

	const TArray<FVector>& PlayerSlots = Area->GetPlayerSlots();
	for (int32 i = 0; i < PlayerSlots.Num(); ++i)
	{
		AddCharacter(PlayerSlots[i], FColor(70, 150, 255), FString::Printf(TEXT("P%d"), i + 1));
	}

	const TArray<FVector>& MonsterSlots = Area->GetMonsterSlots();
	for (int32 i = 0; i < MonsterSlots.Num(); ++i)
	{
		AddCharacter(MonsterSlots[i], FColor(255, 80, 80), FString::Printf(TEXT("M%d"), i + 1));
	}
#endif
}
