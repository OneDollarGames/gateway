#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatewayVRPanel.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
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
	UPROPERTY() UStaticMeshComponent* Highlight = nullptr;  // rectangulo detras de la fila seleccionada
	UPROPERTY() UMaterialInstanceDynamic* HighlightMID = nullptr;
	UPROPERTY() UPointLightComponent* Light = nullptr;  // el material de texto es lit: sin luz sale negro
	UPROPERTY() UTextRenderComponent* Title = nullptr;
	UPROPERTY() UTextRenderComponent* Body = nullptr;
	UPROPERTY() UTextRenderComponent* Detail = nullptr;
	UPROPERTY() UTextRenderComponent* Footer = nullptr;
	UPROPERTY() UTextRenderComponent* Overlay = nullptr;  // textos durante la sesion (titulo, segmento, tiempo)

	FString LastBody, LastDetail, LastTitle, LastFooter, LastOverlay;
	bool bPanelVisible = true;
	float PanelAlpha = 1.f;
	bool bNeedsRecenter = true;
	float StartupRecenter = 0.f;   // recentrar de nuevo cuando el visor ya tiene pose (1.5 s y 4 s)
	int32 MenuFirst = 0;           // primera fila visible del menu (scroll)
	float LineH = 0.f;             // alto real de una linea del cuerpo (medido al dibujar)
	static constexpr float BodyTop = 52.f;     // z del borde superior del cuerpo
	static constexpr float BodyBottom = -56.f; // z donde empieza el pie
	static constexpr float BodyY = -104.f;     // y del borde izquierdo del cuerpo
	int32 VisibleRows() const { return LineH > 0.f ? FMath::Max(8, int32((BodyTop - BodyBottom) / LineH)) : 17; }

	static FString Wrap(const FString& S, int32 MaxChars);
	void SetVisibleSmooth(bool bVisible, float Dt);
	void PlaceHighlight(int32 Row, int32 TotalLines, float Width);
};
