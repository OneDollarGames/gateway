#include "GatewayVRPanel.h"
#include "Gateway.h"
#include "GatewayDirector.h"
#include "GatewayAudioDevices.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, float Size, const FVector& Rel, EHorizTextAligment H, const FColor& Col)
	{
		UTextRenderComponent* T = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
		T->SetupAttachment(Parent);
		T->SetRelativeLocation(Rel);
		T->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // el texto mira hacia -X del panel, es decir hacia el usuario
		T->SetWorldSize(Size);
		T->SetHorizontalAlignment(H);
		T->SetVerticalAlignment(EVRTA_TextTop);
		T->SetTextRenderColor(Col);
		T->SetText(FText::GetEmpty());
		return T;
	}
}

AGatewayVRPanel::AGatewayVRPanel()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Backdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) { Backdrop->SetStaticMesh(Plane.Object); }
	// Plano de 100x100 en XY; lo giramos para que quede vertical frente al usuario (normal hacia -X)
	Backdrop->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Backdrop->SetRelativeScale3D(FVector(1.56f, 2.2f, 1.f)); // 220 cm ancho x 156 cm alto
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PanelMat(TEXT("/Game/Gateway/Materials/M_Panel.M_Panel"));
	if (PanelMat.Succeeded()) { Backdrop->SetMaterial(0, PanelMat.Object); }

	// Rectangulo de seleccion: mismo plano, medio centimetro delante del fondo y detras del texto
	Highlight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Highlight"));
	Highlight->SetupAttachment(Root);
	if (Plane.Succeeded()) { Highlight->SetStaticMesh(Plane.Object); }
	Highlight->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Highlight->SetRelativeLocation(FVector(-0.5f, 0.f, 0.f));
	Highlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Highlight->SetCastShadow(false);
	Highlight->SetVisibility(false);
	if (PanelMat.Succeeded()) { Highlight->SetMaterial(0, PanelMat.Object); }

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(-140.f, 0.f, 20.f));
	Light->SetIntensity(2500.f);
	Light->SetAttenuationRadius(450.f);
	Light->SetLightColor(FLinearColor(1.f, 1.f, 1.f));
	Light->SetCastShadows(false);
	Light->SetMobility(EComponentMobility::Movable);

	// Textos (x = -1 cm delante del panel; y = horizontal; z = vertical)
	Title = MakeText(this, Root, TEXT("Title"), 9.f, FVector(-1.f, 0.f, 70.f), EHTA_Center, FColor(255, 220, 140));
	Body = MakeText(this, Root, TEXT("Body"), 4.35f, FVector(-1.f, BodyY, BodyTop), EHTA_Left, FColor(235, 235, 250));
	Detail = MakeText(this, Root, TEXT("Detail"), 4.0f, FVector(-1.f, 8.f, BodyTop), EHTA_Left, FColor(200, 205, 230));
	Footer = MakeText(this, Root, TEXT("Footer"), 3.6f, FVector(-1.f, 0.f, -64.f), EHTA_Center, FColor(150, 160, 190));
	Overlay = MakeText(this, Root, TEXT("Overlay"), 4.5f, FVector(-1.f, 0.f, 95.f), EHTA_Center, FColor(220, 220, 240));
}

void AGatewayVRPanel::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* M = Highlight->GetMaterial(0))
	{
		HighlightMID = UMaterialInstanceDynamic::Create(M, this);
		HighlightMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.16f, 0.26f, 0.70f));
		HighlightMID->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
		Highlight->SetMaterial(0, HighlightMID);
	}
	Recenter();
}

// Coloca el rectangulo detras de la fila Row (0 = primera linea del cuerpo). Row < 0 lo oculta.
void AGatewayVRPanel::PlaceHighlight(int32 Row, int32 TotalLines, float Width)
{
	if (TotalLines > 0) { const float H = Body->GetTextLocalSize().Z / TotalLines; if (H > 0.f) { LineH = H; } }  // alto real de una linea
	if (Row < 0 || TotalLines <= 0 || LineH <= 0.f) { Highlight->SetVisibility(false); return; }
	const float Zc = BodyTop - (Row + 0.5f) * LineH;
	Highlight->SetRelativeLocation(FVector(-0.5f, BodyY + Width * 0.5f, Zc));
	Highlight->SetRelativeScale3D(FVector(LineH * 1.05f / 100.f, Width / 100.f, 1.f));
	Highlight->SetVisibility(PanelAlpha > 0.02f);
}

void AGatewayVRPanel::Recenter()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	FVector Loc; FRotator Rot; PC->GetPlayerViewPoint(Loc, Rot);
	FVector F = Rot.Vector();
	// Si el usuario esta acostado mirando al techo, el panel va donde mira; si no, al frente horizontal
	if (FMath::Abs(F.Z) < 0.6f) { F.Z = 0.f; F.Normalize(); }
	const FVector Pos = Loc + F * 200.f;
	// El panel mira hacia el usuario: su eje +X apunta en la direccion de la mirada (texto girado 180 en su propio espacio)
	const FRotator R = FRotationMatrix::MakeFromX(F).Rotator();
	SetActorLocationAndRotation(Pos, R);
	bNeedsRecenter = false;
}

FString AGatewayVRPanel::Wrap(const FString& S, int32 MaxChars)
{
	TArray<FString> Paras; S.ParseIntoArray(Paras, TEXT("\n"), false);
	FString Out;
	for (const FString& P : Paras)
	{
		TArray<FString> Words; P.ParseIntoArray(Words, TEXT(" "), true);
		FString Line;
		for (const FString& W : Words)
		{
			if (Line.Len() + W.Len() + 1 > MaxChars && !Line.IsEmpty()) { Out += Line + TEXT("\n"); Line = W; }
			else { Line = Line.IsEmpty() ? W : Line + TEXT(" ") + W; }
		}
		Out += (Line.IsEmpty() ? TEXT(" ") : Line) + TEXT("\n");  // una linea vacia se dibuja altisima en TextRender: va un espacio
	}
	return Out;
}

void AGatewayVRPanel::SetVisibleSmooth(bool bVisible, float Dt)
{
	PanelAlpha = FMath::FInterpTo(PanelAlpha, bVisible ? 1.f : 0.f, Dt, 3.f);
	const bool bShow = PanelAlpha > 0.02f;
	Backdrop->SetVisibility(bShow);
	if (Light) { Light->SetVisibility(bShow); }
	Title->SetVisibility(bShow); Body->SetVisibility(bShow); Detail->SetVisibility(bShow); Footer->SetVisibility(bShow);
}

void AGatewayVRPanel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AGatewayDirector* D = AGatewayDirector::Get(GetWorld());
	if (!D) return;
	const EGatewayState St = D->GetState();
	const bool bMenuState = (St != EGatewayState::Running);
	StartupRecenter += DeltaSeconds;
	if (bMenuState && (bNeedsRecenter || (StartupRecenter > 1.5f && StartupRecenter < 1.5f + DeltaSeconds) || (StartupRecenter > 4.f && StartupRecenter < 4.f + DeltaSeconds))) { Recenter(); }
	if (!bMenuState) { bNeedsRecenter = true; }
	SetVisibleSmooth(bMenuState, DeltaSeconds);

	FString T, B, Dt2, Ft, Ov;
	int32 HiRow = -1;          // fila del cuerpo a resaltar
	float HiWidth = 108.f;     // ancho del rectangulo (mitad izquierda del panel por defecto)
	const TArray<FGatewaySessionDef>& S = D->GetSessions();
	switch (St)
	{
	case EGatewayState::Menu:
	{
		T = TEXT("G A T E W A Y");
		const int32 Sel = D->GetMenuIndex();
		// Lista completa de filas (cabeceras de onda + sesiones) y fila de la seleccion
		TArray<FString> Lines; TArray<bool> IsHeader; int32 SelRow = 0;
		FString LastWave;
		for (int32 i = 0; i < S.Num(); ++i)
		{
			if (S[i].Wave != LastWave) { LastWave = S[i].Wave; Lines.Add(FString::Printf(TEXT("<%s>"), *LastWave)); IsHeader.Add(true); }
			if (i == Sel) { SelRow = Lines.Num(); }
			const bool bDone = D->GetCompleted().Contains(S[i].Id);
			Lines.Add(FString::Printf(TEXT("  %s%s. %s   %d min"), bDone ? TEXT("*") : TEXT(" "), *S[i].OrderText(), *S[i].Title, int32(S[i].TotalSeconds / 60.f)));
			IsHeader.Add(false);
		}
		// Scroll: la seleccion siempre visible; al subir se arrastra tambien la cabecera de su onda
		const int32 MenuRows = VisibleRows();
		if (SelRow < MenuFirst) { MenuFirst = (SelRow > 0 && IsHeader[SelRow - 1]) ? SelRow - 1 : SelRow; }
		if (SelRow >= MenuFirst + MenuRows) { MenuFirst = SelRow - MenuRows + 1; }
		MenuFirst = FMath::Clamp(MenuFirst, 0, FMath::Max(0, Lines.Num() - MenuRows));
		for (int32 r = MenuFirst; r < Lines.Num() && r < MenuFirst + MenuRows; ++r) { B += Lines[r] + TEXT("\n"); }
		HiRow = SelRow - MenuFirst;
		if (S.IsValidIndex(Sel))
		{
			Dt2 = Wrap(S[Sel].Title + TEXT("\n") + S[Sel].Wave + TEXT("\n\n") + S[Sel].Description, 44);
			if (S[Sel].bUsesFlicker) { Dt2 += TEXT("\n(Incluye luz intermitente: ojos cerrados)"); }
		}
		Ft = TEXT("Stick der: elegir   A: comenzar   B: ayuda   click del stick: ajustes   gatillo: recentrar panel");
		if (D->GetDevices()) { Ft += TEXT("\nSalida: ") + D->GetDevices()->CurrentName(); }
		break;
	}
	case EGatewayState::Warning:
		T = TEXT("Antes de continuar: luz intermitente");
		B = Wrap(TEXT("Esta sesion incluye luz intermitente (flicker) de 4 a 12 Hz. Puede provocar crisis en personas con epilepsia fotosensible.\n\nNO la uses si tienes epilepsia o convulsiones (o un familiar directo), migrana con aura, psicosis, embarazo, psicofarmacos sin consultar, o eres menor.\n\nManten los OJOS CERRADOS durante el flicker. Si notas malestar, pulsa B: la luz se apaga.\n\nPuedes dejar el flicker en 0 en Ajustes. Al aceptar confirmas que no tienes contraindicaciones."), 60);
		Ft = TEXT("A: acepto y continuo      B: volver");
		break;
	case EGatewayState::Settings:
	{
		T = TEXT("Ajustes");
		const FGatewaySettings& Sg = D->GetSettings();
		const int32 Sel = D->GetSettingsIndex();
		const FString Rows[GatewaySettingsRows] = {
			FString::Printf(TEXT("Intensidad del flicker        %3d %%"), int32(Sg.FlickerScale * 100 + 0.5f)),
			FString::Printf(TEXT("Volumen tonos Hemi-Sync       %3d %%"), int32(Sg.VolTones * 100 + 0.5f)),
			FString::Printf(TEXT("Volumen voz guia              %3d %%"), int32(Sg.VolVoices * 100 + 0.5f)),
			FString::Printf(TEXT("Volumen ruido / ambiente      %3d %%"), int32(Sg.VolNoise * 100 + 0.5f)),
			FString::Printf(TEXT("Activar audio libre (visor quitado)   %s"), Sg.bAudioOnly ? TEXT("si") : TEXT("no")),
			FString::Printf(TEXT("Pantalla completa (escritorio) %s"), Sg.bFullscreen ? TEXT("si") : TEXT("no")),
			FString::Printf(TEXT("Salida de audio: %s"), D->GetDevices() ? *D->GetDevices()->CurrentName() : TEXT("")),
		};
		for (int32 i = 0; i < GatewaySettingsRows; ++i) { B += TEXT("  ") + Rows[i] + TEXT("\n"); }
		HiRow = Sel; HiWidth = 200.f;
		if (Sel == 4)
		{
			B += TEXT(" \n") + Wrap(TEXT("Audio libre: al comenzar una sesion la imagen se apaga y solo suena el audio, para quitarte el visor y dormir (sesiones 4.1, 5, 14 y 16). Cualquier boton vuelve a encender la imagen.\n\nPara que el Quest no se duerma al quitartelo, en el visor: Ajustes > Sistema > Energia > Suspension automatica del visor: 4 horas (o nunca)."), 88);
		}
		Ft = TEXT("Stick der arriba/abajo: elegir   izq/der: ajustar   A: entrar   B: volver");
		break;
	}
	case EGatewayState::Devices:
	{
		T = TEXT("Salida de audio");
		UGatewayAudioDevices* Dev = D->GetDevices();
		const int32 Sel = D->GetDeviceIndex();
		if (Dev)
		{
			for (int32 i = 0; i < Dev->GetDevices().Num(); ++i)
			{
				const FAudioOutputDeviceInfo& I = Dev->GetDevices()[i];
				B += FString::Printf(TEXT("  %s%s\n"), *I.Name, I.bIsCurrentDevice ? TEXT("  (actual)") : TEXT(""));
			}
			HiRow = Sel;
		}
		Dt2 = Wrap(TEXT("Elige los AirPods en su perfil ESTEREO (Headphones / Stereo). El perfil Hands-Free es mono y anula el efecto binaural."), 44);
		Ft = TEXT("A: usar esta salida      B: volver");
		break;
	}
	case EGatewayState::Finished:
	{
		const FGatewaySessionDef* Cs = D->CurrentSession();
		T = TEXT("Sesion completada");
		B = Cs ? Cs->Title : TEXT("");
		Dt2 = Wrap(TEXT("Tomate un minuto antes de levantarte. Si quieres recordar lo vivido, repasalo ahora."), 44);
		Ft = TEXT("A: volver al menu");
		break;
	}
	case EGatewayState::Running:
	{
		const FGatewaySessionDef* Cs = D->CurrentSession();
		const FGatewaySegment* Sg = D->CurrentSegment();
		if ((D->bShowOverlay || D->IsPaused()) && Cs && !Cs->bNight && !D->IsAudioOnlyActive())
		{
			const int32 E = int32(D->GetElapsed()), Tt = int32(Cs->TotalSeconds);
			Ov = FString::Printf(TEXT("%s\n%s\n%d:%02d / %d:%02d%s"), *Cs->Title, Sg ? *Sg->Name : TEXT(""), E / 60, E % 60, Tt / 60, Tt % 60, D->IsPaused() ? TEXT("\n\nPAUSA  (A continuar, B terminar)") : TEXT(""));
		}
		break;
	}
	}

	if (T != LastTitle) { Title->SetText(FText::FromString(T)); LastTitle = T; }
	if (B != LastBody) { Body->SetText(FText::FromString(B)); LastBody = B; }
	if (Dt2 != LastDetail) { Detail->SetText(FText::FromString(Dt2)); LastDetail = Dt2; }
	if (Ft != LastFooter) { Footer->SetText(FText::FromString(Ft)); LastFooter = Ft; }
	if (Ov != LastOverlay) { Overlay->SetText(FText::FromString(Ov)); LastOverlay = Ov; }

	int32 BodyLines = 0;
	for (const TCHAR C : B) { if (C == TEXT('\n')) ++BodyLines; }
	PlaceHighlight(bMenuState ? HiRow : -1, BodyLines, HiWidth);

	// El overlay de sesion sigue la mirada suavemente (arriba del campo visual)
	if (St == EGatewayState::Running && !Ov.IsEmpty())
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			FVector Loc; FRotator Rot; PC->GetPlayerViewPoint(Loc, Rot);
			const FVector Target = Loc + Rot.Vector() * 180.f;
			const FVector Cur = Overlay->GetComponentLocation();
			Overlay->SetWorldLocationAndRotation(FMath::VInterpTo(Cur, Target, DeltaSeconds, 2.f), FRotationMatrix::MakeFromX(Rot.Vector()).Rotator() + FRotator(0.f, 180.f, 0.f));
			Overlay->SetVisibility(true);
		}
	}
	else
	{
		Overlay->SetVisibility(false);
	}
}
