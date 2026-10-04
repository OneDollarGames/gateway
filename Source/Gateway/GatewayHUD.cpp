#include "GatewayHUD.h"
#include "Gateway.h"
#include "GatewayDirector.h"
#include "GatewayAudioDevices.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor ColText(0.92f, 0.92f, 0.98f, 1.f);
	const FLinearColor ColDim(0.62f, 0.64f, 0.75f, 1.f);
	const FLinearColor ColAccent(0.55f, 0.75f, 1.f, 1.f);
	const FLinearColor ColGold(1.f, 0.85f, 0.5f, 1.f);
	const FLinearColor ColOk(0.55f, 0.95f, 0.7f, 1.f);
	const FLinearColor ColWarn(1.f, 0.55f, 0.45f, 1.f);
}

AGatewayHUD::AGatewayHUD()
{
	FontBig = GEngine ? GEngine->GetLargeFont() : nullptr;
	FontMed = GEngine ? GEngine->GetMediumFont() : nullptr;
	FontSmall = GEngine ? GEngine->GetSmallFont() : nullptr;
}

FString AGatewayHUD::Clock(float Seconds)
{
	const int32 S = FMath::Max(0, int32(Seconds));
	if (S >= 3600) return FString::Printf(TEXT("%d:%02d:%02d"), S / 3600, (S / 60) % 60, S % 60);
	return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
}

void AGatewayHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, Alpha), X, Y, W, H);
	DrawRect(FLinearColor(0.6f, 0.7f, 1.f, Alpha * 0.35f), X, Y, W, 1.f);
	DrawRect(FLinearColor(0.6f, 0.7f, 1.f, Alpha * 0.35f), X, Y + H - 1.f, W, 1.f);
}

float AGatewayHUD::Text(const FString& S, float X, float Y, const FLinearColor& C, UFont* F, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(S, W, H, F, Scale);
	DrawText(S, C, X, Y, F, Scale, false);
	return H;
}

float AGatewayHUD::TextCentered(const FString& S, float CX, float Y, const FLinearColor& C, UFont* F, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(S, W, H, F, Scale);
	DrawText(S, C, CX - W * 0.5f, Y, F, Scale, false);
	return H;
}

float AGatewayHUD::Wrapped(const FString& S, float X, float Y, float MaxW, const FLinearColor& C, UFont* F, float Scale, float LineGap)
{
	TArray<FString> Paragraphs;
	S.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	float CY = Y;
	for (const FString& Para : Paragraphs)
	{
		TArray<FString> Words;
		Para.ParseIntoArray(Words, TEXT(" "), true);
		FString Line;
		float LineH = 0.f;
		for (const FString& Wd : Words)
		{
			const FString Test = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
			float W = 0.f, H = 0.f;
			GetTextSize(Test, W, H, F, Scale);
			LineH = H;
			if (W > MaxW && !Line.IsEmpty())
			{
				DrawText(Line, C, X, CY, F, Scale, false);
				CY += H + LineGap;
				Line = Wd;
			}
			else { Line = Test; }
		}
		if (!Line.IsEmpty() || Para.IsEmpty())
		{
			if (!Line.IsEmpty()) { DrawText(Line, C, X, CY, F, Scale, false); }
			float W = 0.f, H = LineH; if (Line.IsEmpty()) { GetTextSize(TEXT("X"), W, H, F, Scale); }
			CY += H + LineGap;
		}
	}
	return CY - Y;
}

void AGatewayHUD::Circle(float CX, float CY, float R, const FLinearColor& C, float Thickness, int32 Segs)
{
	float PX = CX + R, PY = CY;
	for (int32 i = 1; i <= Segs; ++i)
	{
		const float A = 2.f * PI * i / Segs;
		const float X = CX + R * FMath::Cos(A), Y = CY + R * FMath::Sin(A);
		DrawLine(PX, PY, X, Y, C, Thickness);
		PX = X; PY = Y;
	}
}

void AGatewayHUD::Button(const FString& Label, float X, float Y, float W, float H, FName Id, bool bSelected)
{
	const bool bHover = (Hovered == Id);
	DrawRect(FLinearColor(0.3f, 0.4f, 0.8f, bSelected ? 0.55f : (bHover ? 0.35f : 0.18f)), X, Y, W, H);
	float TW = 0.f, TH = 0.f; GetTextSize(Label, TW, TH, FontMed, 1.f);
	DrawText(Label, bSelected ? ColText : ColDim, X + (W - TW) * 0.5f, Y + (H - TH) * 0.5f, FontMed, 1.f, false);
	AddHitBox(FVector2D(X, Y), FVector2D(W, H), Id, true, 0);
}

void AGatewayHUD::NotifyHitBoxClick(FName BoxName)
{
	AGatewayDirector* D = AGatewayDirector::Get(GetWorld());
	if (!D) return;
	const FString N = BoxName.ToString();
	if (N.StartsWith(TEXT("ses_")))
	{
		const int32 Idx = FCString::Atoi(*N.Mid(4));
		if (D->GetState() == EGatewayState::Menu)
		{
			if (Idx == D->GetMenuIndex()) { D->MenuConfirm(); }
			else { D->MenuMove(Idx - D->GetMenuIndex()); }
		}
	}
	else if (N.StartsWith(TEXT("dev_")))
	{
		const int32 Idx = FCString::Atoi(*N.Mid(4));
		D->MenuMove(Idx - D->GetDeviceIndex());
		D->MenuConfirm();
	}
	else if (N.StartsWith(TEXT("set_")))
	{
		const int32 Idx = FCString::Atoi(*N.Mid(4));
		D->MenuMove(Idx - D->GetSettingsIndex());
	}
	else if (N == TEXT("btn_start")) D->MenuConfirm();
	else if (N == TEXT("btn_settings")) D->OpenSettings();
	else if (N == TEXT("btn_devices")) D->OpenDevices();
	else if (N == TEXT("btn_back")) D->MenuBack();
	else if (N == TEXT("btn_accept")) D->AcceptWarning();
	else if (N == TEXT("btn_minus")) D->MenuAdjust(-1);
	else if (N == TEXT("btn_plus")) D->MenuAdjust(1);
	else if (N == TEXT("btn_help")) { D->bShowHelp = !D->bShowHelp; }
}

void AGatewayHUD::NotifyHitBoxBeginCursorOver(FName BoxName) { Hovered = BoxName; }
void AGatewayHUD::NotifyHitBoxEndCursorOver(FName BoxName) { if (Hovered == BoxName) Hovered = NAME_None; }

void AGatewayHUD::DrawHUD()
{
	Super::DrawHUD();
	AGatewayDirector* D = AGatewayDirector::Get(GetWorld());
	if (!D || !Canvas) return;
	if (D->bVR) return; // en el visor el menu es el panel 3D; el canvas se proyectaria encima
	switch (D->GetState())
	{
	case EGatewayState::Menu: DrawMenu(D); break;
	case EGatewayState::Warning: DrawWarning(D); break;
	case EGatewayState::Settings: DrawSettings(D); break;
	case EGatewayState::Devices: DrawDevices(D); break;
	case EGatewayState::Running: DrawRunning(D); break;
	case EGatewayState::Finished: DrawFinished(D); break;
	}
}

void AGatewayHUD::DrawMenu(AGatewayDirector* D)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float M = FMath::Max(24.f, W * 0.04f);

	TextCentered(TEXT("G A T E W A Y"), W * 0.5f, M, ColText, FontBig, 2.6f);
	TextCentered(TEXT("Luz, sonido y conciencia  ·  Hemi-Sync binaural · Ganzfeld · Flicker · Sueño lúcido"), W * 0.5f, M + 70.f, ColDim, FontMed, 1.f);

	const TArray<FGatewaySessionDef>& S = D->GetSessions();
	const float ListX = M, ListY = M + 120.f, ListW = W * 0.52f, ListH = H - ListY - M - 60.f;
	Panel(ListX, ListY, ListW, ListH);

	if (S.Num() == 0)
	{
		Wrapped(TEXT("No hay sesiones en Content/Gateway/Sessions. Genera las sesiones con Tools/sesiones.py."), ListX + 20.f, ListY + 20.f, ListW - 40.f, ColWarn, FontMed);
	}
	float Y = ListY + 16.f;
	FString LastWave;
	const int32 Sel = D->GetMenuIndex();
	// scroll simple: mantener la seleccion visible
	const float RowH = 34.f;
	int32 First = 0;
	{
		int32 Rows = int32((ListH - 32.f) / RowH) - 2;
		if (Sel >= Rows) { First = Sel - Rows + 1; }
	}
	for (int32 i = First; i < S.Num(); ++i)
	{
		const FGatewaySessionDef& Def = S[i];
		if (Def.Wave != LastWave)
		{
			LastWave = Def.Wave;
			if (Y + 22.f > ListY + ListH - 20.f) break;
			Text(Def.Wave, ListX + 20.f, Y, ColAccent, FontSmall, 1.f);
			Y += 22.f;
		}
		if (Y + RowH > ListY + ListH - 10.f) break;
		const bool bSel = (i == Sel);
		const bool bDone = D->GetCompleted().Contains(Def.Id);
		const bool bReady = D->IsSessionRecommended(Def);
		const FName Id(*FString::Printf(TEXT("ses_%d"), i));
		if (bSel || Hovered == Id) { DrawRect(FLinearColor(0.35f, 0.45f, 0.9f, bSel ? 0.35f : 0.15f), ListX + 8.f, Y - 4.f, ListW - 16.f, RowH); }
		AddHitBox(FVector2D(ListX + 8.f, Y - 4.f), FVector2D(ListW - 16.f, RowH), Id, true, 0);
		const FString Mark = bDone ? TEXT("*") : (bReady ? TEXT("o") : TEXT("-"));
		Text(Mark, ListX + 24.f, Y, bDone ? ColOk : (bReady ? ColDim : FLinearColor(0.4f, 0.4f, 0.5f)), FontMed, 1.f);
		Text(FString::Printf(TEXT("%2d.  %s"), Def.Order, *Def.Title), ListX + 52.f, Y, bSel ? ColText : (bReady ? ColText : ColDim), FontMed, 1.f);
		const FString Dur = Clock(Def.TotalSeconds) + (Def.bNight ? TEXT("  noche") : (Def.bSleep ? TEXT("  dormir") : TEXT("")));
		float DW, DH; GetTextSize(Dur, DW, DH, FontSmall, 1.f);
		Text(Dur, ListX + ListW - 24.f - DW, Y + 3.f, ColDim, FontSmall, 1.f);
		Y += RowH;
	}

	// Panel derecho: detalle
	const float DX = ListX + ListW + M * 0.5f, DW2 = W - DX - M;
	Panel(DX, ListY, DW2, ListH);
	if (S.IsValidIndex(Sel))
	{
		const FGatewaySessionDef& Def = S[Sel];
		float DY = ListY + 20.f;
		DY += Wrapped(Def.Title, DX + 20.f, DY, DW2 - 40.f, ColGold, FontBig, 1.3f) + 6.f;
		DY += Text(FString::Printf(TEXT("%s   ·   %s"), *Def.Wave, *Clock(Def.TotalSeconds)), DX + 20.f, DY, ColAccent, FontSmall) + 14.f;
		DY += Wrapped(Def.Description, DX + 20.f, DY, DW2 - 40.f, ColText, FontMed, 1.f, 5.f) + 14.f;
		if (!D->IsSessionRecommended(Def))
		{
			DY += Wrapped(FString::Printf(TEXT("Recomendado antes: completar \"%s\". Puedes empezar igual."), *Def.Requires), DX + 20.f, DY, DW2 - 40.f, ColWarn, FontSmall) + 8.f;
		}
		if (Def.bUsesFlicker)
		{
			DY += Wrapped(D->GetSettings().FlickerScale > 0.f ? TEXT("Incluye luz intermitente (flicker). Ojos cerrados. Intensidad en Ajustes.") : TEXT("Esta sesión incluye flicker pero está apagado en Ajustes."), DX + 20.f, DY, DW2 - 40.f, ColWarn, FontSmall) + 8.f;
		}
		if (Def.bSleep) { DY += Wrapped(TEXT("Termina en silencio y oscuridad: diseñada para quedarte dormido."), DX + 20.f, DY, DW2 - 40.f, ColDim, FontSmall) + 8.f; }
		if (Def.bNight) { DY += Wrapped(TEXT("Protocolo nocturno: deja la computadora encendida con los audífonos puestos; la pantalla se apaga sola."), DX + 20.f, DY, DW2 - 40.f, ColDim, FontSmall) + 8.f; }

		Button(TEXT("COMENZAR  (Enter)"), DX + 20.f, ListY + ListH - 64.f, DW2 - 40.f, 44.f, TEXT("btn_start"), true);
	}

	// Barra inferior
	const float BY = H - M - 44.f;
	Button(TEXT("Ajustes (O)"), M, BY, 170.f, 36.f, TEXT("btn_settings"), false);
	Button(TEXT("Salida de audio (D)"), M + 180.f, BY, 210.f, 36.f, TEXT("btn_devices"), false);
	Button(TEXT("Ayuda (H)"), M + 400.f, BY, 130.f, 36.f, TEXT("btn_help"), false);
	const FString Status = D->StatusLine.IsEmpty() ? (D->GetDevices() ? FString::Printf(TEXT("Salida: %s"), *D->GetDevices()->CurrentName()) : TEXT("")) : D->StatusLine;
	float SW, SH; GetTextSize(Status, SW, SH, FontSmall, 1.f);
	Text(Status, W - M - SW, BY + 10.f, ColDim, FontSmall);

	if (D->bShowHelp)
	{
		const float HW = FMath::Min(760.f, W - 2 * M), HX = (W - HW) * 0.5f, HY = H * 0.2f;
		Panel(HX, HY, HW, 330.f, 0.9f);
		float Y2 = HY + 18.f;
		Y2 += Text(TEXT("Cómo usar Gateway"), HX + 24.f, Y2, ColGold, FontBig, 1.1f) + 10.f;
		Y2 += Wrapped(TEXT("1. Ponte los AirPods (perfil estéreo, no \"manos libres\"). Comprueba en \"Salida de audio\" que el programa salga por ellos.\n2. Recuéstate en un lugar oscuro y tranquilo, sin interrupciones, una hora después de comer.\n3. Sigue el programa en orden: cada sesión se apoya en la anterior. Repite una sesión los días que quieras antes de avanzar.\n4. Durante la sesión: Espacio = pausa, Esc = terminar, flecha derecha = saltar segmento, F = pantalla completa, H = esta ayuda.\n5. El flicker (luz intermitente) va con los ojos cerrados. Si tienes epilepsia, migraña con aura o antecedentes, déjalo en 0 en Ajustes."), HX + 24.f, Y2, HW - 48.f, ColText, FontMed, 1.f, 6.f);
	}
}

void AGatewayHUD::DrawWarning(AGatewayDirector* D)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float PW = FMath::Min(900.f, W * 0.8f), PX = (W - PW) * 0.5f, PY = H * 0.12f;
	Panel(PX, PY, PW, H * 0.76f, 0.9f);
	float Y = PY + 24.f;
	Y += TextCentered(TEXT("Antes de continuar: luz intermitente"), W * 0.5f, Y, ColGold, FontBig, 1.4f) + 16.f;
	Y += Wrapped(TEXT("Esta sesión incluye estimulación lumínica intermitente (flicker) de 4 a 12 Hz, la misma que usan los estudios de inducción de imaginería (Bartossek 2021, Amaya 2023, Reeder 2022). Es segura para la mayoría, pero puede provocar crisis en personas con epilepsia fotosensible.\n\nNO la uses si:\n• tienes epilepsia o has tenido convulsiones, o un familiar directo las tiene;\n• tienes migraña con aura, psicosis o estás embarazada;\n• tomas psicofármacos sin consultarlo con tu médico;\n• eres menor de edad.\n\nDurante el flicker mantén los OJOS CERRADOS: la luz atraviesa los párpados y así es como se usa. Si notas malestar, mareo o náusea, pulsa Esc: la luz se apaga de inmediato.\n\nPuedes dejar la intensidad del flicker en 0 en Ajustes y hacer todas las sesiones sin él.\n\nAl aceptar confirmas que has leído esto y que no tienes ninguna contraindicación."), PX + 30.f, Y, PW - 60.f, ColText, FontMed, 1.f, 5.f) + 10.f;
	Button(TEXT("ACEPTO Y CONTINÚO  (Enter)"), PX + 30.f, PY + H * 0.76f - 64.f, PW * 0.5f - 40.f, 44.f, TEXT("btn_accept"), true);
	Button(TEXT("VOLVER  (Esc)"), PX + PW * 0.5f + 10.f, PY + H * 0.76f - 64.f, PW * 0.5f - 40.f, 44.f, TEXT("btn_back"), false);
}

void AGatewayHUD::DrawSettings(AGatewayDirector* D)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float PW = FMath::Min(820.f, W * 0.8f), PX = (W - PW) * 0.5f, PY = H * 0.15f;
	Panel(PX, PY, PW, H * 0.7f, 0.9f);
	float Y = PY + 24.f;
	Y += TextCentered(TEXT("Ajustes"), W * 0.5f, Y, ColGold, FontBig, 1.4f) + 24.f;
	const FGatewaySettings& S = D->GetSettings();
	struct FRow { FString Name; FString Value; float Bar; };
	const FRow Rows[6] = {
		{ TEXT("Intensidad del flicker (luz intermitente)"), FString::Printf(TEXT("%d %%"), int32(S.FlickerScale * 100.f + 0.5f)), S.FlickerScale },
		{ TEXT("Volumen de tonos Hemi-Sync"), FString::Printf(TEXT("%d %%"), int32(S.VolTones * 100.f + 0.5f)), S.VolTones },
		{ TEXT("Volumen de la voz guía"), FString::Printf(TEXT("%d %%"), int32(S.VolVoices * 100.f + 0.5f)), S.VolVoices / 1.5f },
		{ TEXT("Volumen de ruido / ambiente"), FString::Printf(TEXT("%d %%"), int32(S.VolNoise * 100.f + 0.5f)), S.VolNoise },
		{ TEXT("Pantalla completa"), S.bFullscreen ? TEXT("sí") : TEXT("no"), -1.f },
		{ TEXT("Salida de audio"), D->GetDevices() ? D->GetDevices()->CurrentName() : TEXT(""), -1.f },
	};
	const int32 Sel = D->GetSettingsIndex();
	for (int32 i = 0; i < 6; ++i)
	{
		const bool bSel = (i == Sel);
		const FName Id(*FString::Printf(TEXT("set_%d"), i));
		if (bSel) { DrawRect(FLinearColor(0.35f, 0.45f, 0.9f, 0.3f), PX + 16.f, Y - 6.f, PW - 32.f, 54.f); }
		AddHitBox(FVector2D(PX + 16.f, Y - 6.f), FVector2D(PW - 32.f, 54.f), Id, true, 0);
		Text(Rows[i].Name, PX + 30.f, Y, bSel ? ColText : ColDim, FontMed);
		float VW, VH; GetTextSize(Rows[i].Value, VW, VH, FontMed, 1.f);
		Text(Rows[i].Value, PX + PW - 30.f - VW, Y, bSel ? ColGold : ColDim, FontMed);
		if (Rows[i].Bar >= 0.f)
		{
			DrawRect(FLinearColor(1, 1, 1, 0.12f), PX + 30.f, Y + 28.f, PW - 60.f, 6.f);
			DrawRect(bSel ? ColAccent : ColDim, PX + 30.f, Y + 28.f, (PW - 60.f) * FMath::Clamp(Rows[i].Bar, 0.f, 1.f), 6.f);
		}
		Y += 56.f;
	}
	Y += 10.f;
	Wrapped(TEXT("Flecha izquierda/derecha o los botones ajustan el valor. Enter en \"Salida de audio\" abre la lista de dispositivos. Esc vuelve al menú.\nEl flicker al 100 % equivale al protocolo de laboratorio (10 Hz, pantalla completa). 30-50 % es suficiente para ver geometrías con los ojos cerrados."), PX + 30.f, Y, PW - 60.f, ColDim, FontSmall, 1.f, 4.f);
	Button(TEXT("-"), PX + 30.f, PY + H * 0.7f - 64.f, 60.f, 44.f, TEXT("btn_minus"), false);
	Button(TEXT("+"), PX + 100.f, PY + H * 0.7f - 64.f, 60.f, 44.f, TEXT("btn_plus"), false);
	Button(TEXT("VOLVER  (Esc)"), PX + PW - 230.f, PY + H * 0.7f - 64.f, 200.f, 44.f, TEXT("btn_back"), false);
}

void AGatewayHUD::DrawDevices(AGatewayDirector* D)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float PW = FMath::Min(900.f, W * 0.85f), PX = (W - PW) * 0.5f, PY = H * 0.12f;
	Panel(PX, PY, PW, H * 0.76f, 0.9f);
	float Y = PY + 24.f;
	Y += TextCentered(TEXT("Salida de audio"), W * 0.5f, Y, ColGold, FontBig, 1.4f) + 10.f;
	Y += Wrapped(TEXT("Elige los AirPods en su perfil ESTÉREO (\"Headphones\" / \"Auriculares\" / \"Stereo\"). El perfil \"Hands-Free\" / \"Headset\" es mono y anula el efecto binaural. El programa intenta elegirlo solo al arrancar."), PX + 30.f, Y, PW - 60.f, ColDim, FontSmall, 1.f, 4.f) + 14.f;
	UGatewayAudioDevices* Dev = D->GetDevices();
	if (!Dev || Dev->GetDevices().Num() == 0)
	{
		Text(Dev && Dev->IsRefreshing() ? TEXT("Buscando dispositivos...") : TEXT("No se encontraron dispositivos de salida."), PX + 30.f, Y, ColWarn, FontMed);
	}
	else
	{
		const int32 Sel = D->GetDeviceIndex();
		for (int32 i = 0; i < Dev->GetDevices().Num(); ++i)
		{
			const FAudioOutputDeviceInfo& Info = Dev->GetDevices()[i];
			if (Y + 40.f > PY + H * 0.76f - 70.f) break;
			const bool bSel = (i == Sel);
			const FName Id(*FString::Printf(TEXT("dev_%d"), i));
			if (bSel) { DrawRect(FLinearColor(0.35f, 0.45f, 0.9f, 0.3f), PX + 16.f, Y - 6.f, PW - 32.f, 40.f); }
			AddHitBox(FVector2D(PX + 16.f, Y - 6.f), FVector2D(PW - 32.f, 40.f), Id, true, 0);
			const int32 Score = UGatewayAudioDevices::ScoreDevice(Info);
			const FString Mark = Info.bIsCurrentDevice ? TEXT(">") : TEXT(" ");
			Text(Mark, PX + 30.f, Y, ColOk, FontMed);
			Text(Info.Name, PX + 56.f, Y, bSel ? ColText : ColDim, FontMed);
			const FString Extra = FString::Printf(TEXT("%d ch · %d Hz%s%s"), Info.NumChannels, Info.SampleRate, Info.bIsSystemDefault ? TEXT(" · predeterminado") : TEXT(""), Score >= 100 ? TEXT(" · recomendado") : (Score < 0 ? TEXT(" · evitar (mono)") : TEXT("")));
			float EW, EH; GetTextSize(Extra, EW, EH, FontSmall, 1.f);
			Text(Extra, PX + PW - 30.f - EW, Y + 3.f, Score >= 100 ? ColOk : (Score < 0 ? ColWarn : ColDim), FontSmall);
			Y += 40.f;
		}
	}
	Button(TEXT("USAR ESTA SALIDA  (Enter)"), PX + 30.f, PY + H * 0.76f - 64.f, PW * 0.5f - 40.f, 44.f, TEXT("btn_start"), true);
	Button(TEXT("VOLVER  (Esc)"), PX + PW * 0.5f + 10.f, PY + H * 0.76f - 64.f, PW * 0.5f - 40.f, 44.f, TEXT("btn_back"), false);
}

void AGatewayHUD::DrawBreathGuide(AGatewayDirector* D, float Alpha)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float P = D->GetBreathPhase();
	const float Env = (P < 0.5f) ? P * 2.f : 1.f - (P - 0.5f) * 2.f;
	const float R = FMath::Min(W, H) * (0.07f + 0.09f * Env);
	const FLinearColor C(0.8f, 0.9f, 1.f, 0.55f * Alpha);
	Circle(W * 0.5f, H * 0.5f, R, C, 2.f);
	Circle(W * 0.5f, H * 0.5f, FMath::Min(W, H) * 0.17f, FLinearColor(0.8f, 0.9f, 1.f, 0.12f * Alpha), 1.f);
	TextCentered(P < 0.5f ? TEXT("inhala") : TEXT("exhala"), W * 0.5f, H * 0.5f + FMath::Min(W, H) * 0.19f, FLinearColor(1, 1, 1, 0.6f * Alpha), FontMed);
}

void AGatewayHUD::DrawRunning(AGatewayDirector* D)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const FGatewaySessionDef* S = D->CurrentSession();
	const FGatewaySegment* Seg = D->CurrentSegment();
	if (!S) return;

	// Fundido del overlay de textos
	const float Dt = GetWorld()->GetDeltaSeconds();
	const float Target = (D->bShowOverlay || D->IsPaused()) ? 1.f : 0.f;
	OverlayAlpha = FMath::FInterpTo(OverlayAlpha, Target, Dt, 2.f);

	if (D->IsBreathGuideVisible())
	{
		DrawBreathGuide(D, S->bNight ? 0.25f : 1.f);
	}

	if (OverlayAlpha > 0.01f)
	{
		const float A = OverlayAlpha * (S->bNight ? 0.4f : 1.f);
		const float M = 28.f;
		Text(S->Title, M, M, FLinearColor(ColGold.R, ColGold.G, ColGold.B, A), FontBig, 1.1f);
		if (Seg) { Text(Seg->Name, M, M + 34.f, FLinearColor(ColDim.R, ColDim.G, ColDim.B, A), FontMed); }
		const FString T = Clock(D->GetElapsed()) + TEXT(" / ") + Clock(S->TotalSeconds);
		float TW, TH; GetTextSize(T, TW, TH, FontMed, 1.f);
		Text(T, W - M - TW, M, FLinearColor(ColText.R, ColText.G, ColText.B, A), FontMed);
		// barra de progreso
		DrawRect(FLinearColor(1, 1, 1, 0.08f * A), M, H - M - 4.f, W - 2 * M, 3.f);
		DrawRect(FLinearColor(ColAccent.R, ColAccent.G, ColAccent.B, 0.6f * A), M, H - M - 4.f, (W - 2 * M) * FMath::Clamp(D->GetElapsed() / FMath::Max(1.f, S->TotalSeconds), 0.f, 1.f), 3.f);
		// marcas de segmentos
		for (const FGatewaySegment& Sg : S->Segments)
		{
			const float X = M + (W - 2 * M) * FMath::Clamp(Sg.StartTime / FMath::Max(1.f, S->TotalSeconds), 0.f, 1.f);
			DrawRect(FLinearColor(1, 1, 1, 0.25f * A), X, H - M - 7.f, 1.f, 9.f);
		}
		if (Seg && !Seg->Caption.IsEmpty())
		{
			Wrapped(Seg->Caption, W * 0.2f, H - M - 70.f, W * 0.6f, FLinearColor(ColText.R, ColText.G, ColText.B, 0.85f * A), FontMed, 1.f, 4.f);
		}
		if (D->IsPaused())
		{
			TextCentered(TEXT("PAUSA  ·  Espacio para continuar  ·  Esc para terminar"), W * 0.5f, H * 0.5f - 12.f, FLinearColor(1, 1, 1, A), FontBig, 1.2f);
		}
		const FString Help = TEXT("Espacio pausa · Esc terminar · Der. saltar · H ayuda");
		float HW, HH; GetTextSize(Help, HW, HH, FontSmall, 1.f);
		Text(Help, W - M - HW, H - M - 30.f, FLinearColor(ColDim.R, ColDim.G, ColDim.B, 0.7f * A), FontSmall);
		if (!D->StatusLine.IsEmpty()) { Text(D->StatusLine, M, H - M - 30.f, FLinearColor(ColWarn.R, ColWarn.G, ColWarn.B, A), FontSmall); }
	}
}

void AGatewayHUD::DrawFinished(AGatewayDirector* D)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const FGatewaySessionDef* S = D->CurrentSession();
	if (S && (S->bSleep || S->bNight))
	{
		// Pantalla negra: solo un punto tenue de salida
		TextCentered(TEXT("Esc para salir"), W * 0.5f, H - 40.f, FLinearColor(1, 1, 1, 0.08f), FontSmall);
		return;
	}
	const float PW = FMath::Min(760.f, W * 0.8f), PX = (W - PW) * 0.5f, PY = H * 0.3f;
	Panel(PX, PY, PW, 260.f, 0.85f);
	float Y = PY + 28.f;
	Y += TextCentered(TEXT("Sesión completada"), W * 0.5f, Y, ColGold, FontBig, 1.5f) + 14.f;
	if (S) { Y += TextCentered(S->Title, W * 0.5f, Y, ColText, FontMed, 1.1f) + 10.f; }
	Y += TextCentered(FString::Printf(TEXT("Duración: %s"), *Clock(D->GetElapsed())), W * 0.5f, Y, ColDim, FontMed) + 20.f;
	Y += Wrapped(TEXT("Tómate un minuto antes de levantarte. Si quieres recordar lo vivido, anótalo ahora: la memoria de estos estados se desvanece rápido."), PX + 30.f, Y, PW - 60.f, ColDim, FontSmall, 1.f, 4.f);
	Button(TEXT("VOLVER AL MENÚ  (Enter)"), PX + PW * 0.25f, PY + 260.f - 60.f, PW * 0.5f, 44.f, TEXT("btn_start"), true);
}
