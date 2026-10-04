#include "GatewayWav.h"
#include "Misc/FileHelper.h"

namespace
{
	uint32 ReadU32(const uint8* P) { return P[0] | (P[1] << 8) | (P[2] << 16) | (uint32(P[3]) << 24); }
	uint16 ReadU16(const uint8* P) { return P[0] | (P[1] << 8); }
}

bool GatewayWav::Load(const FString& Path, int32 TargetSampleRate, FGatewayWavData& Out, FString* Error)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		if (Error) *Error = FString::Printf(TEXT("No se pudo leer %s"), *Path);
		return false;
	}
	if (Bytes.Num() < 12 || FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
	{
		if (Error) *Error = FString::Printf(TEXT("%s no es un WAV RIFF"), *Path);
		return false;
	}

	uint16 Format = 0, Channels = 0, Bits = 0;
	uint32 Rate = 0;
	const uint8* Data = nullptr;
	uint32 DataLen = 0;

	int32 Pos = 12;
	while (Pos + 8 <= Bytes.Num())
	{
		const uint8* Chunk = Bytes.GetData() + Pos;
		uint32 Len = ReadU32(Chunk + 4);
		const uint8* Body = Chunk + 8;
		if (FMemory::Memcmp(Chunk, "fmt ", 4) == 0 && Len >= 16)
		{
			Format = ReadU16(Body);
			Channels = ReadU16(Body + 2);
			Rate = ReadU32(Body + 4);
			Bits = ReadU16(Body + 14);
			if (Format == 0xFFFE && Len >= 26) // WAVE_FORMAT_EXTENSIBLE: subformato en el GUID
			{
				Format = ReadU16(Body + 24);
			}
		}
		else if (FMemory::Memcmp(Chunk, "data", 4) == 0)
		{
			Data = Body;
			// Algunos generadores (OpenAI TTS en streaming) escriben longitud 0 o 0xFFFFFFFF: usar el resto del archivo.
			const uint32 Remaining = uint32(Bytes.Num() - (Pos + 8));
			DataLen = (Len == 0 || Len > Remaining) ? Remaining : Len;
			break;
		}
		Pos += 8 + Len + (Len & 1);
	}

	if (!Data || Channels == 0 || Rate == 0 || Bits == 0)
	{
		if (Error) *Error = FString::Printf(TEXT("%s: cabecera WAV incompleta"), *Path);
		return false;
	}
	if (!(Format == 1 || Format == 3))
	{
		if (Error) *Error = FString::Printf(TEXT("%s: formato WAV %d no soportado (solo PCM/float)"), *Path, Format);
		return false;
	}

	const int32 BytesPerSample = Bits / 8;
	const int32 Frames = DataLen / (BytesPerSample * Channels);
	if (Frames <= 0)
	{
		if (Error) *Error = FString::Printf(TEXT("%s: sin muestras"), *Path);
		return false;
	}

	// Decodificar a estereo float a la frecuencia original
	TArray<float> Src;
	Src.SetNumUninitialized(Frames * 2);
	for (int32 F = 0; F < Frames; ++F)
	{
		float Acc[2] = { 0.f, 0.f };
		for (int32 C = 0; C < Channels; ++C)
		{
			const uint8* S = Data + (F * Channels + C) * BytesPerSample;
			float V = 0.f;
			if (Format == 3 && Bits == 32)
			{
				float Tmp; FMemory::Memcpy(&Tmp, S, 4); V = Tmp;
			}
			else if (Bits == 16)
			{
				V = int16(ReadU16(S)) / 32768.f;
			}
			else if (Bits == 24)
			{
				int32 I = (S[0] << 8) | (S[1] << 16) | (S[2] << 24); V = (I >> 8) / 8388608.f;
			}
			else if (Bits == 32)
			{
				V = int32(ReadU32(S)) / 2147483648.f;
			}
			else if (Bits == 8)
			{
				V = (int32(S[0]) - 128) / 128.f;
			}
			if (Channels == 1) { Acc[0] = V; Acc[1] = V; }
			else if (C < 2) { Acc[C] = V; }
		}
		Src[F * 2] = Acc[0];
		Src[F * 2 + 1] = Acc[1];
	}

	// Remuestreo lineal a la frecuencia de salida
	if ((int32)Rate == TargetSampleRate || TargetSampleRate <= 0)
	{
		Out.Stereo = MoveTemp(Src);
		Out.SampleRate = Rate;
	}
	else
	{
		const double Ratio = double(Rate) / double(TargetSampleRate);
		const int32 OutFrames = int32(Frames / Ratio);
		Out.Stereo.SetNumUninitialized(OutFrames * 2);
		for (int32 F = 0; F < OutFrames; ++F)
		{
			const double SrcPos = F * Ratio;
			const int32 I0 = FMath::Min(int32(SrcPos), Frames - 1);
			const int32 I1 = FMath::Min(I0 + 1, Frames - 1);
			const float T = float(SrcPos - I0);
			Out.Stereo[F * 2] = FMath::Lerp(Src[I0 * 2], Src[I1 * 2], T);
			Out.Stereo[F * 2 + 1] = FMath::Lerp(Src[I0 * 2 + 1], Src[I1 * 2 + 1], T);
		}
		Out.SampleRate = TargetSampleRate;
	}
	Out.Seconds = float(Out.Stereo.Num() / 2) / float(Out.SampleRate);
	return true;
}
