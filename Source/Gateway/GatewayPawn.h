#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GatewayPawn.generated.h"

class UCameraComponent;
class AGatewayDirector;

// Observador: camara en el centro del domo con mirada suave por raton (muy lenta) y
// las teclas de la experiencia. No se mueve: el usuario esta recostado con los ojos cerrados
// la mayor parte del tiempo.
UCLASS()
class GATEWAY_API AGatewayPawn : public APawn
{
	GENERATED_BODY()
public:
	AGatewayPawn();
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera = nullptr;

private:
	float Yaw = 0.f, Pitch = 0.f, TargetYaw = 0.f, TargetPitch = 0.f;
	AGatewayDirector* Director() const;
	void LookX(float V);
	void LookY(float V);
	void KeyUp(); void KeyDown(); void KeyLeft(); void KeyRight();
	void KeyEnter(); void KeyEscape(); void KeySpace(); void KeySettings(); void KeyFullscreen(); void KeyHelp(); void KeyDevices();
	// VR (Quest Touch)
	void VrA(); void VrB(); void VrX(); void VrY(); void VrStickUp(); void VrStickDown(); void VrStickLeft(); void VrStickRight(); void VrSkip(); void VrMenu();
	bool bVR = false;
	// Sticks como ejes (OpenXR no manda las teclas digitales Thumbstick_Up/Down): umbral + rearme
	float AxLX = 0.f, AxLY = 0.f, AxRX = 0.f, AxRY = 0.f;
	int32 ArmLY = 0, ArmLX = 0, ArmRX = 0, ArmRY = 0;
	void AxisRY(float V) { AxRY = V; }
	void AxisLX(float V) { AxLX = V; }
	void AxisLY(float V) { AxLY = V; }
	void AxisRX(float V) { AxRX = V; }
	void TickSticks();
};

UCLASS()
class GATEWAY_API AGatewayPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AGatewayPlayerController();
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class GATEWAY_API AGatewayGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGatewayGameMode();
	virtual void BeginPlay() override;
};
