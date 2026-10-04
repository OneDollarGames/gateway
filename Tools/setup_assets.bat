@echo off
rem Crea materiales, texturas y mapa dentro del editor (sin GUI). Requiere haber compilado antes.
"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Discos\Proyectos\NEXCODE\gateway\Gateway.uproject" -run=pythonscript -script="C:\Discos\Proyectos\NEXCODE\gateway\Tools\setup_assets.py" -unattended -nopause -stdout -FullStdOutLogOutput -NoShaderCompile=0
echo EXIT %ERRORLEVEL%
