#pragma once

#include "CoreMinimal.h"
#include "GatewaySynth.h"
#include "GatewaySession.generated.h"

// Modos del shader del domo (ver Shaders/GatewayDome.ush)
UENUM(BlueprintType)
enum class EGatewayVisualMode : uint8
{
	Ganzfeld = 0,   // campo homogeneo de color (Ganzfeld)
	Orb = 1,        // esfera de energia que respira (REBAL)
	Tunnel = 2,     // tunel/vortice hacia adelante
	Mandala = 3,    // caleidoscopio fractal
	Cosmos = 4,     // estrellas + nebulosa (imagen)
	Void = 5,       // oscuridad con chispas lejanas (sueno)
	Box = 6,        // caja de conversion de energia (imagen + brillo)
};

USTRUCT(BlueprintType)
struct FGatewayVisual
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGatewayVisualMode Mode = EGatewayVisualMode::Ganzfeld;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor(0.35f, 0.1f, 0.6f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Intensity = 0.5f;   // brillo general 0..2
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 0.3f;       // velocidad de animacion
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Complexity = 0.5f;  // detalle / simetrias
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HueDrift = 0.f;     // ciclos de matiz por minuto
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Image;            // textura /Game/Gateway/Images/T_xxx (modos Cosmos/Box)
};

USTRUCT(BlueprintType)
struct FGatewayFlicker
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Hz = 0.f;       // 0 = apagado. Rango util 4-12 Hz (Ganzflicker ~10)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Depth = 0.f;    // 0..1 profundidad de modulacion
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shape = 0.f;    // 0 seno, 1 cuadrado
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct FGatewayBreath
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bActive = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Rpm = 6.f;          // respiraciones por minuto (6 = resonancia 0.1 Hz)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InhaleRatio = 0.4f; // fraccion del ciclo inhalando
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowGuide = true;   // dibujar el circulo guia
};

USTRUCT(BlueprintType)
struct FGatewaySegment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StartTime = 0.f;   // segundos desde el inicio de la sesion
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Voice;           // ruta relativa a Content/Gateway/Voice (vacio = silencio)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float VoiceGain = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Caption;         // texto breve en pantalla (opcional)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RampSeconds = 12.f;// transicion de audio/visual al entrar
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGatewaySoundscape Sound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGatewayVisual Visual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGatewayFlicker Flicker;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGatewayBreath Breath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bChime = false;     // campana al entrar (cue)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChimeHz = 528.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChimeGain = 0.35f;
};

USTRUCT(BlueprintType)
struct FGatewaySessionDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Wave;          // "Onda I - Descubrimiento", ...
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Order = 0;       // orden global del programa
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Requires;      // id de sesion recomendada antes
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TotalSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSleep = false;   // termina sin despertar (dormir)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNight = false;   // protocolo nocturno (pantalla casi negra, cues)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUsesFlicker = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGatewaySegment> Segments;
	UPROPERTY() FString FilePath;
};

UCLASS()
class GATEWAY_API UGatewaySessionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Carpeta Content/Gateway (sesiones, voces, imagenes)
	static FString ContentDir();
	static FString SessionsDir();
	static FString VoiceDir();
	static FString LogPath();

	static bool LoadSession(const FString& JsonPath, FGatewaySessionDef& Out, FString* Error = nullptr);
	static TArray<FGatewaySessionDef> LoadAllSessions();

	// Registro de sesiones completadas (Saved/gateway_log.txt): id;fecha;segundos
	static void AppendLog(const FString& SessionId, float Seconds, bool bCompleted);
	static TSet<FString> CompletedSessionIds();
};
