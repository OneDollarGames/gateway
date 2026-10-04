param([string]$Keys = "{ENTER}")
Add-Type -AssemblyName System.Windows.Forms
$p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like "Gateway (64-bit*" } | Select-Object -First 1
if (-not $p) { Write-Output "sin ventana del juego"; exit 1 }
$sh = New-Object -ComObject WScript.Shell
$sh.AppActivate($p.Id) | Out-Null
Start-Sleep -Milliseconds 250
[System.Windows.Forms.SendKeys]::SendWait($Keys)
Write-Output "ok $Keys"
