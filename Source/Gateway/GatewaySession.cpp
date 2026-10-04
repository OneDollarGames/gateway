#include "GatewaySession.h"
#include "Gateway.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString UGatewaySessionLibrary::ContentDir() { return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Gateway")); }
FString UGatewaySessionLibrary::SessionsDir() { return FPaths::Combine(ContentDir(), TEXT("Sessions")); }
FString UGatewaySessionLibrary::VoiceDir() { return FPaths::Combine(ContentDir(), TEXT("Voice")); }
FString UGatewaySessionLibrary::LogPath() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("gateway_log.txt")); }

namespace
{
	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Def)
	{
		double V; return (O.IsValid() && O->TryGetNumberField(FString(Key), V)) ? float(V) : Def;
	}
	bool Bool(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Def)
	{
		bool V; return (O.IsValid() && O->TryGetBoolField(FString(Key), V)) ? V : Def;
	}
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Def = FString())
	{
		FString V; return (O.IsValid() && O->TryGetStringField(FString(Key), V)) ? V : Def;
	}
	FLinearColor Color(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FLinearColor& Def)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O.IsValid() && O->TryGetArrayField(FString(Key), Arr) && Arr->Num() >= 3)
		{
			return FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), Arr->Num() > 3 ? (*Arr)[3]->AsNumber() : 1.f);
		}
		return Def;
	}
	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* Sub = nullptr;
		return (O.IsValid() && O->TryGetObjectField(FString(Key), Sub)) ? *Sub : nullptr;
	}

	EGatewayVisualMode ParseMode(const FString& S)
	{
		const FString L = S.ToLower();
		if (L == TEXT("ganzfeld")) return EGatewayVisualMode::Ganzfeld;
		if (L == TEXT("orbe") || L == TEXT("orb") || L == TEXT("rebal")) return EGatewayVisualMode::Orb;
		if (L == TEXT("tunel") || L == TEXT("tunnel")) return EGatewayVisualMode::Tunnel;
		if (L == TEXT("mandala")) return EGatewayVisualMode::Mandala;
		if (L == TEXT("cosmos")) return EGatewayVisualMode::Cosmos;
		if (L == TEXT("vacio") || L == TEXT("void")) return EGatewayVisualMode::Void;
		if (L == TEXT("caja") || L == TEXT("box")) return EGatewayVisualMode::Box;
		return EGatewayVisualMode::Ganzfeld;
	}
}

bool UGatewaySessionLibrary::LoadSession(const FString& JsonPath, FGatewaySessionDef& Out, FString* Error)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath))
	{
		if (Error) *Error = FString::Printf(TEXT("No se pudo leer %s"), *JsonPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		if (Error) *Error = FString::Printf(TEXT("JSON invalido: %s"), *JsonPath);
		return false;
	}

	Out = FGatewaySessionDef();
	Out.FilePath = JsonPath;
	Out.Id = Str(Root, TEXT("id"), FPaths::GetBaseFilename(JsonPath));
	Out.Title = Str(Root, TEXT("titulo"), Out.Id);
	Out.Wave = Str(Root, TEXT("onda"));
	Out.Order = int32(Num(Root, TEXT("orden"), 0));
	Out.Description = Str(Root, TEXT("descripcion"));
	Out.Requires = Str(Root, TEXT("requiere"));
	Out.bSleep = Bool(Root, TEXT("dormir"), false);
	Out.bNight = Bool(Root, TEXT("nocturna"), false);

	const TArray<TSharedPtr<FJsonValue>>* Segs = nullptr;
	if (Root->TryGetArrayField(TEXT("segmentos"), Segs))
	{
		float Cursor = 0.f;
		for (const TSharedPtr<FJsonValue>& V : *Segs)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			if (!S.IsValid()) continue;
			FGatewaySegment Seg;
			Seg.Name = Str(S, TEXT("nombre"));
			Seg.StartTime = Num(S, TEXT("inicio"), Cursor);
			Seg.Duration = Num(S, TEXT("duracion"), 60.f);
			Seg.Voice = Str(S, TEXT("voz"));
			Seg.VoiceGain = Num(S, TEXT("voz_gain"), 1.f);
			Seg.Caption = Str(S, TEXT("texto"));
			Seg.RampSeconds = Num(S, TEXT("rampa"), 12.f);
			Seg.bChime = Bool(S, TEXT("campana"), false);
			Seg.ChimeHz = Num(S, TEXT("campana_hz"), 528.f);
			Seg.ChimeGain = Num(S, TEXT("campana_gain"), 0.35f);

			if (TSharedPtr<FJsonObject> Snd = Obj(S, TEXT("sonido")))
			{
				const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
				if (Snd->TryGetArrayField(TEXT("capas"), Layers))
				{
					for (const TSharedPtr<FJsonValue>& LV : *Layers)
					{
						const TSharedPtr<FJsonObject> L = LV->AsObject();
						if (!L.IsValid()) continue;
						FGatewayToneLayer Layer;
						Layer.CarrierHz = Num(L, TEXT("portadora"), 100.f);
						Layer.BeatHz = Num(L, TEXT("batido"), 4.f);
						Layer.Gain = Num(L, TEXT("gain"), 0.2f);
						Layer.IsoDepth = Num(L, TEXT("iso"), 0.f);
						Layer.Pan = Num(L, TEXT("pan"), 0.f);
						Seg.Sound.Layers.Add(Layer);
					}
				}
				Seg.Sound.PinkNoise = Num(Snd, TEXT("rosa"), 0.f);
				Seg.Sound.BrownNoise = Num(Snd, TEXT("marron"), 0.f);
				Seg.Sound.OceanRate = Num(Snd, TEXT("oceano"), 0.f);
				Seg.Sound.BreathToneGain = Num(Snd, TEXT("tono_respiracion"), 0.f);
				Seg.Sound.Master = Num(Snd, TEXT("master"), 1.f);
			}
			if (TSharedPtr<FJsonObject> Vis = Obj(S, TEXT("visual")))
			{
				Seg.Visual.Mode = ParseMode(Str(Vis, TEXT("modo"), TEXT("ganzfeld")));
				Seg.Visual.Color = Color(Vis, TEXT("color"), Seg.Visual.Color);
				Seg.Visual.Intensity = Num(Vis, TEXT("intensidad"), 0.5f);
				Seg.Visual.Speed = Num(Vis, TEXT("velocidad"), 0.3f);
				Seg.Visual.Complexity = Num(Vis, TEXT("complejidad"), 0.5f);
				Seg.Visual.HueDrift = Num(Vis, TEXT("matiz"), 0.f);
				Seg.Visual.Image = Str(Vis, TEXT("imagen"));
			}
			if (TSharedPtr<FJsonObject> Fl = Obj(S, TEXT("flicker")))
			{
				Seg.Flicker.Hz = Num(Fl, TEXT("hz"), 0.f);
				Seg.Flicker.Depth = Num(Fl, TEXT("profundidad"), 0.f);
				Seg.Flicker.Shape = Num(Fl, TEXT("forma"), 0.f);
				Seg.Flicker.Color = Color(Fl, TEXT("color"), FLinearColor::White);
				if (Seg.Flicker.Hz > 0.f && Seg.Flicker.Depth > 0.f) { Out.bUsesFlicker = true; }
			}
			if (TSharedPtr<FJsonObject> Br = Obj(S, TEXT("respiracion")))
			{
				Seg.Breath.bActive = Bool(Br, TEXT("activa"), true);
				Seg.Breath.Rpm = Num(Br, TEXT("rpm"), 6.f);
				Seg.Breath.InhaleRatio = Num(Br, TEXT("inhalar"), 0.4f);
				Seg.Breath.bShowGuide = Bool(Br, TEXT("guia"), true);
			}
			Cursor = Seg.StartTime + Seg.Duration;
			Out.Segments.Add(Seg);
		}
		Out.TotalSeconds = Cursor;
	}
	Out.TotalSeconds = Num(Root, TEXT("duracion_total"), Out.TotalSeconds);
	return Out.Segments.Num() > 0;
}

TArray<FGatewaySessionDef> UGatewaySessionLibrary::LoadAllSessions()
{
	TArray<FGatewaySessionDef> Result;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(SessionsDir(), TEXT("*.json")), true, false);
	for (const FString& F : Files)
	{
		FGatewaySessionDef Def; FString Err;
		if (LoadSession(FPaths::Combine(SessionsDir(), F), Def, &Err)) { Result.Add(Def); }
		else { UE_LOG(LogGateway, Warning, TEXT("Sesion %s: %s"), *F, *Err); }
	}
	Result.Sort([](const FGatewaySessionDef& A, const FGatewaySessionDef& B) { return A.Order < B.Order; });
	return Result;
}

void UGatewaySessionLibrary::AppendLog(const FString& SessionId, float Seconds, bool bCompleted)
{
	const FString Line = FString::Printf(TEXT("%s;%s;%d;%s\n"), *SessionId, *FDateTime::Now().ToString(), int32(Seconds), bCompleted ? TEXT("completa") : TEXT("parcial"));
	FFileHelper::SaveStringToFile(Line, *LogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

TSet<FString> UGatewaySessionLibrary::CompletedSessionIds()
{
	TSet<FString> Ids;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *LogPath()))
	{
		for (const FString& L : Lines)
		{
			TArray<FString> Parts; L.ParseIntoArray(Parts, TEXT(";"));
			if (Parts.Num() >= 4 && Parts[3] == TEXT("completa")) { Ids.Add(Parts[0]); }
		}
	}
	return Ids;
}
