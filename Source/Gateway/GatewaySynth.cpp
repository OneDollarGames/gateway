#include "GatewaySynth.h"
#include "Gateway.h"

UGatewaySynth::UGatewaySynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

bool UGatewaySynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	OutSampleRate = SampleRate;
	Smooth = 1.f - FMath::Exp(-1.f / (0.5f * SampleRate)); // rampa por defecto 0.5 s
	for (FLayerState& L : Layers) { L.Gain = L.TGain = 0.f; }
	return true;
}

void UGatewaySynth::SetSoundscape(const FGatewaySoundscape& In, float RampSeconds)
{
	FGatewaySoundscape Copy = In;
	const float Ramp = FMath::Max(0.05f, RampSeconds);
	SynthCommand([this, Copy, Ramp]()
	{
		Smooth = 1.f - FMath::Exp(-1.f / (Ramp * OutSampleRate));
		for (int32 i = 0; i < MaxLayers; ++i)
		{
			if (i < Copy.Layers.Num())
			{
				const FGatewayToneLayer& S = Copy.Layers[i];
				Layers[i].TCar = FMath::Clamp(S.CarrierHz, 20.f, 1500.f);
				Layers[i].TBeat = FMath::Clamp(S.BeatHz, 0.f, 60.f);
				Layers[i].TGain = FMath::Clamp(S.Gain, 0.f, 1.f);
				Layers[i].TIso = FMath::Clamp(S.IsoDepth, 0.f, 1.f);
				Layers[i].TPan = FMath::Clamp(S.Pan, -1.f, 1.f);
				// Si la capa estaba en silencio, arrancar ya en la frecuencia nueva (evita glissandos raros)
				if (Layers[i].Gain < 0.0005f) { Layers[i].Car = Layers[i].TCar; Layers[i].Beat = Layers[i].TBeat; }
			}
			else
			{
				Layers[i].TGain = 0.f;
			}
		}
		TPink = FMath::Clamp(Copy.PinkNoise, 0.f, 1.f);
		TBrown = FMath::Clamp(Copy.BrownNoise, 0.f, 1.f);
		TOcean = FMath::Clamp(Copy.OceanRate, 0.f, 2.f);
		TBreathTone = FMath::Clamp(Copy.BreathToneGain, 0.f, 1.f);
		TMaster = FMath::Clamp(Copy.Master, 0.f, 1.f);
	});
}

float UGatewaySynth::PlayVoice(const FString& WavPath, float Gain)
{
	TSharedPtr<FGatewayWavData, ESPMode::ThreadSafe> Data = MakeShared<FGatewayWavData, ESPMode::ThreadSafe>();
	FString Err;
	if (!GatewayWav::Load(WavPath, OutSampleRate, *Data, &Err))
	{
		UE_LOG(LogGateway, Warning, TEXT("Voz: %s"), *Err);
		return -1.f;
	}
	const float Seconds = Data->Seconds;
	VoicesPlaying.fetch_add(1);
	SynthCommand([this, Data, Gain]()
	{
		FVoice V; V.Data = Data; V.Gain = Gain;
		Voices.Add(MoveTemp(V));
	});
	return Seconds;
}

void UGatewaySynth::StopVoices(float FadeSeconds)
{
	const float Step = 1.f / FMath::Max(1.f, FadeSeconds * OutSampleRate);
	SynthCommand([this, Step]()
	{
		for (FVoice& V : Voices) { V.bStopping = true; V.FadeStep = Step; }
	});
}

void UGatewaySynth::PlayChime(float BaseHz, float Seconds, float Gain)
{
	// Campana sintetizada en el hilo de juego (barata) y encolada como una "voz" mas.
	TSharedPtr<FGatewayWavData, ESPMode::ThreadSafe> Data = MakeShared<FGatewayWavData, ESPMode::ThreadSafe>();
	const int32 Frames = int32(Seconds * OutSampleRate);
	Data->SampleRate = OutSampleRate;
	Data->Seconds = Seconds;
	Data->Stereo.SetNumUninitialized(Frames * 2);
	const float Partials[3] = { 1.f, 2.756f, 5.404f };   // relaciones de una campana tubular
	const float PGain[3] = { 1.f, 0.45f, 0.2f };
	for (int32 F = 0; F < Frames; ++F)
	{
		const float T = float(F) / OutSampleRate;
		float S = 0.f;
		for (int32 P = 0; P < 3; ++P)
		{
			S += PGain[P] * FMath::Sin(2.f * PI * BaseHz * Partials[P] * T) * FMath::Exp(-T * (1.2f + P * 1.5f));
		}
		const float Attack = FMath::Min(1.f, T / 0.01f);
		S *= Attack * 0.5f;
		Data->Stereo[F * 2] = S;
		Data->Stereo[F * 2 + 1] = S;
	}
	SynthCommand([this, Data, Gain]()
	{
		FVoice V; V.Data = Data; V.Gain = Gain;
		Chimes.Add(MoveTemp(V));
	});
}

void UGatewaySynth::SetBreath(float Phase01, bool bActive)
{
	SynthCommand([this, Phase01, bActive]() { BreathPhase = Phase01; bBreathActive = bActive; });
}

void UGatewaySynth::SetUserGains(float Tones, float VoicesG, float Noise)
{
	SynthCommand([this, Tones, VoicesG, Noise]() { UserTones = Tones; UserVoices = VoicesG; UserNoise = Noise; });
}

void UGatewaySynth::SetPaused(bool bInPaused)
{
	SynthCommand([this, bInPaused]() { bPaused = bInPaused; });
}

float UGatewaySynth::NextWhite()
{
	// xorshift32 -> [-1,1]
	Rng ^= Rng << 13; Rng ^= Rng >> 17; Rng ^= Rng << 5;
	return (Rng * (1.f / 4294967296.f)) * 2.f - 1.f;
}

float UGatewaySynth::NextPink()
{
	const float W = NextWhite();
	b0 = 0.99886f * b0 + W * 0.0555179f;
	b1 = 0.99332f * b1 + W * 0.0750759f;
	b2 = 0.96900f * b2 + W * 0.1538520f;
	b3 = 0.86650f * b3 + W * 0.3104856f;
	b4 = 0.55000f * b4 + W * 0.5329522f;
	b5 = -0.7616f * b5 - W * 0.0168980f;
	const float P = b0 + b1 + b2 + b3 + b4 + b5 + b6 + W * 0.5362f;
	b6 = W * 0.115926f;
	return P * 0.11f;
}

float UGatewaySynth::NextBrown()
{
	BrownState = (BrownState + 0.02f * NextWhite()) * 0.998f;
	return FMath::Clamp(BrownState * 3.5f, -1.f, 1.f);
}

void UGatewaySynth::MixVoices(TArray<FVoice>& List, float& L, float& R, float UserGain)
{
	for (int32 i = List.Num() - 1; i >= 0; --i)
	{
		FVoice& V = List[i];
		const TArray<float>& S = V.Data->Stereo;
		if (V.Pos * 2 + 1 >= S.Num() || (V.bStopping && V.Fade <= 0.f))
		{
			List.RemoveAtSwap(i);
			if (&List == &Voices) { VoicesPlaying.fetch_sub(1); }
			continue;
		}
		if (V.bStopping) { V.Fade = FMath::Max(0.f, V.Fade - V.FadeStep); }
		const float G = V.Gain * V.Fade * UserGain;
		L += S[V.Pos * 2] * G;
		R += S[V.Pos * 2 + 1] * G;
		++V.Pos;
	}
}

int32 UGatewaySynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 Frames = NumSamples / 2;
	const float InvSr = 1.f / OutSampleRate;
	const float DuckTarget = (Voices.Num() > 0) ? DuckFactor : 1.f;
	const float DuckSmooth = 1.f - FMath::Exp(-1.f / (0.8f * OutSampleRate));
	const float PauseTarget = bPaused ? 0.f : 1.f;
	const float PauseSmooth = 1.f - FMath::Exp(-1.f / (1.0f * OutSampleRate));

	for (int32 F = 0; F < Frames; ++F)
	{
		// suavizados
		for (FLayerState& Ly : Layers)
		{
			Ly.Car += (Ly.TCar - Ly.Car) * Smooth;
			Ly.Beat += (Ly.TBeat - Ly.Beat) * Smooth;
			Ly.Gain += (Ly.TGain - Ly.Gain) * Smooth;
			Ly.Iso += (Ly.TIso - Ly.Iso) * Smooth;
			Ly.Pan += (Ly.TPan - Ly.Pan) * Smooth;
		}
		Pink += (TPink - Pink) * Smooth;
		Brown += (TBrown - Brown) * Smooth;
		Ocean += (TOcean - Ocean) * Smooth;
		BreathTone += (TBreathTone - BreathTone) * Smooth;
		Master += (TMaster - Master) * Smooth;
		Duck += (DuckTarget - Duck) * DuckSmooth;
		PauseGain += (PauseTarget - PauseGain) * PauseSmooth;

		float L = 0.f, R = 0.f;

		// capas binaurales / isocronicas
		for (FLayerState& Ly : Layers)
		{
			if (Ly.Gain < 0.0002f) { continue; }
			const double FL = Ly.Car + Ly.Beat * 0.5;
			const double FR = Ly.Car - Ly.Beat * 0.5;
			Ly.PhL += FL * InvSr; if (Ly.PhL >= 1.0) Ly.PhL -= 1.0;
			Ly.PhR += FR * InvSr; if (Ly.PhR >= 1.0) Ly.PhR -= 1.0;
			Ly.PhIso += Ly.Beat * InvSr; if (Ly.PhIso >= 1.0) Ly.PhIso -= 1.0;
			float Am = 1.f;
			if (Ly.Iso > 0.001f)
			{
				// pulso isocronico suave (seno elevado) a la frecuencia del batido
				const float Pulse = 0.5f - 0.5f * FMath::Cos(2.f * PI * float(Ly.PhIso));
				Am = 1.f - Ly.Iso * (1.f - Pulse);
			}
			const float G = Ly.Gain * Am * 0.35f;
			const float GL = G * ((Ly.Pan > 0.f) ? (1.f - Ly.Pan) : 1.f);
			const float GR = G * ((Ly.Pan < 0.f) ? (1.f + Ly.Pan) : 1.f);
			L += FMath::Sin(2.f * PI * float(Ly.PhL)) * GL;
			R += FMath::Sin(2.f * PI * float(Ly.PhR)) * GR;
		}
		L *= UserTones; R *= UserTones;

		// ruidos (misma senal en ambos oidos: no interfiere con el batido)
		float Noise = 0.f;
		if (Pink > 0.0005f) { Noise += NextPink() * Pink; }
		if (Brown > 0.0005f) { Noise += NextBrown() * Brown * 0.6f; }
		if (Ocean > 0.001f)
		{
			OceanPh += Ocean * InvSr; if (OceanPh >= 1.0) OceanPh -= 1.0;
			Noise *= 0.55f + 0.45f * FMath::Sin(2.f * PI * float(OceanPh));
		}
		L += Noise * UserNoise; R += Noise * UserNoise;

		// tono de respiracion: sube al inhalar, baja al exhalar (guia sin palabras)
		if (BreathTone > 0.0005f && bBreathActive)
		{
			const float Env = (BreathPhase < 0.5f) ? (BreathPhase * 2.f) : (1.f - (BreathPhase - 0.5f) * 2.f);
			BreathEnv += (Env - BreathEnv) * 0.0005f;
			const float Hz = 180.f + 60.f * BreathEnv;
			BreathPh += Hz * InvSr; if (BreathPh >= 1.0) BreathPh -= 1.0;
			const float S = FMath::Sin(2.f * PI * float(BreathPh)) * BreathTone * (0.08f + 0.12f * BreathEnv) * UserTones;
			L += S; R += S;
		}

		// ducking de todo lo anterior mientras habla la guia
		L *= Duck; R *= Duck;

		MixVoices(Voices, L, R, UserVoices);
		MixVoices(Chimes, L, R, UserVoices);

		// master, pausa y limitador suave
		L *= Master * PauseGain; R *= Master * PauseGain;
		L = L / (1.f + FMath::Abs(L) * 0.35f);
		R = R / (1.f + FMath::Abs(R) * 0.35f);
		OutAudio[F * 2] = L;
		OutAudio[F * 2 + 1] = R;
	}
	return NumSamples;
}
