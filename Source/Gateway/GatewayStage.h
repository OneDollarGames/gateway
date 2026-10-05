#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatewaySession.h"
#include "GatewayStage.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class APostProcessVolume;
class UTexture2D;

// Escenario visual: un domo que envuelve a la camara con un material procedural
// (Shaders/GatewayDome.ush) y un post-proceso (Shaders/GatewayPost.ush) con flicker fotico,
// vineta, aberracion cromatica y fundidos. Todo se controla por parametros desde el director.
UCLASS()
class GATEWAY_API AGatewayStage : public AActor
{
	GENERATED_BODY()

public:
	AGatewayStage();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Objetivo visual nuevo (se funde en RampSeconds)
	void SetVisual(const FGatewayVisual& Target, float RampSeconds);
	void SetFlicker(const FGatewayFlicker& Target, float RampSeconds);
	void SetBreath(float Phase01, bool bActive);
	// Fundido a negro (1 = negro)
	void SetFade(float Target, float Seconds);
	// Escala de usuario para el flicker (ajustes de seguridad) 0..1
	void SetFlickerUserScale(float S) { FlickerUserScale = S; }
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }
	void SetViewForward(const FVector& F) { ViewForward = F; }
	void SetGuide(bool bOn) { bGuide = bOn; }
	float GetSceneTime() const { return SceneTime; }

	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Dome = nullptr;
	// Particulas 3D alrededor del usuario (profundidad real en VR): espiral REBAL, corriente del tunel, estrellas
	UPROPERTY(VisibleAnywhere) UInstancedStaticMeshComponent* Particles = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* ParticleMID = nullptr;
	// Objeto 3D del modo "caja": la Caja de Conversion de Energia (Blender) delante del usuario, con su luz
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Prop = nullptr;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* PropLight = nullptr;
	float PropAlpha = 0.f;
	UPROPERTY() UMaterialInstanceDynamic* DomeMID = nullptr;
	UPROPERTY() UTexture2D* BlackTex = nullptr;   // imagen "ninguna": sin esto el material conserva la ultima textura
	UPROPERTY() UMaterialInstanceDynamic* PostMID = nullptr;
	UPROPERTY() APostProcessVolume* PostVolume = nullptr;

private:
	struct FVisualState
	{
		float Mode = 0.f; FLinearColor Color = FLinearColor::Black; float Intensity = 0.f; float Speed = 0.3f; float Complexity = 0.5f; float Hue = 0.f; UTexture2D* Image = nullptr;
	};
	FVisualState CurA, CurB; // A = actual, B = destino (crossfade)
	float Mix = 0.f, MixRate = 0.f;
	FGatewayVisual TargetVisual; float VisualRamp = 10.f;
	FLinearColor ColorNow = FLinearColor::Black; float IntensityNow = 0.f, SpeedNow = 0.3f, ComplexityNow = 0.5f, HueNow = 0.f;
	float HueDriftPerMin = 0.f;

	FGatewayFlicker FlickerNow, FlickerTarget; float FlickerRamp = 5.f;
	float FlickerUserScale = 1.f;

	float BreathPhase = 0.f; bool bBreathActive = false; float BreathNow = 0.f;
	float FadeNow = 1.f, FadeTarget = 1.f, FadeSpeed = 1.f;
	float SceneTime = 0.f; bool bPaused = false;
	FVector ViewForward = FVector::ForwardVector;
	bool bGuide = false; float GuideNow = 0.f;

	UTexture2D* LoadImage(const FString& Name);
	void ApplyParams();

	// ---- particulas ----
	struct FParticle { float Seed = 0.f; float Radius = 1.f; float Angle = 0.f; float Height = 0.f; float Depth = 1.f; float Phase = 0.f; FVector Pos = FVector::ZeroVector; float Scale = 1.f; };
	TArray<FParticle> Parts;
	TArray<FTransform> PartXf;
	float ParticleAlpha = 0.f; // 0..1 cuanta "presencia" tienen (segun el modo)
	void InitParticles(int32 Count);
	void UpdateParticles(float Dt);
};
