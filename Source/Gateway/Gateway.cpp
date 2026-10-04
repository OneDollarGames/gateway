#include "Gateway.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogGateway);

void FGatewayModule::StartupModule()
{
	const FString ShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Gateway")))
	{
		AddShaderSourceDirectoryMapping(TEXT("/Gateway"), ShaderDir);
	}
	UE_LOG(LogGateway, Log, TEXT("Gateway module: shaders en %s"), *ShaderDir);
}

void FGatewayModule::ShutdownModule()
{
}

IMPLEMENT_PRIMARY_GAME_MODULE(FGatewayModule, Gateway, "Gateway");
