#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGateway, Log, All);

// Modulo del juego. Registra el directorio Shaders/ del proyecto como "/Gateway"
// para que los materiales (nodo Custom) puedan incluir GatewayDome.ush y GatewayPost.ush.
class FGatewayModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
