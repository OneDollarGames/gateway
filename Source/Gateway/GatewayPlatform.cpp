#include "GatewayPlatform.h"
#include "Gateway.h"
#include "HAL/PlatformApplicationMisc.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

namespace
{
	// sendBroadcast(new Intent(Action)) desde la GameActivity; cualquier excepcion Java se traga
	void SendBroadcast(const char* Action)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env || !FJavaWrapper::GameActivityThis) return;
		jclass IntentClass = FAndroidApplication::FindJavaClass("android/content/Intent");
		if (!IntentClass) return;
		jmethodID Ctor = Env->GetMethodID(IntentClass, "<init>", "(Ljava/lang/String;)V");
		jmethodID Send = Env->GetMethodID(FJavaWrapper::GameActivityClassID, "sendBroadcast", "(Landroid/content/Intent;)V");
		if (Env->ExceptionCheck()) { Env->ExceptionClear(); return; }
		if (!Ctor || !Send) return;
		jstring JAction = Env->NewStringUTF(Action);
		jobject Intent = Env->NewObject(IntentClass, Ctor, JAction);
		if (Intent && !Env->ExceptionCheck()) { Env->CallVoidMethod(FJavaWrapper::GameActivityThis, Send, Intent); }
		if (Env->ExceptionCheck()) { Env->ExceptionClear(); UE_LOG(LogGateway, Warning, TEXT("Broadcast %s rechazado"), ANSI_TO_TCHAR(Action)); }
		else { UE_LOG(LogGateway, Log, TEXT("Broadcast %s enviado"), ANSI_TO_TCHAR(Action)); }
		if (Intent) { Env->DeleteLocalRef(Intent); }
		Env->DeleteLocalRef(JAction);
	}
}
#endif

void GatewayPlatform::SetKeepAwake(bool bOn)
{
	FPlatformApplicationMisc::ControlScreensaver(bOn ? FGenericPlatformApplicationMisc::Disable : FGenericPlatformApplicationMisc::Enable);
#if PLATFORM_ANDROID
	SendBroadcast(bOn ? "com.oculus.vrpowermanager.prox_close" : "com.oculus.vrpowermanager.automation_disable");
#endif
	UE_LOG(LogGateway, Log, TEXT("Audio libre: mantener despierto = %s"), bOn ? TEXT("si") : TEXT("no"));
}
