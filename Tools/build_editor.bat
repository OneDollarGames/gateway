@echo off
rem Compila el modulo Gateway (Editor, Development). Necesario tras cambiar C++.
call "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" GatewayEditor Win64 Development -Project="C:\Discos\Proyectos\NEXCODE\gateway\Gateway.uproject" -WaitMutex
echo EXIT %ERRORLEVEL%
