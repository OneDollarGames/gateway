#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AudioMixerBlueprintLibrary.h"
#include "GatewayAudioDevices.generated.h"

// Seleccion del dispositivo de salida (los AirPods). Enumera las salidas de Windows,
// prefiere automaticamente el perfil ESTEREO de los AirPods (el perfil "Hands-Free" es mono y
// destruye el batido binaural) y permite cambiar desde el menu.
UCLASS()
class GATEWAY_API UGatewayAudioDevices : public UObject
{
	GENERATED_BODY()

public:
	void Refresh(UObject* WorldContext);
	void Swap(UObject* WorldContext, const FString& DeviceId);
	// Elige los AirPods (o cualquier auricular estereo) si existen. Devuelve true si cambio.
	bool AutoSelectHeadphones(UObject* WorldContext);

	const TArray<FAudioOutputDeviceInfo>& GetDevices() const { return Devices; }
	FString CurrentName() const;
	bool IsRefreshing() const { return bRefreshing; }
	FString LastSwapMessage;

	// Puntuacion heuristica: mayor = mejor candidato a auriculares estereo
	static int32 ScoreDevice(const FAudioOutputDeviceInfo& D);

private:
	UFUNCTION() void OnDevicesObtained(const TArray<FAudioOutputDeviceInfo>& Available);
	UFUNCTION() void OnSwapCompleted(const FSwapAudioOutputResult& Result);

	UPROPERTY() TArray<FAudioOutputDeviceInfo> Devices;
	bool bRefreshing = false;
	TWeakObjectPtr<UObject> PendingAutoSelectContext;
};
