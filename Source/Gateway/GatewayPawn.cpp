#include "GatewayPawn.h"
#include "Gateway.h"
#include "GatewayDirector.h"
#include "GatewayHUD.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "GatewayVRPanel.h"

AGatewayPawn::AGatewayPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(100.f);
	Camera->PostProcessBlendWeight = 0.f;
	bUseControllerRotationYaw = false;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

AGatewayDirector* AGatewayPawn::Director() const { return AGatewayDirector::Get(GetWorld()); }

void AGatewayPawn::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxisKey(EKeys::MouseX, this, &AGatewayPawn::LookX);
	In->BindAxisKey(EKeys::MouseY, this, &AGatewayPawn::LookY);
	In->BindKey(EKeys::Up, IE_Pressed, this, &AGatewayPawn::KeyUp);
	In->BindKey(EKeys::Down, IE_Pressed, this, &AGatewayPawn::KeyDown);
	In->BindKey(EKeys::W, IE_Pressed, this, &AGatewayPawn::KeyUp);
	In->BindKey(EKeys::S, IE_Pressed, this, &AGatewayPawn::KeyDown);
	In->BindKey(EKeys::Left, IE_Pressed, this, &AGatewayPawn::KeyLeft);
	In->BindKey(EKeys::Right, IE_Pressed, this, &AGatewayPawn::KeyRight);
	In->BindKey(EKeys::Enter, IE_Pressed, this, &AGatewayPawn::KeyEnter);
	In->BindKey(EKeys::Escape, IE_Pressed, this, &AGatewayPawn::KeyEscape);
	In->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AGatewayPawn::KeySpace);
	In->BindKey(EKeys::O, IE_Pressed, this, &AGatewayPawn::KeySettings);
	In->BindKey(EKeys::F, IE_Pressed, this, &AGatewayPawn::KeyFullscreen);
	In->BindKey(EKeys::H, IE_Pressed, this, &AGatewayPawn::KeyHelp);
	In->BindKey(EKeys::D, IE_Pressed, this, &AGatewayPawn::KeyDevices);

	// Mandos Touch del Quest (OpenXR): A/gatillo confirmar, B volver, X pausa/ajustes, Y recentrar, sticks navegar
	In->BindKey(EKeys::OculusTouch_Right_A_Click, IE_Pressed, this, &AGatewayPawn::VrA);
	// Solo mando derecho: A confirmar/pausa, B volver, click del stick = ajustes/pausa, gatillo = recentrar
	In->BindKey(EKeys::OculusTouch_Right_Trigger_Click, IE_Pressed, this, &AGatewayPawn::VrY);
	In->BindKey(EKeys::OculusTouch_Right_Thumbstick_Click, IE_Pressed, this, &AGatewayPawn::VrX);
	In->BindKey(EKeys::OculusTouch_Right_B_Click, IE_Pressed, this, &AGatewayPawn::VrB);
	In->BindKey(EKeys::OculusTouch_Left_X_Click, IE_Pressed, this, &AGatewayPawn::VrX);
	In->BindKey(EKeys::OculusTouch_Left_Y_Click, IE_Pressed, this, &AGatewayPawn::VrY);
	In->BindKey(EKeys::OculusTouch_Left_Thumbstick_Up, IE_Pressed, this, &AGatewayPawn::VrStickUp);
	In->BindKey(EKeys::OculusTouch_Left_Thumbstick_Down, IE_Pressed, this, &AGatewayPawn::VrStickDown);
	In->BindKey(EKeys::OculusTouch_Left_Thumbstick_Left, IE_Pressed, this, &AGatewayPawn::VrStickLeft);
	In->BindKey(EKeys::OculusTouch_Left_Thumbstick_Right, IE_Pressed, this, &AGatewayPawn::VrStickRight);
	In->BindKey(EKeys::OculusTouch_Right_Thumbstick_Up, IE_Pressed, this, &AGatewayPawn::VrStickUp);
	In->BindKey(EKeys::OculusTouch_Right_Thumbstick_Down, IE_Pressed, this, &AGatewayPawn::VrStickDown);
	In->BindKey(EKeys::OculusTouch_Right_Thumbstick_Right, IE_Pressed, this, &AGatewayPawn::VrSkip);
	In->BindKey(EKeys::OculusTouch_Left_Menu_Click, IE_Pressed, this, &AGatewayPawn::VrMenu);
	In->BindAxisKey(EKeys::OculusTouch_Left_Thumbstick_X, this, &AGatewayPawn::AxisLX);
	In->BindAxisKey(EKeys::OculusTouch_Left_Thumbstick_Y, this, &AGatewayPawn::AxisLY);
	In->BindAxisKey(EKeys::OculusTouch_Right_Thumbstick_X, this, &AGatewayPawn::AxisRX);
	In->BindAxisKey(EKeys::OculusTouch_Right_Thumbstick_Y, this, &AGatewayPawn::AxisRY);

	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	if (bVR)
	{
		// Experiencia sentada/acostada: el origen es la posicion inicial del visor
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Local);
		UE_LOG(LogGateway, Log, TEXT("VR activo: %s"), *UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName().ToString());
	}
}

void AGatewayPawn::VrA() { if (AGatewayDirector* D = Director()) { if (D->GetState() == EGatewayState::Running) D->TogglePause(); else D->MenuConfirm(); } }
void AGatewayPawn::VrB() { if (AGatewayDirector* D = Director()) D->MenuBack(); }
void AGatewayPawn::VrX()
{
	if (AGatewayDirector* D = Director())
	{
		if (D->GetState() == EGatewayState::Running) D->TogglePause();
		else if (D->GetState() == EGatewayState::Menu) D->OpenSettings();
	}
}
void AGatewayPawn::VrY()
{
	for (TActorIterator<AGatewayVRPanel> It(GetWorld()); It; ++It) { It->Recenter(); }
	if (bVR) { UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(); }
	if (AGatewayDirector* D = Director()) { D->ShowOverlay(6.f); }
}
void AGatewayPawn::VrStickUp() { KeyUp(); }
void AGatewayPawn::VrStickDown() { KeyDown(); }
void AGatewayPawn::VrStickLeft() { KeyLeft(); }
void AGatewayPawn::VrStickRight() { if (AGatewayDirector* D = Director()) { if (D->GetState() != EGatewayState::Running) D->MenuAdjust(1); } }
void AGatewayPawn::VrSkip() { if (AGatewayDirector* D = Director()) { if (D->GetState() == EGatewayState::Running) D->SkipSegment(); else D->MenuAdjust(1); } }
void AGatewayPawn::VrMenu() { if (AGatewayDirector* D = Director()) { if (D->GetState() == EGatewayState::Running) D->TogglePause(); else { D->bShowHelp = !D->bShowHelp; } } }

void AGatewayPawn::LookX(float V)
{
	if (bVR) return;
	if (AGatewayDirector* D = Director()) { if (D->GetState() != EGatewayState::Running) return; }
	TargetYaw = FMath::Clamp(TargetYaw + V * 0.08f, -60.f, 60.f);
}
void AGatewayPawn::LookY(float V)
{
	if (bVR) return;
	if (AGatewayDirector* D = Director()) { if (D->GetState() != EGatewayState::Running) return; }
	TargetPitch = FMath::Clamp(TargetPitch + V * 0.08f, -40.f, 40.f);
}

void AGatewayPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// La mirada vuelve sola al centro muy despacio: nada que "perseguir"
	TargetYaw = FMath::FInterpTo(TargetYaw, 0.f, DeltaSeconds, 0.05f);
	TargetPitch = FMath::FInterpTo(TargetPitch, 0.f, DeltaSeconds, 0.05f);
	Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 2.f);
	Pitch = FMath::FInterpTo(Pitch, TargetPitch, DeltaSeconds, 2.f);
	if (!bVR) { Camera->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f)); }
	TickSticks();
}

void AGatewayPawn::TickSticks()
{
	auto Step = [](float V, int32& Arm) -> int32
	{
		if (V > 0.6f && Arm != 1) { Arm = 1; return 1; }
		if (V < -0.6f && Arm != -1) { Arm = -1; return -1; }
		if (FMath::Abs(V) < 0.3f) { Arm = 0; }
		return 0;
	};
	const int32 Y = Step(AxLY, ArmLY);
	if (Y > 0) KeyUp(); else if (Y < 0) KeyDown();
	const int32 X = Step(AxLX, ArmLX);
	if (X > 0) VrStickRight(); else if (X < 0) KeyLeft();
	const int32 RX = Step(AxRX, ArmRX);
	if (RX > 0) VrSkip(); else if (RX < 0) KeyLeft();
	const int32 RY = Step(AxRY, ArmRY);
	if (RY > 0) KeyUp(); else if (RY < 0) KeyDown();
}

void AGatewayPawn::KeyUp() { if (AGatewayDirector* D = Director()) D->MenuMove(-1); }
void AGatewayPawn::KeyDown() { if (AGatewayDirector* D = Director()) D->MenuMove(1); }
void AGatewayPawn::KeyLeft() { if (AGatewayDirector* D = Director()) D->MenuAdjust(-1); }
void AGatewayPawn::KeyRight()
{
	if (AGatewayDirector* D = Director())
	{
		if (D->GetState() == EGatewayState::Running) D->SkipSegment(); else D->MenuAdjust(1);
	}
}
void AGatewayPawn::KeyEnter() { if (AGatewayDirector* D = Director()) D->MenuConfirm(); }
void AGatewayPawn::KeyEscape() { if (AGatewayDirector* D = Director()) D->MenuBack(); }
void AGatewayPawn::KeySpace()
{
	if (AGatewayDirector* D = Director())
	{
		if (D->GetState() == EGatewayState::Running) D->TogglePause(); else D->MenuConfirm();
	}
}
void AGatewayPawn::KeySettings() { if (AGatewayDirector* D = Director()) { if (D->GetState() == EGatewayState::Menu) D->OpenSettings(); } }
void AGatewayPawn::KeyFullscreen() { if (AGatewayDirector* D = Director()) D->ToggleFullscreen(); }
void AGatewayPawn::KeyHelp() { if (AGatewayDirector* D = Director()) { D->bShowHelp = !D->bShowHelp; D->ShowOverlay(8.f); } }
void AGatewayPawn::KeyDevices() { if (AGatewayDirector* D = Director()) { if (D->GetState() == EGatewayState::Menu || D->GetState() == EGatewayState::Settings) D->OpenDevices(); } }

// ---------------- controller ----------------

AGatewayPlayerController::AGatewayPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AGatewayPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AGatewayDirector* D = AGatewayDirector::Get(GetWorld()))
	{
		const bool bRunning = D->GetState() == EGatewayState::Running;
		if (bShowMouseCursor == bRunning)
		{
			bShowMouseCursor = !bRunning;
			if (bRunning) { SetInputMode(FInputModeGameOnly()); }
			else { FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(M); }
		}
	}
}

// ---------------- game mode ----------------

AGatewayGameMode::AGatewayGameMode()
{
	DefaultPawnClass = AGatewayPawn::StaticClass();
	PlayerControllerClass = AGatewayPlayerController::StaticClass();
	HUDClass = AGatewayHUD::StaticClass();
}

void AGatewayGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!AGatewayDirector::Get(GetWorld()))
	{
		GetWorld()->SpawnActor<AGatewayDirector>(AGatewayDirector::StaticClass(), FTransform::Identity);
	}
}
