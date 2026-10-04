#include "GatewayStage.h"
#include "Gateway.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGatewayStage::AGatewayStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dome"));
	SetRootComponent(Dome);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded()) { Dome->SetStaticMesh(SphereMesh.Object); }
	Dome->SetWorldScale3D(FVector(400.f)); // esfera de 100 cm -> 400 m de diametro
	Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dome->SetCastShadow(false);
	Dome->bCastDynamicShadow = false;
	Dome->SetGenerateOverlapEvents(false);
	Dome->bAffectDistanceFieldLighting = false;
	Dome->bVisibleInRayTracing = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DomeMat(TEXT("/Game/Gateway/Materials/M_Dome.M_Dome"));
	if (DomeMat.Succeeded()) { Dome->SetMaterial(0, DomeMat.Object); }

	Particles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Particles"));
	Particles->SetupAttachment(Dome);
	// El domo esta escalado x400: las particulas se colocan en espacio del actor compensando esa escala
	Particles->SetRelativeScale3D(FVector(1.f / 400.f));
	if (SphereMesh.Succeeded()) { Particles->SetStaticMesh(SphereMesh.Object); }
	Particles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Particles->SetCastShadow(false);
	Particles->NumCustomDataFloats = 0;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StarMat(TEXT("/Game/Gateway/Materials/M_Star.M_Star"));
	if (StarMat.Succeeded()) { Particles->SetMaterial(0, StarMat.Object); }

	Prop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Prop"));
	Prop->SetupAttachment(Dome);
	Prop->SetRelativeScale3D(FVector(1.f / 400.f));
	Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prop->SetCastShadow(false);
	Prop->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(TEXT("/Game/Gateway/Models/SM_Caja.SM_Caja"));
	if (BoxMesh.Succeeded()) { Prop->SetStaticMesh(BoxMesh.Object); }
	PropLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PropLight"));
	PropLight->SetupAttachment(Prop);
	PropLight->SetRelativeLocation(FVector(0.f, -60.f, 110.f));
	PropLight->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
	PropLight->SetIntensity(0.f);
	PropLight->SetAttenuationRadius(600.f);
	PropLight->SetCastShadows(false);
	PropLight->SetMobility(EComponentMobility::Movable);
}

void AGatewayStage::InitParticles(int32 Count)
{
	FRandomStream R(1979);
	Parts.SetNum(Count);
	PartXf.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FParticle& P = Parts[i];
		P.Seed = R.FRand();
		P.Radius = R.FRandRange(0.6f, 1.6f);
		P.Angle = R.FRandRange(0.f, 2.f * PI);
		P.Height = R.FRandRange(-1.f, 1.f);
		P.Depth = R.FRandRange(0.3f, 12.f);
		P.Phase = R.FRandRange(0.f, 2.f * PI);
		P.Scale = R.FRandRange(0.6f, 1.4f);
		P.Pos = FVector(R.FRandRange(-8.f, 8.f), R.FRandRange(-8.f, 8.f), R.FRandRange(-4.f, 4.f)) * 100.f;
		PartXf[i] = FTransform(FQuat::Identity, P.Pos, FVector(2.f));
	}
	Particles->ClearInstances();
	Particles->AddInstances(PartXf, false, false);
}

void AGatewayStage::UpdateParticles(float Dt)
{
	if (Parts.Num() == 0 || !Particles) return;
	const int32 Mode = int32(CurB.Mode + 0.5f);
	// Presencia por modo: orbe y tunel fuertes, cosmos/mandala medias, ganzfeld casi nada, vacio pocas
	float Target = 0.f;
	switch (Mode) { case 1: Target = 1.f; break; case 2: Target = 1.f; break; case 3: Target = 0.6f; break; case 4: Target = 0.7f; break; case 5: Target = 0.25f; break; case 6: Target = 0.5f; break; default: Target = 0.08f; }
	ParticleAlpha = FMath::FInterpTo(ParticleAlpha, Target, Dt, 0.4f);

	const FVector F = ViewForward.GetSafeNormal();
	const FVector Rt = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(F, Rt);
	const float T = SceneTime;
	const float Spin = (0.25f + 0.55f * SpeedNow);
	const int32 N = Parts.Num();
	for (int32 i = 0; i < N; ++i)
	{
		FParticle& P = Parts[i];
		FVector TargetPos; float TargetScale = P.Scale;
		bool bTeleport = false;
		if (Mode == 1) // REBAL: fuente desde la coronilla, espiral alrededor, entra por los pies
		{
			if (!bPaused) { P.Angle += Dt * Spin * (1.2f + 0.4f * P.Seed); P.Height -= Dt * 0.18f * (0.7f + 0.6f * P.Seed); }
			if (P.Height < -1.f) { P.Height += 2.f; bTeleport = true; }
			const float Rad = (0.75f + 0.45f * BreathNow) * P.Radius * (0.55f + 0.45f * FMath::Sqrt(1.f - P.Height * P.Height));
			TargetPos = FVector(FMath::Cos(P.Angle) * Rad, FMath::Sin(P.Angle) * Rad, P.Height * 1.1f) * 100.f;
			TargetScale = P.Scale * (0.9f + 0.5f * BreathNow);
		}
		else if (Mode == 2) // tunel: corriente hacia el usuario a lo largo de la mirada
		{
			if (!bPaused) { P.Depth -= Dt * (1.5f + 4.f * SpeedNow) * (0.6f + 0.8f * P.Seed); P.Angle += Dt * 0.4f * Spin; }
			if (P.Depth < 0.4f) { P.Depth += 12.f; bTeleport = true; }
			const float Rho = 0.5f + 1.2f * P.Radius;
			TargetPos = (F * P.Depth + Rt * (FMath::Cos(P.Angle + P.Phase) * Rho) + Up * (FMath::Sin(P.Angle + P.Phase) * Rho)) * 100.f;
			TargetScale = P.Scale * (1.2f + 0.3f * ComplexityNow);
		}
		else if (Mode == 6) // caja: nube quieta y calida alrededor
		{
			const float A = P.Phase + T * 0.05f;
			TargetPos = FVector(FMath::Cos(A) * 2.f * P.Radius, FMath::Sin(A) * 2.f * P.Radius, P.Height * 1.5f) * 100.f;
		}
		else // estrellas/chispas en 3D con deriva lenta
		{
			const float A = P.Phase + T * 0.02f * (0.5f + P.Seed) * (0.5f + SpeedNow);
			const float Rad = 2.f + 10.f * P.Seed;
			TargetPos = FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, (P.Height + 0.1f * FMath::Sin(T * 0.3f + P.Phase)) * 5.f) * 100.f;
			TargetScale = P.Scale * (0.5f + 0.9f * P.Seed) * (0.8f + 0.3f * FMath::Sin(T * (1.f + P.Seed * 3.f) + P.Phase));
		}
		if (bTeleport) { P.Pos = TargetPos; }
		else { P.Pos = FMath::VInterpTo(P.Pos, TargetPos, Dt, 1.6f); }
		const float S = TargetScale * ParticleAlpha * 1.6f; // esfera de 100 cm: escala 0.016 -> 1.6 cm; se reescala por el 1/400 del componente
		PartXf[i] = FTransform(FQuat::Identity, P.Pos, FVector(FMath::Max(0.001f, S * 0.007f)));
	}
	Particles->BatchUpdateInstancesTransforms(0, PartXf, false, true, false);

	// Caja de conversion de energia: aparece delante (modo 6), gira despacio y su luz late con la respiracion
	PropAlpha = FMath::FInterpTo(PropAlpha, (Mode == 6) ? 1.f : 0.f, Dt, 0.6f);
	if (Prop && Prop->GetStaticMesh())
	{
		const bool bShow = PropAlpha > 0.01f;
		if (Prop->IsVisible() != bShow) { Prop->SetVisibility(bShow); }
		if (bShow)
		{
			const FVector Pos = (F * 1.9f + FVector(0.f, 0.f, -0.55f + 0.4f * (1.f - PropAlpha))) * 100.f;
			const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X)) + 90.f + 8.f * FMath::Sin(T * 0.15f), 180.f); // el FBX de Blender llega con Z invertida
			Prop->SetRelativeLocationAndRotation(Pos, Rot);
			Prop->SetRelativeScale3D(FVector(PropAlpha / 400.f));
			PropLight->SetIntensity(PropAlpha * (1500.f + 900.f * BreathNow));
		}
		else { PropLight->SetIntensity(0.f); }
	}
	if (ParticleMID)
	{
		ParticleMID->SetVectorParameterValue(TEXT("Color"), ColorNow * (0.6f + 1.2f * ParticleAlpha) + FLinearColor(0.15f, 0.15f, 0.2f) * ParticleAlpha);
	}
}

void AGatewayStage::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Base = Dome->GetMaterial(0))
	{
		DomeMID = UMaterialInstanceDynamic::Create(Base, this);
		Dome->SetMaterial(0, DomeMID);
	}
	else
	{
		UE_LOG(LogGateway, Warning, TEXT("M_Dome no existe: corre Tools/setup_assets.py en el editor"));
	}

	if (UMaterialInterface* StarBase = Particles->GetMaterial(0))
	{
		ParticleMID = UMaterialInstanceDynamic::Create(StarBase, this);
		Particles->SetMaterial(0, ParticleMID);
	}
	InitParticles(1200);

	// Post-proceso global
	FActorSpawnParameters P; P.Owner = this;
	PostVolume = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, P);
	if (PostVolume)
	{
		PostVolume->bUnbound = true;
		PostVolume->Priority = 10.f;
		FPostProcessSettings& S = PostVolume->Settings;
		// Exposicion fija (r.DefaultFeature.AutoExposure=False en DefaultEngine.ini): el brillo lo decide la sesion
		S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = 1.f;
		S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 1.f;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = 1.6f;
		S.bOverride_BloomThreshold = true; S.BloomThreshold = 0.6f;
		S.bOverride_BloomSizeScale = true; S.BloomSizeScale = 6.f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.f; // la vineta la hace el shader
		S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f;
		S.bOverride_DynamicGlobalIlluminationMethod = true; S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		S.bOverride_ReflectionMethod = true; S.ReflectionMethod = EReflectionMethod::None;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.f;
		if (UMaterialInterface* PostBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Gateway/Materials/M_Post.M_Post")))
		{
			PostMID = UMaterialInstanceDynamic::Create(PostBase, this);
			S.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, PostMID));
		}
		else
		{
			UE_LOG(LogGateway, Warning, TEXT("M_Post no existe: corre Tools/setup_assets.py en el editor"));
		}
	}

	FadeNow = 1.f; FadeTarget = 0.f; FadeSpeed = 1.f / 3.f;
	ApplyParams();
}

UTexture2D* AGatewayStage::LoadImage(const FString& Name)
{
	if (Name.IsEmpty()) return nullptr;
	const FString Path = Name.StartsWith(TEXT("/")) ? Name : FString::Printf(TEXT("/Game/Gateway/Images/%s.%s"), *Name, *Name);
	UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Path);
	if (!T) { UE_LOG(LogGateway, Warning, TEXT("Imagen no encontrada: %s"), *Path); }
	return T;
}

void AGatewayStage::SetVisual(const FGatewayVisual& Target, float RampSeconds)
{
	TargetVisual = Target;
	VisualRamp = FMath::Max(0.5f, RampSeconds);
	HueDriftPerMin = Target.HueDrift;
	const float NewMode = float(uint8(Target.Mode));
	UTexture2D* NewImage = LoadImage(Target.Image);
	if (!FMath::IsNearlyEqual(NewMode, CurB.Mode) || NewImage != CurB.Image)
	{
		// consolidar el crossfade en curso y empezar otro
		if (Mix > 0.f) { CurA = CurB; }
		CurB.Mode = NewMode; CurB.Image = NewImage;
		Mix = 0.f;
		MixRate = 1.f / VisualRamp;
	}
}

void AGatewayStage::SetFlicker(const FGatewayFlicker& Target, float RampSeconds)
{
	FlickerTarget = Target;
	FlickerTarget.Hz = FMath::Clamp(Target.Hz, 0.f, 15.f);  // techo de seguridad
	FlickerRamp = FMath::Max(0.5f, RampSeconds);
}

void AGatewayStage::SetBreath(float Phase01, bool bActive)
{
	BreathPhase = Phase01; bBreathActive = bActive;
}

void AGatewayStage::SetFade(float Target, float Seconds)
{
	FadeTarget = FMath::Clamp(Target, 0.f, 1.f);
	FadeSpeed = 1.f / FMath::Max(0.05f, Seconds);
}

void AGatewayStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	if (!bPaused) { SceneTime += Dt; }

	// Crossfade de modo
	if (Mix < 1.f && MixRate > 0.f)
	{
		Mix = FMath::Min(1.f, Mix + Dt * MixRate);
		if (Mix >= 1.f) { CurA = CurB; Mix = 0.f; MixRate = 0.f; }
	}
	// Parametros continuos hacia el objetivo
	const float K = 1.f - FMath::Exp(-Dt / (VisualRamp * 0.35f));
	ColorNow = FMath::Lerp(ColorNow, TargetVisual.Color, K);
	IntensityNow = FMath::Lerp(IntensityNow, TargetVisual.Intensity, K);
	SpeedNow = FMath::Lerp(SpeedNow, TargetVisual.Speed, K);
	ComplexityNow = FMath::Lerp(ComplexityNow, TargetVisual.Complexity, K);
	if (!bPaused) { HueNow = FMath::Fmod(HueNow + HueDriftPerMin * Dt / 60.f, 1.f); }

	const float KF = 1.f - FMath::Exp(-Dt / (FlickerRamp * 0.35f));
	FlickerNow.Hz = FlickerTarget.Hz; // la frecuencia no se interpola (evita barridos por rangos peligrosos)
	FlickerNow.Depth = FMath::Lerp(FlickerNow.Depth, FlickerTarget.Depth, KF);
	FlickerNow.Shape = FlickerTarget.Shape;
	FlickerNow.Color = FMath::Lerp(FlickerNow.Color, FlickerTarget.Color, KF);

	// Respiracion: 0..1 inhalando sube, exhalando baja
	const float BreathTarget = bBreathActive ? ((BreathPhase < 0.5f) ? BreathPhase * 2.f : 1.f - (BreathPhase - 0.5f) * 2.f) : 0.5f;
	BreathNow = FMath::Lerp(BreathNow, BreathTarget, 1.f - FMath::Exp(-Dt * 6.f));

	FadeNow = FMath::FInterpConstantTo(FadeNow, FadeTarget, Dt, FadeSpeed);
	GuideNow = FMath::FInterpTo(GuideNow, bGuide ? 1.f : 0.f, Dt, 2.f);
	UpdateParticles(Dt);

	ApplyParams();
}

void AGatewayStage::ApplyParams()
{
	if (DomeMID)
	{
		DomeMID->SetScalarParameterValue(TEXT("ModeA"), CurA.Mode);
		DomeMID->SetScalarParameterValue(TEXT("ModeB"), CurB.Mode);
		DomeMID->SetScalarParameterValue(TEXT("Mix"), Mix);
		DomeMID->SetVectorParameterValue(TEXT("Color"), ColorNow);
		DomeMID->SetScalarParameterValue(TEXT("Intensity"), IntensityNow);
		DomeMID->SetScalarParameterValue(TEXT("Speed"), SpeedNow);
		DomeMID->SetScalarParameterValue(TEXT("Complexity"), ComplexityNow);
		DomeMID->SetScalarParameterValue(TEXT("Hue"), HueNow);
		DomeMID->SetScalarParameterValue(TEXT("Breath"), BreathNow);
		DomeMID->SetScalarParameterValue(TEXT("SceneTime"), SceneTime);
		DomeMID->SetScalarParameterValue(TEXT("Guide"), GuideNow);
		DomeMID->SetVectorParameterValue(TEXT("ViewForward"), FLinearColor(ViewForward.X, ViewForward.Y, ViewForward.Z, 0.f));
		if (CurA.Image) { DomeMID->SetTextureParameterValue(TEXT("ImageA"), CurA.Image); }
		if (CurB.Image) { DomeMID->SetTextureParameterValue(TEXT("ImageB"), CurB.Image); }
	}
	if (PostMID)
	{
		PostMID->SetScalarParameterValue(TEXT("FlickerHz"), FlickerNow.Hz);
		PostMID->SetScalarParameterValue(TEXT("FlickerDepth"), FlickerNow.Depth * FlickerUserScale);
		PostMID->SetScalarParameterValue(TEXT("FlickerShape"), FlickerNow.Shape);
		PostMID->SetVectorParameterValue(TEXT("FlickerColor"), FlickerNow.Color);
		PostMID->SetScalarParameterValue(TEXT("Fade"), FadeNow);
		PostMID->SetScalarParameterValue(TEXT("SceneTime"), SceneTime);
		PostMID->SetScalarParameterValue(TEXT("Breath"), BreathNow);
	}
}
