

mkdir -p "deps/gmod"
mkdir -p "deps/openvr/lib_linux32"
mkdir -p "deps/openvr/lib_linux64"

if [ ! -f "deps/gmod/Interface.h" ]; then
    wget -O deps/gmod/tmp.zip https://github.com/Facepunch/gmod-module-base/archive/15bf18f369a41ac3d4eba29ee0679f386ec628b7.zip
    unzip -j deps/gmod/tmp.zip gmod-module-base-15bf18f369a41ac3d4eba29ee0679f386ec628b7/include/GarrysMod/Lua/* -d deps/gmod/
    rm deps/gmod/tmp.zip
fi

if [ ! -f "deps/openvr/lib_linux32/libopenvr_api.so" ]; then
    wget -O deps/openvr/openvr.h https://github.com/ValveSoftware/openvr/raw/823135df1783009cb468d0fc4190816254f7687d/headers/openvr.h
    wget -O deps/openvr/lib_linux32/libopenvr_api.so https://github.com/ValveSoftware/openvr/raw/823135df1783009cb468d0fc4190816254f7687d/lib/linux32/libopenvr_api.so
    wget -O deps/openvr/lib_linux64/libopenvr_api.so https://github.com/ValveSoftware/openvr/raw/823135df1783009cb468d0fc4190816254f7687d/lib/linux64/libopenvr_api.so
fi

g++ -fPIC -shared -m32 -O3 -I ./deps src/vrmod.cpp -o install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_linux.dll -L ./deps/openvr/lib_linux32 -l openvr_api -ldl -Wl,-rpath='$ORIGIN'
g++ -fPIC -shared -m64 -O3 -I ./deps src/vrmod.cpp -o install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_linux64.dll -L ./deps/openvr/lib_linux64 -l openvr_api -ldl -Wl,-rpath='$ORIGIN'


