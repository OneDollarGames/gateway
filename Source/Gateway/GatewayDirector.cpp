#include "GatewayDirector.h"
#include "Gateway.h"
#include "GatewaySynth.h"
#include "GatewayStage.h"
#include "GatewayAudioDevices.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "GatewayVRPanel.h"
#include "HeadMountedDisplayFunctionLibrary.h"

AGatewayDirector::AGatewayDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Synth = CreateDefaultSubobject<UGatewaySynth>(TEXT("Synth"));
	Synth->SetupAttachment(Root);
	Synth->bAutoActivate = true;
	Synth->bAllowSpatialization = false;
}

AGatewayDirector* AGatewayDirector::Get(UWorld* World)
{
	for (TActorIterator<AGatewayDirector> It(World); It; ++It) { return *It; }
	return nullptr;
}

void AGatewayDirector::BeginPlay()
{
	Super::BeginPlay();
	LoadSettings();
	Sessions = UGatewaySessionLibrary::LoadAllSessions();
	Completed = UGatewaySessionLibrary::CompletedSessionIds();
	UE_LOG(LogGateway, Log, TEXT("%d sesiones cargadas, %d completadas"), Sessions.Num(), Completed.Num());

	// Escenario visual
	Stage = nullptr;
	for (TActorIterator<AGatewayStage> It(GetWorld()); It; ++It) { Stage = *It; break; }
	if (!Stage) { Stage = GetWorld()->SpawnActor<AGatewayStage>(AGatewayStage::StaticClass(), FTransform::Identity); }
	Stage->SetFlickerUserScale(Settings.FlickerScale);

	// Visor: menu flotante 3D (el HUD de canvas no se ve en VR)
	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() || FParse::Param(FCommandLine::Get(), TEXT("gwvrpanel"));
	if (bVR)
	{
		VRPanel = GetWorld()->SpawnActor<AGatewayVRPanel>(AGatewayVRPanel::StaticClass(), FTransform::Identity);
		UE_LOG(LogGateway, Log, TEXT("Modo VR: panel 3D creado"));
	}

	// Audio
	Devices = NewObject<UGatewayAudioDevices>(this);
	if (Synth && !Synth->IsPlaying()) { Synth->Start(); }
	ApplyUserGains();
#if !PLATFORM_ANDROID
	AutoSelectDevice();
#endif

	if (Settings.bFullscreen)
	{
		if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			GUS->SetFullscreenMode(EWindowMode::WindowedFullscreen);
			GUS->ApplySettings(false);
		}
	}
	// Pone al menu un fondo vivo: cosmos lento y tono suave
	ApplyMenuVisual();
	// Selecciona la primera sesion no completada
	for (int32 i = 0; i < Sessions.Num(); ++i) { if (!Completed.Contains(Sessions[i].Id)) { MenuIndex = i; break; } }
}

void AGatewayDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (State == EGatewayState::Running) { FinishSession(false); }
	SaveSettings();
	Super::EndPlay(Reason);
}

void AGatewayDirector::ApplyMenuVisual()
{
	// Fondo del menu: galaxia tenue en indigo, deriva lentisima (el usuario pidio algo suave y "espiritual")
	FGatewayVisual V; V.Mode = EGatewayVisualMode::Cosmos; V.Color = FLinearColor(0.10f, 0.14f, 0.38f); V.Intensity = 0.13f; V.Speed = 0.03f; V.Complexity = 0.5f; V.Image = TEXT("T_Galaxy01");
	Stage->SetVisual(V, 4.f);
	FGatewayFlicker F; Stage->SetFlicker(F, 1.f);
	Stage->SetFade(0.f, 2.f);
	FGatewaySoundscape S;
	FGatewayToneLayer L; L.CarrierHz = 220.f; L.BeatHz = 0.f; L.Gain = 0.f; S.Layers.Add(L);
	S.PinkNoise = 0.0f; S.BrownNoise = 0.06f; S.OceanRate = 0.09f;
	Synth->SetSoundscape(S, 3.f);
	Synth->SetBreath(0.f, false);
}

void AGatewayDirector::ApplyUserGains()
{
	if (Synth) { Synth->SetUserGains(Settings.VolTones, Settings.VolVoices, Settings.VolNoise); }
	if (Stage) { Stage->SetFlickerUserScale(Settings.FlickerScale); }
}

void AGatewayDirector::AutoSelectDevice()
{
	if (!Devices) return;
	Devices->Refresh(this);
	DeviceRefreshTimer = 1.5f; // al tener la lista, elegir
}

const FGatewaySegment* AGatewayDirector::CurrentSegment() const
{
	const FGatewaySessionDef* S = CurrentSession();
	return (S && ActiveSegment >= 0 && ActiveSegment < S->Segments.Num()) ? &S->Segments[ActiveSegment] : nullptr;
}

bool AGatewayDirector::IsBreathGuideVisible() const
{
	const FGatewaySegment* Seg = CurrentSegment();
	return Seg && Seg->Breath.bActive && Seg->Breath.bShowGuide;
}

bool AGatewayDirector::IsSessionRecommended(const FGatewaySessionDef& S) const
{
	return S.Requires.IsEmpty() || Completed.Contains(S.Requires);
}

// ---------------- menu ----------------

void AGatewayDirector::MenuMove(int32 Delta)
{
	switch (State)
	{
	case EGatewayState::Menu:
		if (Sessions.Num() > 0) { MenuIndex = (MenuIndex + Delta + Sessions.Num()) % Sessions.Num(); }
		break;
	case EGatewayState::Settings:
		SettingsIndex = (SettingsIndex + Delta + 6) % 6;
		break;
	case EGatewayState::Devices:
	{
		const int32 N = Devices ? Devices->GetDevices().Num() : 0;
		if (N > 0) { DeviceIndex = (DeviceIndex + Delta + N) % N; }
		break;
	}
	default: break;
	}
}

void AGatewayDirector::MenuAdjust(int32 Delta)
{
	if (State != EGatewayState::Settings) return;
	const float Step = 0.1f * Delta;
	switch (SettingsIndex)
	{
	case 0: Settings.FlickerScale = FMath::Clamp(Settings.FlickerScale + Step, 0.f, 1.f); break;
	case 1: Settings.VolTones = FMath::Clamp(Settings.VolTones + Step, 0.f, 1.f); break;
	case 2: Settings.VolVoices = FMath::Clamp(Settings.VolVoices + Step, 0.f, 1.5f); break;
	case 3: Settings.VolNoise = FMath::Clamp(Settings.VolNoise + Step, 0.f, 1.f); break;
	case 4: Settings.bFullscreen = !Settings.bFullscreen; ToggleFullscreen(); break;
	case 5: break; // dispositivos (Enter)
	}
	ApplyUserGains();
	SaveSettings();
}

void AGatewayDirector::MenuConfirm()
{
	switch (State)
	{
	case EGatewayState::Menu:
		StartSession(MenuIndex);
		break;
	case EGatewayState::Warning:
		AcceptWarning();
		break;
	case EGatewayState::Settings:
		if (SettingsIndex == 5) { OpenDevices(); }
		else if (SettingsIndex == 4) { MenuAdjust(1); }
		break;
	case EGatewayState::Devices:
		if (Devices && Devices->GetDevices().IsValidIndex(DeviceIndex))
		{
			const FAudioOutputDeviceInfo& D = Devices->GetDevices()[DeviceIndex];
			Settings.PreferredDevice = D.Name;
			Devices->Swap(this, D.DeviceId);
			StatusLine = FString::Printf(TEXT("Salida: %s"), *D.Name);
			SaveSettings();
			State = EGatewayState::Settings;
		}
		break;
	case EGatewayState::Finished:
		State = EGatewayState::Menu;
		ApplyMenuVisual();
		break;
	default: break;
	}
}

void AGatewayDirector::MenuBack()
{
	switch (State)
	{
	case EGatewayState::Warning: PendingSession = -1; State = EGatewayState::Menu; break;
	case EGatewayState::Settings: State = EGatewayState::Menu; break;
	case EGatewayState::Devices: State = EGatewayState::Settings; break;
	case EGatewayState::Running: AbortSession(); break;
	case EGatewayState::Finished: State = EGatewayState::Menu; ApplyMenuVisual(); break;
	case EGatewayState::Menu: bShowHelp = !bShowHelp; break;
	}
}

void AGatewayDirector::OpenSettings() { if (State == EGatewayState::Menu) { State = EGatewayState::Settings; } }

void AGatewayDirector::OpenDevices()
{
	State = EGatewayState::Devices;
	if (Devices) { Devices->Refresh(this); }
	DeviceIndex = 0;
}

void AGatewayDirector::ToggleFullscreen()
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const bool bFull = GUS->GetFullscreenMode() != EWindowMode::Windowed;
		GUS->SetFullscreenMode(bFull ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
		GUS->ApplySettings(false);
		Settings.bFullscreen = !bFull;
	}
}

// ---------------- sesion ----------------

void AGatewayDirector::StartSession(int32 Index)
{
	if (!Sessions.IsValidIndex(Index)) return;
	const FGatewaySessionDef& S = Sessions[Index];
	if (S.bUsesFlicker && !Settings.bFlickerConsent && Settings.FlickerScale > 0.f)
	{
		PendingSession = Index;
		State = EGatewayState::Warning;
		return;
	}
	ActiveSession = Index;
	ActiveSegment = -1;
	Elapsed = 0.f;
	bPaused = false;
	bLoggedThisSession = false;
	State = EGatewayState::Running;
	ShowOverlay(10.f);
	Stage->SetFade(0.f, 2.f);
	Synth->SetPaused(false);
	UE_LOG(LogGateway, Log, TEXT("Sesion iniciada: %s (%.0f s, %d segmentos)"), *S.Title, S.TotalSeconds, S.Segments.Num());
}

void AGatewayDirector::AcceptWarning()
{
	Settings.bFlickerConsent = true;
	SaveSettings();
	const int32 P = PendingSession; PendingSession = -1;
	State = EGatewayState::Menu;
	if (P >= 0) { StartSession(P); }
}

void AGatewayDirector::AbortSession()
{
	if (State != EGatewayState::Running) return;
	FinishSession(false);
	State = EGatewayState::Menu;
	ApplyMenuVisual();
	Synth->StopVoices(0.8f);
}

void AGatewayDirector::FinishSession(bool bCompleted)
{
	if (const FGatewaySessionDef* S = CurrentSession())
	{
		if (!bLoggedThisSession && Elapsed > 30.f)
		{
			UGatewaySessionLibrary::AppendLog(S->Id, Elapsed, bCompleted);
			if (bCompleted) { Completed.Add(S->Id); }
			bLoggedThisSession = true;
		}
	}
	bPaused = false;
	Synth->SetPaused(false);
	Stage->SetPaused(false);
	Stage->SetGuide(false);
	Stage->SetBreath(0.f, false);
	Synth->SetBreath(0.f, false);
}

void AGatewayDirector::TogglePause()
{
	if (State != EGatewayState::Running) return;
	bPaused = !bPaused;
	Synth->SetPaused(bPaused);
	Stage->SetPaused(bPaused);
	ShowOverlay(bPaused ? 3600.f : 4.f);
}

void AGatewayDirector::SkipSegment()
{
	const FGatewaySessionDef* S = CurrentSession();
	if (State != EGatewayState::Running || !S) return;
	const int32 Next = ActiveSegment + 1;
	if (S->Segments.IsValidIndex(Next))
	{
		Elapsed = S->Segments[Next].StartTime;
		Synth->StopVoices(0.3f);
		ShowOverlay(5.f);
	}
	else
	{
		Elapsed = S->TotalSeconds;
	}
}

void AGatewayDirector::ApplySegment(int32 Index)
{
	const FGatewaySessionDef* S = CurrentSession();
	if (!S || !S->Segments.IsValidIndex(Index)) return;
	const FGatewaySegment& Seg = S->Segments[Index];
	ActiveSegment = Index;
	UE_LOG(LogGateway, Log, TEXT("  [%6.0fs] %s"), Elapsed, *Seg.Name);

	Synth->SetSoundscape(Seg.Sound, Seg.RampSeconds);
	Stage->SetVisual(Seg.Visual, Seg.RampSeconds);
	Stage->SetFlicker(Seg.Flicker, FMath::Max(3.f, Seg.RampSeconds * 0.5f));
	if (Seg.bChime) { Synth->PlayChime(Seg.ChimeHz, 3.f, Seg.ChimeGain); }
	if (!Seg.Voice.IsEmpty())
	{
		const FString Path = FPaths::Combine(UGatewaySessionLibrary::VoiceDir(), Seg.Voice);
		const float Len = Synth->PlayVoice(Path, Seg.VoiceGain);
		if (Len < 0.f) { StatusLine = FString::Printf(TEXT("Falta la voz %s"), *Seg.Voice); }
	}
	if (!Seg.Caption.IsEmpty()) { ShowOverlay(8.f); }
}

void AGatewayDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Seleccion automatica de salida cuando llega la lista
	if (DeviceRefreshTimer > 0.f)
	{
		DeviceRefreshTimer -= DeltaSeconds;
		if (DeviceRefreshTimer <= 0.f && Devices && Devices->GetDevices().Num() > 0)
		{
			bool bDone = false;
			if (!Settings.PreferredDevice.IsEmpty())
			{
				for (const FAudioOutputDeviceInfo& D : Devices->GetDevices())
				{
					if (D.Name.Contains(Settings.PreferredDevice)) { if (!D.bIsCurrentDevice) { Devices->Swap(this, D.DeviceId); } bDone = true; StatusLine = FString::Printf(TEXT("Salida: %s"), *D.Name); break; }
				}
			}
			if (!bDone)
			{
				Devices->AutoSelectHeadphones(this);
				StatusLine.Empty(); // el HUD muestra el dispositivo actual en vivo
			}
		}
	}

	if (OverlayTimer > 0.f) { OverlayTimer -= DeltaSeconds; if (OverlayTimer <= 0.f) { bShowOverlay = false; } }

	// Direccion de la vista para el shader (tunel centrado donde mira el usuario)
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FVector Loc; FRotator Rot; PC->GetPlayerViewPoint(Loc, Rot);
		Stage->SetViewForward(Rot.Vector());
		Stage->SetActorLocation(Loc);
	}

	if (State != EGatewayState::Running) return;
	const FGatewaySessionDef* S = CurrentSession();
	if (!S) { State = EGatewayState::Menu; return; }

	if (!bPaused) { Elapsed += DeltaSeconds; }

	// Segmento actual
	int32 Idx = ActiveSegment;
	for (int32 i = 0; i < S->Segments.Num(); ++i) { if (S->Segments[i].StartTime <= Elapsed) { Idx = i; } }
	if (Idx != ActiveSegment)
	{
		// Si se salto mas de uno (p.ej. por el usuario), solo aplicar el ultimo
		ApplySegment(Idx);
	}

	// Respiracion guiada
	if (const FGatewaySegment* Seg = CurrentSegment())
	{
		if (Seg->Breath.bActive && !bPaused)
		{
			const float T = Elapsed - Seg->StartTime;
			const float Cycle = 60.f / FMath::Max(2.f, Seg->Breath.Rpm);
			const float Raw = FMath::Fmod(T, Cycle) / Cycle;
			const float In = FMath::Clamp(Seg->Breath.InhaleRatio, 0.2f, 0.7f);
			BreathPhase = (Raw < In) ? (Raw / In) * 0.5f : 0.5f + ((Raw - In) / (1.f - In)) * 0.5f;
		}
		Stage->SetBreath(BreathPhase, Seg->Breath.bActive);
		Synth->SetBreath(BreathPhase, Seg->Breath.bActive);
		Stage->SetGuide(Seg->Breath.bActive && Seg->Breath.bShowGuide && !bPaused);
	}

	if (Elapsed >= S->TotalSeconds)
	{
		FinishSession(true);
		State = EGatewayState::Finished;
		FinishedTimer = 0.f;
		if (S->bSleep || S->bNight)
		{
			// Dormir: todo a negro y silencio, sin pantalla de cierre luminosa
			Stage->SetFade(1.f, 20.f);
			FGatewaySoundscape Silence; Synth->SetSoundscape(Silence, 20.f);
		}
		else
		{
			ApplyMenuVisual();
		}
	}
}

// ---------------- ajustes ----------------

FString AGatewayDirector::SettingsPath() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("gateway_settings.json")); }

void AGatewayDirector::LoadSettings()
{
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *SettingsPath()))
	{
		FGatewaySettings Loaded;
		if (FJsonObjectConverter::JsonObjectStringToUStruct(Text, &Loaded, 0, 0)) { Settings = Loaded; }
	}
}

void AGatewayDirector::SaveSettings()
{
	FString Text;
	if (FJsonObjectConverter::UStructToJsonObjectString(Settings, Text))
	{
		FFileHelper::SaveStringToFile(Text, *SettingsPath());
	}
}
