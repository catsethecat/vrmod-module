@echo off
powershell -command [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/catsethecat/vrmod-module/archive/master.zip -Out vrmod.zip; Expand-Archive vrmod.zip -Force; Move-Item vrmod\vrmod-module-master\install\GarrysMod\garrysmod\lua\bin\* -Force; Remove-Item vrmod.zip; Remove-Item vrmod -Recurse; echo 'Update script finished, check above for errors.';
@pause
