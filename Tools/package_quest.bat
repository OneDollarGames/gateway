@echo off
rem Empaqueta Gateway para Meta Quest (Android arm64, Vulkan) y deja el APK en Build\Quest.
rem Requiere el SDK/NDK/JDK configurados en DefaultEngine.ini (AndroidSDKSettings). Instalar: adb install -r <apk>
set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot
set ANDROID_HOME=C:\Users\jaime\AppData\Local\Android\Sdk
set ANDROID_SDK_ROOT=%ANDROID_HOME%
set NDKROOT=%ANDROID_HOME%\ndk\25.2.9519653
set NDK_ROOT=%NDKROOT%
set UE_SDKS_ROOT=
call "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="C:\Discos\Proyectos\NEXCODE\gateway\Gateway.uproject" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -package -pak -archive -archivedirectory="C:\Discos\Proyectos\NEXCODE\gateway\Build\Quest" -utf8output -unattended -nop4 -noP4
echo EXIT %ERRORLEVEL%
