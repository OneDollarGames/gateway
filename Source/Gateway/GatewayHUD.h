#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GatewayHUD.generated.h"

class AGatewayDirector;
class UFont;

// Interfaz dibujada en canvas: menu del programa, consentimiento, ajustes, dispositivos,
// textos de sesion y guia de respiracion. Teclado y raton (hit boxes).
UCLASS()
class GATEWAY_API AGatewayHUD : public AHUD
{
	GENERATED_BODY()
public:
	AGatewayHUD();
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
	virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;

private:
	UPROPERTY() UFont* FontBig = nullptr;
	UPROPERTY() UFont* FontMed = nullptr;
	UPROPERTY() UFont* FontSmall = nullptr;
	FName Hovered;
	float OverlayAlpha = 0.f;

	void DrawMenu(AGatewayDirector* D);
	void DrawWarning(AGatewayDirector* D);
	void DrawSettings(AGatewayDirector* D);
	void DrawDevices(AGatewayDirector* D);
	void DrawRunning(AGatewayDirector* D);
	void DrawFinished(AGatewayDirector* D);
	void DrawBreathGuide(AGatewayDirector* D, float Alpha);

	void Panel(float X, float Y, float W, float H, float Alpha = 0.55f);
	float Text(const FString& S, float X, float Y, const FLinearColor& C, UFont* F, float Scale = 1.f);
	float TextCentered(const FString& S, float CX, float Y, const FLinearColor& C, UFont* F, float Scale = 1.f);
	float Wrapped(const FString& S, float X, float Y, float MaxW, const FLinearColor& C, UFont* F, float Scale = 1.f, float LineGap = 4.f);
	void Circle(float CX, float CY, float R, const FLinearColor& C, float Thickness, int32 Segs = 72);
	void Button(const FString& Label, float X, float Y, float W, float H, FName Id, bool bSelected);
	static FString Clock(float Seconds);
};
