#include "GarrysMod/Lua/Interface.h"
#include <stdio.h>
#include <Windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <openvr.h>
#include <MinHook.h>


#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3d9.lib")

//*************************************************************************
//                             globals
//*************************************************************************

//openvr related
typedef struct {
	vr::VRActionHandle_t handle = vr::k_ulInvalidActionHandle;
	char fullname[256];
	char type[256];
	char* name;
}action;
vr::IVRSystem*			g_pSystem = NULL;
vr::IVRInput*			g_pInput = NULL;
vr::TrackedDevicePose_t g_poses[vr::k_unMaxTrackedDeviceCount];
vr::VRActionSetHandle_t g_actionSet	= vr::k_ulInvalidActionSetHandle;
vr::VRActiveActionSet_t g_activeActionSet = { 0 };
action					g_actions[64];
int						g_actionCount = 0;

//directx
ID3D11DeviceContext*	g_d3d11Context;
ID3D11Device*			g_d3d11Device;
ID3D11Texture2D*		g_d3d11Texture = NULL;
IDirect3DDevice9*		g_d3d9Device;
HANDLE					g_sharedTexture = NULL;

//other
float		g_horizontalFOV = 0;
float		g_verticalFOV = 0;
float		g_horizontalOffset = 0;
float		g_verticalOffset = 0;
float		g_calculatedHorizontalOffset = 0;
float		g_calculatedVerticalOffset = 0;
uint32_t	g_recommendedWidth = 0;
uint32_t	g_recommendedHeight = 0;

//*************************************************************************
//                         CreateTexture Hook
//*************************************************************************

typedef HRESULT(APIENTRY* CreateTexture) (IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
CreateTexture CreateTexture_orig = 0;

HRESULT APIENTRY CreateTexture_hook(IDirect3DDevice9* pDevice, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** tex, HANDLE* shared_handle) {
	if (g_sharedTexture == NULL) {
		shared_handle = &g_sharedTexture;
		pool = D3DPOOL_DEFAULT;
	}
	return CreateTexture_orig(pDevice, w, h, levels, usage, format, pool, tex, shared_handle);
};

//*************************************************************************
//                             HookDirectX
//*************************************************************************

void HookDirectX() {
		
	//create temporary dx9 interface
	IDirect3D9* dx = Direct3DCreate9(D3D_SDK_VERSION);
	if (dx == NULL) {
		MessageBoxA(NULL, "Direct3DCreate9", NULL, MB_OK);
		return;
	}

	//create temporary window for dx9 device
	HWND window = CreateWindowA("BUTTON", "Hooking...", WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
	if (window == NULL) {
		MessageBoxA(NULL, "CreateWindow", NULL, MB_OK);
		return;
	}

	//create dx9 device to get CreateTexture address
	D3DPRESENT_PARAMETERS params;
	ZeroMemory(&params, sizeof(params));
	params.Windowed = TRUE;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.hDeviceWindow = window;
	params.BackBufferFormat = D3DFMT_UNKNOWN;

	if (dx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &g_d3d9Device) != D3D_OK) {
		MessageBoxA(NULL, "CreateDevice", NULL, MB_OK);
		return;
	}

	//add the hook
	DWORD* dVtable = (DWORD*)g_d3d9Device;
	dVtable = (DWORD*)dVtable[0];

	CreateTexture_orig = (CreateTexture)dVtable[23];

	if (MH_Initialize() != MH_OK) {
		MessageBoxA(NULL, "MH_Initialize", NULL, MB_OK);
		return;
	}

	if (MH_CreateHook((DWORD_PTR*)dVtable[23], &CreateTexture_hook, reinterpret_cast<void**>(&CreateTexture_orig)) != MH_OK) {
		MessageBoxA(NULL, "MH_CreateHook", NULL, MB_OK);
		return;
	}

	if (MH_EnableHook((DWORD_PTR*)dVtable[23]) != MH_OK) {
		MessageBoxA(NULL, "MH_EnableHook", NULL, MB_OK);
		return;
	}

	//
	dx->Release();
	DestroyWindow(window);

	return;
}

//*************************************************************************
//                           LUA VRMOD_MirrorFrame
//*************************************************************************
LUA_FUNCTION(VRMOD_MirrorFrame) {
	if (g_sharedTexture == NULL)
		return 0;

	if (g_d3d11Texture == NULL) {
		//create dx11 device
		if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &g_d3d11Device, NULL, &g_d3d11Context) != S_OK) {
			MessageBoxA(NULL, "D3D11CreateDevice", NULL, MB_OK);
			return 0;
		}
		//get dx11 texture from shared texture handle
		ID3D11Resource* res;
		if (FAILED(g_d3d11Device->OpenSharedResource(g_sharedTexture, __uuidof(ID3D11Resource), (void**)&res))) {
			MessageBoxA(NULL, "OpenSharedResource", NULL, MB_OK);
			return 0;
		}

		if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_d3d11Texture))) {
			MessageBoxA(NULL, "QueryInterface", NULL, MB_OK);
			return 0;
		}
		//dont need the hook anymore
		DWORD* dVtable = (DWORD*)g_d3d9Device;
		dVtable = (DWORD*)dVtable[0];
		MH_DisableHook((DWORD_PTR*)dVtable[23]);
		MH_RemoveHook((DWORD_PTR*)dVtable[23]);
		if (MH_Uninitialize() != MH_OK)
		{
			MessageBoxA(NULL, "MH_Uninitialize", NULL, MB_OK);
			return 0;
		}
	}

	vr::Texture_t vrTexture = { g_d3d11Texture, vr::TextureType_DirectX, vr::ColorSpace_Auto };
	
	vr::VRTextureBounds_t textureBounds;

	//submit Left eye
	textureBounds.uMin = 0.0f - g_horizontalOffset * 0.25f;
	textureBounds.uMax = 0.5f - g_horizontalOffset * 0.25f;
	textureBounds.vMin = 0.0f + g_verticalOffset * 0.5f;
	textureBounds.vMax = 1.0f + g_verticalOffset * 0.5f;

	vr::VRCompositor()->Submit(vr::EVREye::Eye_Left, &vrTexture, &textureBounds);

	//submit Right eye
	textureBounds.uMin = 0.5f + g_horizontalOffset * 0.25f;
	textureBounds.uMax = 1.0f + g_horizontalOffset * 0.25f;
	textureBounds.vMin = 0.0f + g_verticalOffset * 0.5f;
	textureBounds.vMax = 1.0f + g_verticalOffset * 0.5f;

	vr::VRCompositor()->Submit(vr::EVREye::Eye_Right, &vrTexture, &textureBounds);

	return 0;
}

//*************************************************************************
//                             LUA VRMOD_Init
//*************************************************************************
LUA_FUNCTION(VRMOD_Init) {

	vr::HmdError error = vr::VRInitError_None;

	g_pSystem = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None) {
		MessageBoxA(NULL, "VR_Init failed", NULL, MB_OK);
		LUA->PushNumber(-1);
		return 1;
	}

	if (!vr::VRCompositor()) {
		MessageBoxA(NULL, "VRCompositor failed", NULL, MB_OK);
		LUA->PushNumber(-1);
		return 1;
	}

	HookDirectX();

	g_pSystem->GetRecommendedRenderTargetSize(&g_recommendedWidth, &g_recommendedHeight);

	//get FOV and display offset values
	vr::HmdMatrix44_t proj = g_pSystem->GetProjectionMatrix(vr::Hmd_Eye::Eye_Left, 1, 10);
	float xscale = proj.m[0][0];
	float xoffset = proj.m[0][2];
	float yscale = proj.m[1][1];
	float yoffset = proj.m[1][2];
	float tan_px = fabsf((1.0f - xoffset) / xscale);
	float tan_nx = fabsf((-1.0f - xoffset) / xscale);
	float tan_py = fabsf((1.0f - yoffset) / yscale);
	float tan_ny = fabsf((-1.0f - yoffset) / yscale);
	float w = tan_px + tan_nx;
	float h = tan_py + tan_ny;
	g_horizontalFOV = atan(w / 2.0f) * 180 / 3.141592654 * 2;
	g_verticalFOV = atan(h / 2.0f) * 180 / 3.141592654 * 2;
	g_calculatedHorizontalOffset = -xoffset;
	g_calculatedVerticalOffset = -yoffset;
	g_horizontalOffset = g_calculatedHorizontalOffset;
	g_verticalOffset = g_calculatedVerticalOffset;

	//prepare action related stuff
	char dir[256];
	GetCurrentDirectory(256, dir);
	char path[256];
	sprintf_s(path, 256, "%s\\garrysmod\\lua\\bin\\vrmod_action_manifest.json", dir);

	g_pInput = vr::VRInput();
	if (g_pInput->SetActionManifestPath(path) != vr::VRInputError_None) {
		MessageBoxA(NULL, "SetActionManifestPath", NULL, MB_OK);
		LUA->PushNumber(-1);
		return 1;
	}
	g_pInput->GetActionSetHandle("/actions/vrmod", &g_actionSet);

	FILE * file = NULL;
	fopen_s(&file, path, "r");
	if (file == NULL) {
		MessageBoxA(NULL, "failed to open vrmod_action_manifest.json", NULL, MB_OK);
		LUA->PushNumber(-1);
		return 1;
	}

	char word[256];
	while (fscanf_s(file, "%*[^\"]\"%[^\"]\"", word, 256) == 1) {
		if (strcmp(word, "name") == 0) {
			if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", g_actions[g_actionCount].fullname, 256) != 1)
				break;
			if (fscanf_s(file, "%*[^\"]\"type\"%*[^\"]\"%[^\"]\"", g_actions[g_actionCount].type, 256) != 1)
				break;
			g_actions[g_actionCount].name = g_actions[g_actionCount].fullname;
			for (int i = 0; i < strlen(g_actions[g_actionCount].fullname); i++) {
				if (g_actions[g_actionCount].fullname[i] == '/')
					g_actions[g_actionCount].name = g_actions[g_actionCount].fullname + i + 1;
			}
			g_pInput->GetActionHandle(g_actions[g_actionCount].fullname, &(g_actions[g_actionCount].handle));
			g_actionCount++;
		}
	}

	fclose(file);

	g_activeActionSet.ulActionSet = g_actionSet;

	LUA->PushNumber(0);
	return 1;
}

//*************************************************************************
//                            LUA VRMOD_Shutdown
//*************************************************************************
LUA_FUNCTION(VRMOD_Shutdown) {
	vr::VR_Shutdown();
	g_pSystem = NULL;
	g_d3d9Device->Release();
	g_d3d11Device->Release();
	g_d3d11Context->Release();
	g_d3d11Texture = NULL;
	g_sharedTexture = NULL;
	return 0;
}

//*************************************************************************
//                            LUA VRMOD_UpdatePoses
//*************************************************************************
LUA_FUNCTION(VRMOD_UpdatePoses) {
	vr::VRCompositor()->WaitGetPoses(g_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
	g_pInput->UpdateActionState(&g_activeActionSet, sizeof(g_activeActionSet), 1);
	return 0;
}

//*************************************************************************
//                             LUA VRMOD_GetPoses
//*************************************************************************
LUA_FUNCTION(VRMOD_GetPoses) {
	vr::InputPoseActionData_t poseActionData;
	vr::TrackedDevicePose_t pose;
	char poseName[64];

	LUA->CreateTable();
	
	for (int i = -1; i < g_actionCount; i++) {
		//select a pose
		poseActionData.pose.bPoseIsValid = 0;
		pose.bPoseIsValid = 0;
		if (i == -1) {
			pose = g_poses[0];
			memcpy(poseName, "hmd", 4);
		}
		else if (strcmp(g_actions[i].type,"pose") == 0) {
			g_pInput->GetPoseActionData(g_actions[i].handle, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
			pose = poseActionData.pose;
			strcpy_s(poseName, 64, g_actions[i].name);
		}
		else {
			continue;
		}
		//
		if (pose.bPoseIsValid) {
			//do some conversion
			vr::HmdMatrix34_t mat = pose.mDeviceToAbsoluteTracking;
			Vector pos;
			Vector vel;
			QAngle ang;
			QAngle angvel;
			pos.x = -mat.m[2][3];
			pos.y = -mat.m[0][3];
			pos.z = mat.m[1][3];
			ang.x = asin(mat.m[1][2]) * (180.0 / 3.141592654);
			ang.y = atan2f(mat.m[0][2], mat.m[2][2]) * (180.0 / 3.141592654);
			ang.z = atan2f(-mat.m[1][0], mat.m[1][1]) * (180.0 / 3.141592654);
			vel.x = -pose.vVelocity.v[2];
			vel.y = -pose.vVelocity.v[0];
			vel.z = pose.vVelocity.v[1];
			angvel.x = -pose.vAngularVelocity.v[2] * (180.0 / 3.141592654);
			angvel.y = -pose.vAngularVelocity.v[0] * (180.0 / 3.141592654);
			angvel.z = pose.vAngularVelocity.v[1] * (180.0 / 3.141592654);
			
			//push a table for the pose
			LUA->CreateTable();

			LUA->PushVector(pos);
			LUA->SetField(-2, "pos");

			LUA->PushVector(vel);
			LUA->SetField(-2, "vel");

			LUA->PushAngle(ang);
			LUA->SetField(-2, "ang");

			LUA->PushAngle(angvel);
			LUA->SetField(-2, "angvel");

			LUA->SetField(-2, poseName);

		}

	}

	return 1;
}


//*************************************************************************
//                          LUA VRMOD_GetActions
//*************************************************************************
LUA_FUNCTION(VRMOD_GetActions) {
	vr::InputDigitalActionData_t digitalActionData;
	vr::InputAnalogActionData_t analogActionData;

	LUA->CreateTable();

	for (int i = 0; i < g_actionCount; i++) {
		if (strcmp(g_actions[i].type, "boolean") == 0) {
			LUA->PushBool((g_pInput->GetDigitalActionData(g_actions[i].handle, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
			LUA->SetField(-2, g_actions[i].name);
		}
		else if (strcmp(g_actions[i].type, "vector1") == 0) {
			g_pInput->GetAnalogActionData(g_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
			LUA->PushNumber(analogActionData.x);
			LUA->SetField(-2, g_actions[i].name);
		}
		else if (strcmp(g_actions[i].type, "vector2") == 0) {
			LUA->CreateTable();
			g_pInput->GetAnalogActionData(g_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
			LUA->PushNumber(analogActionData.x);
			LUA->SetField(-2, "x");
			LUA->PushNumber(analogActionData.y);
			LUA->SetField(-2, "y");
			LUA->SetField(-2, g_actions[i].name);
		}
	}

	return 1;
}

//*************************************************************************
//                          LUA VRMOD_TriggerHaptic
//*************************************************************************
LUA_FUNCTION(VRMOD_TriggerHaptic) {
	const char * actionName = LUA->CheckString(1);
	unsigned int nameLen = strlen(actionName);
	for (int i = 0; i < g_actionCount; i++) {
		if (strlen(g_actions[i].name) == nameLen && memcmp(g_actions[i].name, actionName, nameLen) == 0) {
			g_pInput->TriggerHapticVibrationAction(g_actions[i].handle, LUA->CheckNumber(2), LUA->CheckNumber(3), LUA->CheckNumber(4), LUA->CheckNumber(5), vr::k_ulInvalidInputValueHandle);
			break;
		}
	}
	return 0;
}

//*************************************************************************
//                        LUA VRMOD_SetDisplayOffset
//*************************************************************************
LUA_FUNCTION(VRMOD_SetDisplayOffset) {
	if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER) && LUA->IsType(2, GarrysMod::Lua::Type::NUMBER)) {
		g_horizontalOffset = LUA->GetNumber(1);
		g_verticalOffset = LUA->GetNumber(2);
	}
	else {
		g_horizontalOffset = g_calculatedHorizontalOffset;
		g_verticalOffset = g_calculatedVerticalOffset;
	}
	return 0;
}

//*************************************************************************
//                        LUA VRMOD_GetFOV
//*************************************************************************
LUA_FUNCTION(VRMOD_GetFOV) {
	LUA->PushNumber(g_horizontalFOV);
	LUA->PushNumber((float)g_recommendedWidth / (float)g_recommendedHeight); //aspect ratio
	return 2;
}

//*************************************************************************
//                        LUA VRMOD_GetRecommendedResolution
//*************************************************************************
LUA_FUNCTION(VRMOD_GetRecommendedResolution) {
	LUA->PushNumber(g_recommendedWidth*2); //return full res, not just one eye
	LUA->PushNumber(g_recommendedHeight);
	return 2;
}

//*************************************************************************
//                           LUA VRMOD_GetIPD
//*************************************************************************
LUA_FUNCTION(VRMOD_GetIPD) {
	vr::HmdMatrix34_t eyeToHeadRight = g_pSystem->GetEyeToHeadTransform(vr::Eye_Right); //units are in meters
	LUA->PushNumber(eyeToHeadRight.m[0][3] * 2.0f); //ipd
	LUA->PushNumber(eyeToHeadRight.m[2][3]); //z (how far back the eye is compared to hmd pos?)
	return 2;
}

//*************************************************************************
//                        LUA VRMOD_HMDPresent
//*************************************************************************
LUA_FUNCTION(VRMOD_HMDPresent) {
	LUA->PushBool(vr::VR_IsHmdPresent());
	return 1;
}

//*************************************************************************
//                        LUA VRMOD_GetVersion
//*************************************************************************
LUA_FUNCTION(VRMOD_GetVersion) {
	LUA->PushNumber(7);
	return 1;
}

//*************************************************************************
//
//*************************************************************************
GMOD_MODULE_OPEN()
{

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_Init");
	LUA->PushCFunction(VRMOD_Init);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_Shutdown");
	LUA->PushCFunction(VRMOD_Shutdown);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_UpdatePoses");
	LUA->PushCFunction(VRMOD_UpdatePoses);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_GetPoses");
	LUA->PushCFunction(VRMOD_GetPoses);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_GetActions");
	LUA->PushCFunction(VRMOD_GetActions);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_TriggerHaptic");
	LUA->PushCFunction(VRMOD_TriggerHaptic);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_MirrorFrame");
	LUA->PushCFunction(VRMOD_MirrorFrame);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_SetDisplayOffset");
	LUA->PushCFunction(VRMOD_SetDisplayOffset);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_GetFOV");
	LUA->PushCFunction(VRMOD_GetFOV);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_GetRecommendedResolution");
	LUA->PushCFunction(VRMOD_GetRecommendedResolution);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_GetIPD");
	LUA->PushCFunction(VRMOD_GetIPD);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_HMDPresent");
	LUA->PushCFunction(VRMOD_HMDPresent);
	LUA->SetTable(-3);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushString("VRMOD_GetVersion");
	LUA->PushCFunction(VRMOD_GetVersion);
	LUA->SetTable(-3);

	return 0;
}

GMOD_MODULE_CLOSE()
{
	return 0;
}
