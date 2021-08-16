@echo off

REM ***************************** edit this with the correct path to vcvarsall.bat ******************************

set vcvarsallpath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"

REM ******************************************************************************************************************

if exist deps\ goto build
echo Dependencies not found. Press any key to attempt download.
pause
mkdir deps 2>NUL
mkdir deps\gmod 2>NUL
mkdir deps\openvr 2>NUL
pushd deps\gmod
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/Facepunch/gmod-module-base/archive/15bf18f369a41ac3d4eba29ee0679f386ec628b7.zip -Out tmp.zip; Expand-Archive tmp.zip -Force; Move-Item tmp\gmod-module-base-15bf18f369a41ac3d4eba29ee0679f386ec628b7\include\GarrysMod\Lua\* -Force; Remove-Item tmp.zip; Remove-Item tmp -Recurse;
popd
pushd deps\openvr
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/ValveSoftware/openvr/raw/823135df1783009cb468d0fc4190816254f7687d/headers/openvr.h -Out openvr.h;
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/ValveSoftware/openvr/raw/823135df1783009cb468d0fc4190816254f7687d/lib/win32/openvr_api.lib -Out openvr_api_win32.lib;
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/ValveSoftware/openvr/raw/823135df1783009cb468d0fc4190816254f7687d/lib/win64/openvr_api.lib -Out openvr_api_win64.lib;
popd
echo Download complete (if there are no errors above). Press any key to attempt build.
pause

:build

set CompilerFlags= -MT -nologo -Oi -O2 -W3 /wd4996 /I..\..\..\..\..\deps
set LinkerFlags= -INCREMENTAL:NO -opt:ref d3d11.lib USER32.LIB Shell32.lib /LIBPATH:..\..\..\..\..\deps\openvr /DLL
	
pushd install\GarrysMod\garrysmod\lua\bin
call %vcvarsallpath% x64
cl %CompilerFlags% ..\..\..\..\..\src\vrmod.cpp /link %LinkerFlags% openvr_api_win64.lib /out:gmcl_vrmod_win64.dll
call %vcvarsallpath% x86
cl %CompilerFlags% ..\..\..\..\..\src\vrmod.cpp /link %LinkerFlags% openvr_api_win32.lib /out:gmcl_vrmod_win32.dll
del gmcl_vrmod_win64.exp
del gmcl_vrmod_win64.lib
del gmcl_vrmod_win32.exp
del gmcl_vrmod_win32.lib
del vrmod.obj
popd

pause
