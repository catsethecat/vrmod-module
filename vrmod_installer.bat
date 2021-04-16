@echo off
SETLOCAL EnableDelayedExpansion
title VRMod Module Installer

FOR /F "tokens=*" %%a in ('powershell -command $PSVersionTable.PSVersion.Major 2^>nul') do set powershell_version=%%a
if defined powershell_version if !powershell_version! geq 4 GOTO update
echo This script requires Windows PowerShell 4.0+ (included in Windows 8.1 and later)
pause & exit

:update
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/catsethecat/vrmod-module/raw/master/vrmod_installer.bat -Out vrmod_installer.bat

:prompt
cls
FOR /F "tokens=2* skip=2" %%a in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam" /v "InstallPath" 2^>nul') do set steam_dir=%%b
FOR /F "tokens=2* skip=2" %%a in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam" /v "InstallPath" 2^>nul') do set steam_dir=%%b
if not defined steam_dir (
	echo "Steam installation path not found"
	pause
	exit
)
if exist "%steam_dir%\steamapps\appmanifest_4000.acf" set "gmod_dir=%steam_dir%\steamapps\common\GarrysMod"
for /f "usebackq tokens=2 skip=4" %%A in ("%steam_dir%\steamapps\libraryfolders.vdf") do (
  if exist "%%~A\steamapps\appmanifest_4000.acf" set "gmod_dir=%%~A\steamapps\common\GarrysMod"
)
if not defined gmod_dir (
	echo "GMod installation path not found"
	pause
	exit
)

echo Game folder: %gmod_dir%
echo Make sure Garry's Mod is not running before proceeding.
echo.

echo Select an option:
echo 1) install / update
echo 2) uninstall
set /p choice="> "

cls
if %choice%==1 GOTO install
if %choice%==2 GOTO uninstall
echo Invalid option. Valid options are: 1, 2
pause
goto prompt

:install
pushd %gmod_dir%
echo Downloading VRMod repository...
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/catsethecat/vrmod-module/archive/master.zip -Out vrmod.zip

if not exist vrmod.zip (
	echo "Download failed"
	pause
	exit
)

powershell -command "$testlol = Get-FileHash -a SHA1 vrmod.zip | Select-Object -expandproperty Hash | Out-String; echo ('vrmod.zip SHA1: ' + $testlol)"

echo Continue installation Y/N?
set /p choice="> "
if /I not %choice% == Y del vrmod.zip & exit
cls

echo Uncompressing...
powershell -command Expand-Archive vrmod.zip -Force
echo Installing...
xcopy /e /y /q vrmod\vrmod-module-master\install\GarrysMod "%gmod_dir%"
rmdir /s /q vrmod
del vrmod.zip
echo.
echo Done
echo.
pause
exit

:uninstall
echo Uninstalling...
del "%gmod_dir%\garrysmod\lua\bin\gmcl_vrmod_win32.dll"
del "%gmod_dir%\garrysmod\lua\bin\gmcl_vrmod_win64.dll"
del "%gmod_dir%\garrysmod\lua\bin\gmcl_vrmod_linux.dll"
del "%gmod_dir%\garrysmod\lua\bin\gmcl_vrmod_linux64.dll"
del "%gmod_dir%\garrysmod\lua\bin\update_vrmod.bat"
del "%gmod_dir%\bin\openvr_api.dll"
del "%gmod_dir%\bin\openvr_license"
del "%gmod_dir%\bin\libopenvr_api.so"
del "%gmod_dir%\bin\linux32\libopenvr_api.so"
del "%gmod_dir%\bin\linux64\libopenvr_api.so"
del "%gmod_dir%\bin\win64\openvr_api.dll"
del "%gmod_dir%\bin\win64\HTC_License"
del "%gmod_dir%\bin\win64\libHTC_License.dll"
del "%gmod_dir%\bin\win64\nanomsg.dll"
del "%gmod_dir%\bin\win64\SRanipal.dll"
del "%gmod_dir%\bin\win64\SRWorks_Log.dll"
del "%gmod_dir%\bin\win64\ViveSR_Client.dll"

echo Done
pause
