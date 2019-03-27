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
vr::IVRSystem * pSystem = NULL;
vr::IVRInput * pInput = NULL;
vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
//todo: handle actions dynamically
vr::VRActionSetHandle_t actionSet			= vr::k_ulInvalidActionSetHandle;
vr::VRActiveActionSet_t activeActionSet		= { 0 };
vr::VRActionHandle_t boolean_primaryfire	= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_secondaryfire	= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_changeweapon	= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_use			= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_spawnmenu		= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t pose_lefthand			= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t pose_righthand			= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t vector2_walkdirection	= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_walk			= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_flashlight		= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_turnleft		= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_turnright		= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_mic			= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_reload			= vr::k_ulInvalidActionHandle;
vr::VRActionHandle_t boolean_undo			= vr::k_ulInvalidActionHandle;

//directx
ID3D11DeviceContext* d3d11Context;
ID3D11Device* d3d11Device;
ID3D11Texture2D* d3d11Texture = NULL;
IDirect3DDevice9* d3d9Device;
HANDLE sharedTexture = NULL;

//other
float horizontalFOV = 0;
float horizontalOffset = 0;
float calculatedHorizontalOffset = 0;
uint32_t recommendedWidth = 0;
uint32_t recommendedHeight = 0;

//*************************************************************************
//                         CreateTexture Hook
//*************************************************************************

typedef HRESULT(APIENTRY* CreateTexture) (IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
CreateTexture CreateTexture_orig = 0;

HRESULT APIENTRY CreateTexture_hook(IDirect3DDevice9* pDevice, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** tex, HANDLE* shared_handle) {
	if (sharedTexture == NULL) {
		shared_handle = &sharedTexture;
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

	if (dx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &d3d9Device) != D3D_OK) {
		MessageBoxA(NULL, "CreateDevice", NULL, MB_OK);
		return;
	}

	//add the hook
	DWORD* dVtable = (DWORD*)d3d9Device;
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
	if (sharedTexture == NULL)
		return 0;

	if (d3d11Texture == NULL) {
		//create dx11 device
		if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &d3d11Device, NULL, &d3d11Context) != S_OK) {
			MessageBoxA(NULL, "D3D11CreateDevice", NULL, MB_OK);
			return 0;
		}
		//get dx11 texture from shared texture handle
		ID3D11Resource* res;
		if (FAILED(d3d11Device->OpenSharedResource(sharedTexture, __uuidof(ID3D11Resource), (void**)&res))) {
			MessageBoxA(NULL, "OpenSharedResource", NULL, MB_OK);
			return 0;
		}

		if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&d3d11Texture))) {
			MessageBoxA(NULL, "QueryInterface", NULL, MB_OK);
			return 0;
		}
		//dont need the hook anymore
		DWORD* dVtable = (DWORD*)d3d9Device;
		dVtable = (DWORD*)dVtable[0];
		MH_DisableHook((DWORD_PTR*)dVtable[23]);
		MH_RemoveHook((DWORD_PTR*)dVtable[23]);
		if (MH_Uninitialize() != MH_OK)
		{
			MessageBoxA(NULL, "MH_Uninitialize", NULL, MB_OK);
			return 0;
		}
	}

	vr::Texture_t vrTexture = { d3d11Texture, vr::TextureType_DirectX, vr::ColorSpace_Auto };
	
	vr::VRTextureBounds_t textureBounds;

	//submit Left eye
	textureBounds.uMin = 0.0f - horizontalOffset * 0.25f;
	textureBounds.uMax = 0.5f - horizontalOffset * 0.25f;
	textureBounds.vMin = 0.0f;
	textureBounds.vMax = 1.0f;

	vr::VRCompositor()->Submit(vr::EVREye::Eye_Left, &vrTexture, &textureBounds);

	//submit Right eye
	textureBounds.uMin = 0.5f + horizontalOffset * 0.25f;
	textureBounds.uMax = 1.0f + horizontalOffset * 0.25f;
	textureBounds.vMin = 0.0f;
	textureBounds.vMax = 1.0f;

	vr::VRCompositor()->Submit(vr::EVREye::Eye_Right, &vrTexture, &textureBounds);

	return 0;
}

//*************************************************************************
//                             LUA VRMOD_Init
//*************************************************************************
LUA_FUNCTION(VRMOD_Init) {

	vr::HmdError error = vr::VRInitError_None;

	pSystem = vr::VR_Init(&error, vr::VRApplication_Scene);
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

	pSystem->GetRecommendedRenderTargetSize(&recommendedWidth, &recommendedHeight);

	//get FOV and display offset values
	vr::HmdMatrix44_t proj = pSystem->GetProjectionMatrix(vr::Hmd_Eye::Eye_Left, 1, 10);
	float xscale = proj.m[0][0];
	float xoffset = proj.m[0][2];
	float yscale = proj.m[1][1];
	float yoffset = proj.m[1][2];
	float fov_px = 2.0f * (atanf(fabsf((1.0f - xoffset) / xscale)) * 180 / 3.141592654);
	float fov_nx = 2.0f * (atanf(fabsf((-1.0f - xoffset) / xscale)) * 180 / 3.141592654);
	float fov_py = 2.0f * (atanf(fabsf((1.0f - yoffset) / yscale)) * 180 / 3.141592654);
	float fov_ny = 2.0f * (atanf(fabsf((-1.0f - yoffset) / yscale)) * 180 / 3.141592654);
	horizontalFOV = max(fov_px, fov_nx);
	calculatedHorizontalOffset = -xoffset;
	horizontalOffset = -xoffset;

	//prepare action related stuff (todo: handle actions dynamically)
	char dir[256];
	GetCurrentDirectory(256, dir);
	char path[256];
	sprintf_s(path, 256, "%s\\garrysmod\\lua\\bin\\vrmod_action_manifest.json", dir);

	pInput = vr::VRInput();
	if (pInput->SetActionManifestPath(path) != vr::VRInputError_None) {
		MessageBoxA(NULL, "vrmod_action_manifest.json not found!", NULL, MB_OK);
	}
	pInput->GetActionSetHandle("/actions/vrmod", &actionSet);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_primaryfire", &boolean_primaryfire);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_secondaryfire", &boolean_secondaryfire);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_changeweapon", &boolean_changeweapon);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_spawnmenu", &boolean_spawnmenu);
	pInput->GetActionHandle("/actions/vrmod/in/pose_lefthand", &pose_lefthand);
	pInput->GetActionHandle("/actions/vrmod/in/pose_righthand", &pose_righthand);
	pInput->GetActionHandle("/actions/vrmod/in/vector2_walkdirection", &vector2_walkdirection);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_walk", &boolean_walk);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_flashlight", &boolean_flashlight);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_turnleft", &boolean_turnleft);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_turnright", &boolean_turnright);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_use", &boolean_use);
	pInput->GetActionHandle("/actions/vrmod/in/boolean_mic", &boolean_mic);

	activeActionSet.ulActionSet = actionSet;

	LUA->PushNumber(0);
	return 1;
}

//*************************************************************************
//                            LUA VRMOD_Shutdown
//*************************************************************************
LUA_FUNCTION(VRMOD_Shutdown) {
	vr::VR_Shutdown();
	pSystem = NULL;
	d3d9Device->Release();
	d3d11Device->Release();
	d3d11Context->Release();
	d3d11Texture = NULL;
	sharedTexture = NULL;
	return 0;
}

//*************************************************************************
//                            LUA VRMOD_UpdatePoses
//*************************************************************************
LUA_FUNCTION(VRMOD_UpdatePoses) {
	vr::VRCompositor()->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
	pInput->UpdateActionState(&activeActionSet, sizeof(activeActionSet), 1);
	return 0;
}

//*************************************************************************
//                             LUA VRMOD_GetPoses
//*************************************************************************
LUA_FUNCTION(VRMOD_GetPoses) {
	vr::InputPoseActionData_t poseActionData;
	vr::TrackedDevicePose_t pose;
	char poseName[32];

	LUA->CreateTable();
	
	for (int i = 0; i < 3; i++) {
		//select a pose
		poseActionData.pose.bPoseIsValid = 0;
		pose.bPoseIsValid = 0;
		if (i == 0) {
			pose = poses[0];
			memcpy(poseName, "hmd", 4);
		}
		else if (i == 1) {
			pInput->GetPoseActionData(pose_lefthand, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
			pose = poseActionData.pose;
			memcpy(poseName, "lefthand", 9);
		}
		else if (i == 2) {
			pInput->GetPoseActionData(pose_righthand, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
			pose = poseActionData.pose;
			memcpy(poseName, "righthand", 10);
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
			//todo check the correct axis for these
			vel.x = pose.vVelocity.v[0];
			vel.y = pose.vVelocity.v[1];
			vel.z = pose.vVelocity.v[2];
			angvel.x = pose.vAngularVelocity.v[0] * (180.0 / 3.141592654);
			angvel.y = pose.vAngularVelocity.v[1] * (180.0 / 3.141592654);
			angvel.z = pose.vAngularVelocity.v[2] * (180.0 / 3.141592654);
			
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
//todo: handle actions dynamically
LUA_FUNCTION(VRMOD_GetActions) {
	vr::InputDigitalActionData_t digitalActionData;
	vr::InputAnalogActionData_t analogActionData;

	LUA->CreateTable();

	LUA->PushBool((pInput->GetDigitalActionData(boolean_primaryfire, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "primaryfire");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_secondaryfire, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "secondaryfire");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_changeweapon, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "changeweapon");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_use, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "use");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_spawnmenu, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "spawnmenu");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_walk, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "walk");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_flashlight, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "flashlight");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_turnleft, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "turnleft");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_turnright, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "turnright");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_mic, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "mic");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_reload, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "reload");
	LUA->PushBool((pInput->GetDigitalActionData(boolean_undo, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
	LUA->SetField(-2, "undo");

	LUA->CreateTable();

	pInput->GetAnalogActionData(vector2_walkdirection, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
	LUA->PushNumber(analogActionData.x);
	LUA->SetField(-2, "x");
	LUA->PushNumber(analogActionData.y);
	LUA->SetField(-2, "y");

	LUA->SetField(-2, "walkdirection");

	return 1;
}

//*************************************************************************
//                        LUA VRMOD_SetDisplayOffset
//*************************************************************************
LUA_FUNCTION(VRMOD_SetDisplayOffset) {
	if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) {
		horizontalOffset = LUA->GetNumber(1);
	}
	else {
		horizontalOffset = calculatedHorizontalOffset;
	}
	return 0;
}

//*************************************************************************
//                        LUA VRMOD_GetFOV
//*************************************************************************
LUA_FUNCTION(VRMOD_GetFOV) {
	LUA->PushNumber(horizontalFOV);
	LUA->PushNumber((float)recommendedWidth / (float)recommendedHeight); //aspect ratio
	return 2;
}

//*************************************************************************
//                        LUA VRMOD_GetRecommendedResolution
//*************************************************************************
LUA_FUNCTION(VRMOD_GetRecommendedResolution) {
	LUA->PushNumber(recommendedWidth*2); //return full res, not just one eye
	LUA->PushNumber(recommendedHeight);
	return 2;
}

//*************************************************************************
//                           LUA VRMOD_GetIPD
//*************************************************************************
LUA_FUNCTION(VRMOD_GetIPD) {
	vr::HmdMatrix34_t eyeToHeadRight = pSystem->GetEyeToHeadTransform(vr::Eye_Right); //units are in meters
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
	LUA->PushNumber(4);
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
