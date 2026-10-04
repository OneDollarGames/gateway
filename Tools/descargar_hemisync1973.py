# -*- coding: utf-8 -*-
"""
Descarga el unico audio Hemi-Sync oficial que el Monroe Institute publica gratis y con licencia
abierta: "Member Audio (CD) 2003 - 1973 Hemi-Sync Signals" (CC BY-NC-ND 4.0, 34 min, solo senales,
sin voz), y lo deja en Content/Gateway/Audio/hemisync1973/NN.wav en 10 trozos de 48 kHz estereo
para que la sesion 17 ("Señales Hemi-Sync 1973") lo reproduzca.

No se incluye en el repositorio por tamano (356 MB). Requiere ffmpeg.
Fuente: https://archive.org/details/member-audio-cd-2003-1973-hemi-sync-signals
"""
import os
import subprocess
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Content", "Gateway", "Audio", "hemisync1973")
URL = "https://archive.org/download/member-audio-cd-2003-1973-hemi-sync-signals/2003%201973%20Hemi-Sync%20Signals.flac"
TROZOS = 10


def main():
    os.makedirs(OUT, exist_ok=True)
    flac = os.path.join(OUT, "hemisync1973.flac")
    if not os.path.exists(flac):
        print("descargando", URL)
        urllib.request.urlretrieve(URL, flac)
    dur = float(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", flac]).decode().strip())
    trozo = dur / TROZOS
    print("duracion %.1f s, trozos de %.2f s" % (dur, trozo))
    for i in range(TROZOS):
        wav = os.path.join(OUT, "%02d.wav" % (i + 1))
        if os.path.exists(wav):
            continue
        subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-ss", "%.3f" % (i * trozo), "-t", "%.3f" % trozo, "-i", flac, "-ar", "48000", "-ac", "2", "-sample_fmt", "s16", wav])
        print("  ", wav)
    with open(os.path.join(OUT, "LEEME.txt"), "w", encoding="utf-8") as f:
        f.write("1973 Hemi-Sync Signals (Monroe Institute, Member Audio CD 2003). Licencia CC BY-NC-ND 4.0.\n"
                "Fuente: https://archive.org/details/member-audio-cd-2003-1973-hemi-sync-signals\n"
                "Se reproduce sin modificar (solo troceado para cargarlo por partes). Uso no comercial.\n")
    print("listo")


if __name__ == "__main__":
    main()
