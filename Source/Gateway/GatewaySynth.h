#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "GatewayWav.h"
#include <atomic>
#include "GatewaySynth.generated.h"

// Una capa Hemi-Sync: dos senos, uno por oido, separados BeatHz alrededor de CarrierHz.
// El cerebro percibe el "batido" (CarrierL - CarrierR) aunque no exista en el aire.
// IsoDepth > 0 modula la amplitud de la capa a BeatHz (tono isocronico: entrena mejor
// el EEG que el binaural puro segun la literatura; se mezclan ambos).
USTRUCT(BlueprintType)
struct FGatewayToneLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CarrierHz = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BeatHz = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Gain = 0.f;       // 0..1 (lineal)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float IsoDepth = 0.f;   // 0..1
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Pan = 0.f;        // -1 solo izquierda, 0 centro, 1 solo derecha
};

// Paisaje sonoro completo de un segmento de sesion.
USTRUCT(BlueprintType)
struct FGatewaySoundscape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGatewayToneLayer> Layers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PinkNoise = 0.f;     // 0..1
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BrownNoise = 0.f;    // 0..1 (oceano/viento)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float OceanRate = 0.f;     // Hz de vaiven del ruido (0 = constante)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BreathToneGain = 0.f; // tono que sigue la respiracion guiada
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Master = 1.f;
};

// Sintetizador en tiempo real. Toda la experiencia sonora sale de aqui: capas binaurales,
// ruidos, tono de respiracion, voces guia (WAV) y campanas de senal (cue de sueno lucido).
UCLASS(ClassGroup = Gateway, meta = (BlueprintSpawnableComponent))
class GATEWAY_API UGatewaySynth : public USynthComponent
{
	GENERATED_BODY()

public:
	UGatewaySynth(const FObjectInitializer& ObjectInitializer);

	// Cambia el paisaje sonoro con rampa (segundos) para que no haya saltos.
	UFUNCTION(BlueprintCallable, Category = Gateway)
	void SetSoundscape(const FGatewaySoundscape& In, float RampSeconds);

	// Reproduce una voz guia desde un WAV (ruta absoluta). Devuelve su duracion o -1.
	UFUNCTION(BlueprintCallable, Category = Gateway)
	float PlayVoice(const FString& WavPath, float Gain = 1.f);

	UFUNCTION(BlueprintCallable, Category = Gateway)
	void StopVoices(float FadeSeconds = 0.5f);

	UFUNCTION(BlueprintPure, Category = Gateway)
	bool IsVoicePlaying() const { return VoicesPlaying.load() > 0; }

	// Campana sintetizada (cue de senal: 3 parciales con caida exponencial).
	UFUNCTION(BlueprintCallable, Category = Gateway)
	void PlayChime(float BaseHz = 528.f, float Seconds = 2.5f, float Gain = 0.4f);

	// Fase de respiracion 0..1 (0 = inicio inhalacion, 0.5 = inicio exhalacion) y si esta activa.
	void SetBreath(float Phase01, bool bActive);

	// Volumenes globales del usuario (ajustes)
	void SetUserGains(float Tones, float Voices, float Noise);

	// Ducking de tonos/ruido mientras habla la voz (0..1 = factor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gateway) float DuckFactor = 0.55f;

	// Pausa (silencio suave) sin perder fases
	void SetPaused(bool bInPaused);

	int32 GetOutputSampleRate() const { return OutSampleRate; }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	static constexpr int32 MaxLayers = 8;

	struct FLayerState
	{
		float Car = 100.f, Beat = 0.f, Gain = 0.f, Iso = 0.f, Pan = 0.f;      // actuales (suavizados)
		float TCar = 100.f, TBeat = 0.f, TGain = 0.f, TIso = 0.f, TPan = 0.f;  // objetivos
		double PhL = 0.0, PhR = 0.0, PhIso = 0.0;
	};

	struct FVoice
	{
		TSharedPtr<FGatewayWavData, ESPMode::ThreadSafe> Data;
		int32 Pos = 0;
		float Gain = 1.f;
		float Fade = 1.f;      // multiplicador que baja al parar
		float FadeStep = 0.f;  // por muestra cuando se para
		bool bStopping = false;
	};

	// ---- estado del hilo de audio ----
	int32 OutSampleRate = 48000;
	FLayerState Layers[MaxLayers];
	float Pink = 0.f, TPink = 0.f, Brown = 0.f, TBrown = 0.f, Ocean = 0.f, TOcean = 0.f;
	float BreathTone = 0.f, TBreathTone = 0.f, Master = 1.f, TMaster = 1.f;
	float Smooth = 0.f;          // coeficiente de suavizado por muestra
	float UserTones = 1.f, UserVoices = 1.f, UserNoise = 1.f;
	float Duck = 1.f;            // suavizado
	float PauseGain = 1.f; bool bPaused = false;
	float BreathPhase = 0.f; bool bBreathActive = false; float BreathEnv = 0.f;
	double OceanPh = 0.0, BreathPh = 0.0;
	// ruido rosa (Paul Kellet) y marron
	float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, BrownState = 0.f;
	uint32 Rng = 0x9E3779B9u;
	TArray<FVoice> Voices;
	TArray<FVoice> Chimes;

	std::atomic<int32> VoicesPlaying{ 0 };

	float NextWhite();
	float NextPink();
	float NextBrown();
	void MixVoices(TArray<FVoice>& List, float& L, float& R, float UserGain);
};
