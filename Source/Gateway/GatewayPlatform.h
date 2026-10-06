#pragma once

#include "CoreMinimal.h"

// Ayudas de plataforma para el modo "audio libre" (visor quitado, solo audio).
namespace GatewayPlatform
{
	// Mantiene la pantalla/app despierta mientras el usuario no lleva puesto el visor.
	// Quest: FLAG_KEEP_SCREEN_ON + difusion com.oculus.vrpowermanager.prox_close (ignora el sensor
	// de proximidad; se revierte con automation_disable). En escritorio no hace nada.
	void SetKeepAwake(bool bOn);
}
