#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatewayVRPanel.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class AGatewayDirector;

// Menu flotante para el visor (Quest): el HUD de canvas no se ve en VR, asi que el menu,
// los ajustes, la advertencia y los textos de sesion se dibujan como texto 3D sobre un panel
// a 2 m delante de la mirada. Se recentra al abrirse y con el boton Y.
UCLASS()
class GATEWAY_API AGatewayVRPanel : public AActor
{
	GENERATED_BODY()
public:
	AGatewayVRPanel();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Recenter();

private:
	UPROPERTY() USceneComponent* Root = nullptr;
	UPROPERTY() UStaticMeshComponent* Backdrop = nullptr;
	UPROPERTY() UTextRenderComponent* Title = nullptr;
	UPROPERTY() UTextRenderComponent* Body = nullptr;
	UPROPERTY() UTextRenderComponent* Detail = nullptr;
	UPROPERTY() UTextRenderComponent* Footer = nullptr;
	UPROPERTY() UTextRenderComponent* Overlay = nullptr;  // textos durante la sesion (titulo, segmento, tiempo)

	FString LastBody, LastDetail, LastTitle, LastFooter, LastOverlay;
	bool bPanelVisible = true;
	float PanelAlpha = 1.f;
	bool bNeedsRecenter = true;

	static FString Wrap(const FString& S, int32 MaxChars);
	void SetVisibleSmooth(bool bVisible, float Dt);
};
