#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatewaySession.h"
#include "GatewayDirector.generated.h"

class UGatewaySynth;
class AGatewayStage;
class UGatewayAudioDevices;
class AGatewayVRPanel;

UENUM()
enum class EGatewayState : uint8
{
	Menu,
	Warning,     // consentimiento de flicker / contraindicaciones
	Settings,
	Devices,
	Running,
	Finished,
};

USTRUCT()
struct FGatewaySettings
{
	GENERATED_BODY()
	UPROPERTY() float FlickerScale = 0.5f;   // 0..1 (0 = sin flicker)
	UPROPERTY() float VolTones = 0.8f;
	UPROPERTY() float VolVoices = 1.0f;
	UPROPERTY() float VolNoise = 0.8f;
	UPROPERTY() bool bFlickerConsent = false;
	UPROPERTY() bool bFullscreen = true;
	UPROPERTY() FString PreferredDevice;     // subcadena del nombre (p.ej. "AirPods")
};

// Director de la experiencia: menu, ajustes, dispositivos y la maquina de estados de la sesion.
// Lee las sesiones JSON de Content/Gateway/Sessions, manda el audio al sintetizador y el
// visual al escenario, y lleva el registro de sesiones completadas (programa progresivo).
UCLASS()
class GATEWAY_API AGatewayDirector : public AActor
{
	GENERATED_BODY()

public:
	AGatewayDirector();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- consultas para el HUD ----
	EGatewayState GetState() const { return State; }
	const TArray<FGatewaySessionDef>& GetSessions() const { return Sessions; }
	const TSet<FString>& GetCompleted() const { return Completed; }
	int32 GetMenuIndex() const { return MenuIndex; }
	int32 GetSettingsIndex() const { return SettingsIndex; }
	int32 GetDeviceIndex() const { return DeviceIndex; }
	const FGatewaySettings& GetSettings() const { return Settings; }
	UGatewayAudioDevices* GetDevices() const { return Devices; }
	const FGatewaySessionDef* CurrentSession() const { return (ActiveSession >= 0 && ActiveSession < Sessions.Num()) ? &Sessions[ActiveSession] : nullptr; }
	const FGatewaySegment* CurrentSegment() const;
	float GetElapsed() const { return Elapsed; }
	bool IsPaused() const { return bPaused; }
	float GetBreathPhase() const { return BreathPhase; }
	bool IsBreathGuideVisible() const;
	bool IsSessionRecommended(const FGatewaySessionDef& S) const;  // su "requiere" ya esta completa
	FString StatusLine;                                             // mensajes breves (dispositivo, etc.)
	bool bShowHelp = false;
	bool bShowOverlay = true;   // textos durante la sesion (se ocultan solos)
	float OverlayTimer = 0.f;

	// ---- acciones (teclado/HUD) ----
	void MenuMove(int32 Delta);
	void MenuConfirm();
	void MenuBack();
	void MenuAdjust(int32 Delta);      // izquierda/derecha en ajustes
	void TogglePause();
	void SkipSegment();
	void OpenSettings();
	void OpenDevices();
	void StartSession(int32 Index);
	void AbortSession();
	void AcceptWarning();
	void ToggleFullscreen();
	void ShowOverlay(float Seconds = 6.f) { bShowOverlay = true; OverlayTimer = Seconds; }

	UPROPERTY() UGatewaySynth* Synth = nullptr;
	UPROPERTY() AGatewayStage* Stage = nullptr;
	UPROPERTY() UGatewayAudioDevices* Devices = nullptr;
	UPROPERTY() AGatewayVRPanel* VRPanel = nullptr;
	bool bVR = false;

	static AGatewayDirector* Get(UWorld* World);

private:
	EGatewayState State = EGatewayState::Menu;
	TArray<FGatewaySessionDef> Sessions;
	TSet<FString> Completed;
	FGatewaySettings Settings;
	int32 MenuIndex = 0, SettingsIndex = 0, DeviceIndex = 0;
	int32 ActiveSession = -1, ActiveSegment = -1, PendingSession = -1;
	float Elapsed = 0.f; bool bPaused = false; float BreathPhase = 0.f;
	float FinishedTimer = 0.f;
	float DeviceRefreshTimer = 0.f;
	bool bLoggedThisSession = false;

	void ApplySegment(int32 Index);
	void FinishSession(bool bCompleted);
	void ApplyUserGains();
	void LoadSettings();
	void SaveSettings();
	void AutoSelectDevice();
	FString SettingsPath() const;
	void ApplyMenuVisual();
};
