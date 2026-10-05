# Gateway

Experiencia inmersiva de luz y sonido para Meta Quest 3 (PC VR) y escritorio, hecha en **Unreal Engine 5.5 / C++**, que reproduce el **Gateway Process** del Monroe Institute (Hemi-Sync, Focus 3-21, caja de conversión de energía, REBAL, sintonía resonante) y lo amplía con las técnicas de inducción de estados alterados y sueños lúcidos con evidencia científica (respiración en resonancia, Ganzfeld, flicker a 10 Hz, TLR, incubación de sueños).

- 17 sesiones progresivas con voz guía en español (`Content/Gateway/Sessions`, `Content/Gateway/Voice`).
- Sintetizador binaural en tiempo real: hasta 8 capas portadora/batido con componente isocrónica y paneo, ruido rosa/marrón con oleaje, tono de respiración, voces WAV con *ducking*, campanas de señal, limitador.
- Salida de audio seleccionable (elige los AirPods en estéreo automáticamente).
- Visuales 3D: domo procedural (Ganzfeld, orbe REBAL, túnel, mandala, cosmos con imágenes de Webb/Hubble), 1 200 partículas con profundidad real alrededor del usuario, cofre de energía modelado en Blender, flicker fótico de pantalla completa con consentimiento y límite de seguridad.
- VR: OpenXR, tracking local (sentado/acostado), panel de menú flotante, aro de respiración en el domo, mandos Touch.

## Documentación

- [docs/01_gateway_monroe.md](docs/01_gateway_monroe.md): el documento CIA (con la página 25 real), la estructura de las Waves, los pasos de cada ejercicio, los niveles Focus y las frecuencias de las patentes y de las cintas.
- [docs/02_evidencia_cientifica.md](docs/02_evidencia_cientifica.md): qué está comprobado, qué es mixto y qué no (binaurales, flicker, Ganzfeld, sueños lúcidos, respiración, EEG).
- [docs/03_programa_y_uso.md](docs/03_programa_y_uso.md): programa de sesiones, controles, recetas Hemi-Sync, seguridad y cómo editar sesiones.
- `docs/fuentes/`: textos de dominio público usados (CIA, página 25, patentes, Atwater, análisis SBaGen).

## Compilar y ejecutar

Requisitos: Unreal Engine 5.5, Visual Studio 2022/2026 con C++ (MSVC 14.44+), Python 3 (numpy, soundfile, openai), ffmpeg y Blender 5.2 (solo para regenerar el cofre).

```bat
Tools\build_editor.bat      :: compila GatewayEditor (Development)
Tools\setup_assets.bat      :: crea materiales, importa imágenes/modelos y el mapa (editor sin GUI)
Tools\run_game.bat          :: lanza la experiencia (-game); con el Quest conectado arranca en VR
```

### Quest nativo (APK, sin PC)

Requiere el componente **Android** de UE 5.5 (Epic Games Launcher → 5.5.4 → ▾ → Options → Android), Android SDK 34 + NDK 25.2.9519653 + JDK 17 (rutas en `Config/DefaultEngine.ini`, `AndroidSDKSettings`) y el visor en modo de desarrollador (Ajustes → Sistema → Desarrollador) con la depuración USB aceptada.

```bat
Tools\package_quest.bat                                   :: BuildCookRun Android ASTC → Build\Quest\Android_ASTC\Gateway-arm64.apk
adb install -r -g Build\Quest\Android_ASTC\Gateway-arm64.apk
```

En el visor queda en **Biblioteca → Fuentes desconocidas → Gateway**. En Android el flicker y el fundido se aplican en el shader del domo (sin post-proceso, `r.MobileHDR=False`), hay 600 partículas y no hay selector de salida de audio (usa el audio del visor o unos AirPods emparejados al Quest).

Las sesiones y voces ya están generadas en el repo. Para regenerarlas: `python3 Tools/sesiones.py` (usa OpenAI TTS, voz *nova*). El audio oficial Hemi-Sync 1973 (sesión 17) se descarga con `python3 Tools/descargar_hemisync1973.py`.

## Estructura

```
Source/Gateway/     GatewaySynth (audio), GatewaySession (JSON), GatewayDirector (máquina de estados),
                    GatewayStage (domo + partículas + cofre + post), GatewayHUD (canvas), GatewayVRPanel (menú 3D),
                    GatewayPawn (cámara/HMD/mandos), GatewayAudioDevices (salida)
Shaders/            GatewayDome.ush, GatewayPost.ush (nodos Custom de los materiales)
Tools/              sesiones.py (guiones + TTS + JSON), setup_assets.py, blender_caja.py, descargar_hemisync1973.py
Content/Gateway/    Sessions/*.json, Voice/*.wav, Images/src (ESA/Webb, Hubble CC BY 4.0), Models/SM_Caja.fbx
```

## Créditos y licencias

- Gateway Process, Hemi-Sync y los niveles Focus son del Monroe Institute; este proyecto es una reproducción independiente con guiones propios. Las grabaciones comerciales no se incluyen.
- "1973 Hemi-Sync Signals" (Monroe Institute, CC BY-NC-ND 4.0) se descarga aparte y se reproduce sin modificar.
- Imágenes: ESA/Webb (NASA, ESA, CSA, STScI) y NASA/ESA Hubble, CC BY 4.0.
- Documento CIA "Analysis and Assessment of Gateway Process" (1983): dominio público.
