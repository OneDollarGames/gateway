# Programa de sesiones y guía de uso

## Qué es

Gateway es una experiencia inmersiva (PC + Meta Quest 3 por Quest Link, o monitor) que reproduce el **Gateway Process** del Monroe Institute con sonido Hemi-Sync sintetizado en tiempo real, visuales 3D y una voz guía en español, y lo complementa con las técnicas que sí tienen evidencia científica: respiración en resonancia, Ganzfeld, flicker a 10 Hz, reactivación de lucidez (TLR) e incubación de sueños (Dormio).

Todo el audio sale por un único sintetizador (`UGatewaySynth`), de modo que el programa puede elegir la salida (los AirPods) y controlar la mezcla: capas binaurales e isocrónicas, ruido rosa/marrón, tono de respiración, voz guía con *ducking* y campanas de señal.

## Antes de empezar

1. **AirPods** emparejados con Windows. En *Salida de audio* elige el perfil **estéreo** ("Headphones"/"Stereo"); el perfil "Hands-Free"/"Headset" es mono y anula el batido binaural. El programa lo elige solo si lo encuentra.
2. **Quest 3**: dos formas. (a) Nativo: instala el APK (`Tools\package_quest.bat` + `adb install`) y ábrelo desde Biblioteca → Fuentes desconocidas; no necesita PC. (b) PC VR: abre Quest Link (cable o Air Link) y luego lanza el programa; arranca en VR (`bStartInVR`). Sin visor, corre en el monitor.
3. Lugar oscuro y tranquilo, 45 minutos sin interrupciones, una hora después de comer. Acostado o reclinado.
4. Volumen: la voz apenas audible; los tonos por debajo de la voz.

## Controles

| Acción | Teclado/ratón | Quest (mando derecho) |
|---|---|---|
| Elegir sesión / opción | ↑ ↓, clic | stick arriba/abajo |
| Ajustar valor | ← → | stick izq/der |
| Comenzar / confirmar | Enter, Espacio | A |
| Volver / terminar sesión | Esc | B |
| Pausa | Espacio | A durante la sesión (o click del stick) |
| Saltar segmento | → | stick a la derecha durante la sesión |
| Recentrar el panel / vista | — | gatillo |
| Ajustes / Salida de audio / Ayuda | O / D / H | click del stick en el menú / desde Ajustes / B en el menú |
| Pantalla completa | F, Alt+Enter | — |

## El programa (de menos a más)

Sigue el orden. Cada sesión se apoya en la anterior; repite una sesión los días que quieras antes de avanzar (Monroe recomienda un ejercicio al día durante una semana o un mes). Las sesiones completadas se marcan en el menú (`Saved/gateway_log.txt`).

| # | Sesión | Duración | Qué hace |
|---|---|---|---|
| 1 | Orientación | 14 min | Comprobación L/R, demostración Hemi-Sync, caja de conversión de energía, afirmación, sintonía resonante, Focus 3 |
| 2 | Introducción a Focus 10 | 32 min | Relajación de diez puntos, "mente despierta, cuerpo dormido", anclas, afirmación de salud, retorno 10→1 |
| 3 | Focus 10 avanzado | 30 min | REBAL (globo de energía) y entradas/salidas repetidas de Focus 10 |
| 4 | Liberar y recargar | 27 min | Ejercicio diario: sacar un miedo de la caja, soltar la carga, recargar |
| 5 | Exploración del sueño | 41 min | Rodar, flotar, conteo 11-20 y procesador de sueño theta→delta (termina dormido) |
| 6 | Flujo libre 10 | 33 min | Propósito propio, silencio largo |
| 7 | Introducción a Focus 12 | 35 min | Señales de Focus 12 (alfa 10 Hz), túnel y mandala; flicker opcional 8 Hz |
| 8 | Respiración de color | 25 min | Verde/rojo/púrpura en Ganzfeld con respiración a 6 por minuto |
| 9 | Barra de energía | 28 min | Punto → barra → vórtice |
| 10 | Mapa del cuerpo vivo | 30 min | Sistemas del cuerpo por colores + rotación de atención (yoga nidra) |
| 11 | Luz: Ganzflicker | 21 min | Flicker 10 Hz ojos cerrados + alfa 10 Hz + ruido rosa (laboratorio) |
| 12 | Focus 15: no-tiempo | 40 min | Theta 7 Hz sobre 500-750 Hz, vacío, silencio |
| 13 | Focus 21: el puente | 42 min | Beta 16 Hz sobre 600-900 Hz, galaxia, flicker 7.83 Hz opcional |
| 14 | Sueño lúcido: entrenamiento | 32 min | TLR: 4 señales con guía + 12 señales mientras te duermes (termina dormido) |
| 15 | Sueño lúcido: noche | 8 h | 6 h de silencio, luego la señal cada 30 s durante 2 h con volumen creciente |
| 16 | Incubación de sueños | 92 min | Dormio Light: yoga nidra + 4 ciclos de incubación/despertar/reporte (termina dormido) |
| 17 | Señales Hemi-Sync 1973 | 35 min | Audio oficial del Monroe Institute (CC BY-NC-ND), requiere `Tools/descargar_hemisync1973.py` |

Sugerencia de calendario: semana 1-2 sesiones 1-4 (la 4 a diario), semana 3 sesiones 5-6, semanas 4-6 Onda II, después Onda III y el laboratorio; el bloque de sueño lúcido cuando duermas bien y tengas buen recuerdo de los sueños.

## Recetas Hemi-Sync usadas (portadora[batido] Hz)

- Preparación: 50[0.5] 100[1.5] + ruido rosa y oleaje
- Focus 3: 100[1.3] 288[3.7]
- Focus 10: 100[1.5] 200[4] 250[4] 300[4] (la de 200 con componente isocrónica al 30 %)
- Focus 12: Focus 10 + 400[10] 500[10.1] 600[4.8] + 50[0.25]
- Focus 15: Focus 10 + 500[7.05] 630[7.1] 750[7]
- Focus 21: 200[4] 250[4] 300[4] + 600[16.2] 750[15.9] 900[16.2]
- Sueño: theta 100[4] 300[4] 500[2] → delta 100[1.5] 300[1] 496[1] → profundo 100[0.75] 300[0.5] (patente 5,356,368)
- Retorno a C-1: 308[14] 500[15]
- Laboratorio: theta 250[6] isocrónico 45 %; alfa 250[10] isocrónico 50 % para el flicker

Fuentes y evidencia: `docs/01_gateway_monroe.md` y `docs/02_evidencia_cientifica.md`.

## Seguridad

- El **flicker** (sesiones 7, 11 y 13) se usa con los **ojos cerrados**. La primera vez el programa pide consentimiento. No lo uses con epilepsia (propia o familiar), migraña con aura, psicosis, embarazo o psicofármacos sin consulta. En Ajustes puedes dejarlo en 0 %.
- No conduzcas ni manejes máquinas justo después de una sesión.
- Las sesiones nocturnas fragmentan el sueño: no las uses si tienes insomnio.
- Qué NO promete el programa: "sincronización de ondas cerebrales" ni efectos de frecuencias concretas. Lo que sí está comprobado: relajación y reducción de ansiedad con batidos (g≈0.45), imaginería con flicker a 10 Hz (70-90 %), fenómenos visuales en Ganzfeld (>90 %), más sueños lúcidos con TLR y más incorporaciones oníricas con incubación.

## Editar o ampliar sesiones

Los guiones y recetas viven en `Tools/sesiones.py`. Cambia textos, tiempos, capas o visuales y ejecuta `python3 Tools/sesiones.py` (necesita `OPENAI_API_KEY`; solo sintetiza los textos nuevos). Los JSON resultantes en `Content/Gateway/Sessions` se leen al arrancar: no hace falta recompilar.

Campos del JSON por segmento: `inicio`, `duracion`, `voz`, `voz_gain`, `rampa`, `sonido` (`capas[{portadora, batido, gain, iso, pan}]`, `rosa`, `marron`, `oceano`, `tono_respiracion`, `master`), `visual` (`modo`: ganzfeld | orbe | tunel | mandala | cosmos | vacio | caja; `color`, `intensidad`, `velocidad`, `complejidad`, `matiz`, `imagen`), `flicker` (`hz`, `profundidad`, `forma`, `color`), `respiracion` (`activa`, `rpm`, `inhalar`, `guia`), `campana`, `campana_hz`, `campana_gain`.
