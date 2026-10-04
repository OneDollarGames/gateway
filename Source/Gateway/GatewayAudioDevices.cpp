#include "GatewayAudioDevices.h"
#include "Gateway.h"

void UGatewayAudioDevices::Refresh(UObject* WorldContext)
{
	bRefreshing = true;
	FOnAudioOutputDevicesObtained Delegate;
	Delegate.BindDynamic(this, &UGatewayAudioDevices::OnDevicesObtained);
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(WorldContext, Delegate);
}

void UGatewayAudioDevices::OnDevicesObtained(const TArray<FAudioOutputDeviceInfo>& Available)
{
	Devices = Available;
	bRefreshing = false;
	for (const FAudioOutputDeviceInfo& D : Devices)
	{
		UE_LOG(LogGateway, Log, TEXT("Salida de audio: '%s' canales=%d %dHz%s%s"), *D.Name, D.NumChannels, D.SampleRate,
			D.bIsSystemDefault ? TEXT(" [default]") : TEXT(""), D.bIsCurrentDevice ? TEXT(" [actual]") : TEXT(""));
	}
	if (UObject* Ctx = PendingAutoSelectContext.Get())
	{
		PendingAutoSelectContext.Reset();
		AutoSelectHeadphones(Ctx);
	}
}

int32 UGatewayAudioDevices::ScoreDevice(const FAudioOutputDeviceInfo& D)
{
	const FString N = D.Name.ToLower();
	int32 Score = 0;
	if (N.Contains(TEXT("airpods"))) Score += 100;
	if (N.Contains(TEXT("hands-free")) || N.Contains(TEXT("manos libres")) || N.Contains(TEXT("headset")) || N.Contains(TEXT("ag audio"))) Score -= 200; // perfil mono de llamadas
	if (N.Contains(TEXT("stereo")) || N.Contains(TEXT("estéreo")) || N.Contains(TEXT("estereo"))) Score += 40;
	if (N.Contains(TEXT("headphones")) || N.Contains(TEXT("auriculares")) || N.Contains(TEXT("audífonos")) || N.Contains(TEXT("audifonos"))) Score += 30;
	if (N.Contains(TEXT("bluetooth"))) Score += 10;
	if (N.Contains(TEXT("oculus")) || N.Contains(TEXT("virtual")) || N.Contains(TEXT("steam")) || N.Contains(TEXT("nvidia")) || N.Contains(TEXT("hdmi")) || N.Contains(TEXT("display"))) Score -= 150; // salidas virtuales o de monitor
	if (D.NumChannels < 2) Score -= 300;
	return Score;
}

bool UGatewayAudioDevices::AutoSelectHeadphones(UObject* WorldContext)
{
	if (Devices.Num() == 0)
	{
		PendingAutoSelectContext = WorldContext;
		Refresh(WorldContext);
		return false;
	}
	int32 Best = -1, BestScore = 49; // solo cambia de salida si hay un candidato claro (AirPods u otro auricular estereo)
	for (int32 i = 0; i < Devices.Num(); ++i)
	{
		const int32 S = ScoreDevice(Devices[i]);
		if (S > BestScore) { BestScore = S; Best = i; }
	}
	if (Best >= 0 && !Devices[Best].bIsCurrentDevice)
	{
		UE_LOG(LogGateway, Log, TEXT("Cambiando salida a '%s'"), *Devices[Best].Name);
		Swap(WorldContext, Devices[Best].DeviceId);
		return true;
	}
	return false;
}

void UGatewayAudioDevices::Swap(UObject* WorldContext, const FString& DeviceId)
{
	FOnCompletedDeviceSwap Delegate;
	Delegate.BindDynamic(this, &UGatewayAudioDevices::OnSwapCompleted);
	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(WorldContext, DeviceId, Delegate);
}

void UGatewayAudioDevices::OnSwapCompleted(const FSwapAudioOutputResult& Result)
{
	LastSwapMessage = FString::Printf(TEXT("Salida: %s (%s)"), *Result.RequestedDeviceId, Result.Result == ESwapAudioOutputDeviceResultState::Success ? TEXT("ok") : TEXT("fallo"));
	UE_LOG(LogGateway, Log, TEXT("%s"), *LastSwapMessage);
	for (FAudioOutputDeviceInfo& D : Devices) { D.bIsCurrentDevice = (D.DeviceId == Result.RequestedDeviceId) && Result.Result == ESwapAudioOutputDeviceResultState::Success; }
}

FString UGatewayAudioDevices::CurrentName() const
{
	for (const FAudioOutputDeviceInfo& D : Devices) { if (D.bIsCurrentDevice) return D.Name; }
	for (const FAudioOutputDeviceInfo& D : Devices) { if (D.bIsSystemDefault) return D.Name + TEXT(" (predeterminado)"); }
	return TEXT("(sin enumerar)");
}
