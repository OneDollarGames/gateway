#pragma once

#include "CoreMinimal.h"

// Cargador de WAV (PCM 16/24/32 bits y float 32) a estereo intercalado en float [-1,1],
// remuestreado linealmente a la frecuencia del mezclador. Se usa para las voces guia y
// los ambientes: todo el audio de la experiencia pasa por un unico sintetizador para que
// salga por el mismo dispositivo (los AirPods) con mezcla y ducking controlados.
struct FGatewayWavData
{
	TArray<float> Stereo;   // intercalado L R L R ...
	int32 SampleRate = 0;   // ya remuestreado
	float Seconds = 0.f;
};

namespace GatewayWav
{
	bool Load(const FString& Path, int32 TargetSampleRate, FGatewayWavData& Out, FString* Error = nullptr);
}
