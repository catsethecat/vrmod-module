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
//  Globals
//*************************************************************************

//openvr related
typedef struct {
    vr::VRActionHandle_t handle;
    char fullname[256];
    char type[256];
    char* name;
}action;
typedef struct {
    vr::VRActionSetHandle_t handle;
    char name[256];
}actionSet;
vr::IVRSystem*          g_pSystem = NULL;
vr::IVRInput*           g_pInput = NULL;
vr::TrackedDevicePose_t g_poses[vr::k_unMaxTrackedDeviceCount];
actionSet               g_actionSets[16];
int                     g_actionSetCount = 0;
vr::VRActiveActionSet_t g_activeActionSets[16];
int                     g_activeActionSetCount = 0;
action                  g_actions[64];
int                     g_actionCount = 0;

//directx
typedef HRESULT(APIENTRY* CreateTexture) (IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
CreateTexture           g_CreateTextureOriginal = NULL;
ID3D11Device*           g_d3d11Device = NULL;
ID3D11Texture2D*        g_d3d11Texture = NULL;
HANDLE                  g_sharedTexture = NULL;
DWORD_PTR               g_CreateTextureAddr = NULL;

//other
float                   g_horizontalFOVLeft = 0;
float                   g_horizontalFOVRight = 0;
float                   g_aspectRatioLeft = 0;
float                   g_aspectRatioRight = 0;
float                   g_horizontalOffsetLeft = 0;
float                   g_horizontalOffsetRight = 0;
float                   g_verticalOffsetLeft = 0;
float                   g_verticalOffsetRight = 0;

//*************************************************************************
//  CreateTexture hook
//*************************************************************************
HRESULT APIENTRY CreateTextureHook(IDirect3DDevice9* pDevice, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** tex, HANDLE* shared_handle) {
    if (g_sharedTexture == NULL) {
        shared_handle = &g_sharedTexture;
        pool = D3DPOOL_DEFAULT;
    }
    return g_CreateTextureOriginal(pDevice, w, h, levels, usage, format, pool, tex, shared_handle);
};

//*************************************************************************
//    FindCreateTexture thread
//*************************************************************************
DWORD WINAPI FindCreateTexture(LPVOID lParam) {
    IDirect3D9* dx = Direct3DCreate9(D3D_SDK_VERSION);
    if (dx == NULL) {
        return 1;
    }

    HWND window = CreateWindowA("BUTTON", " ", WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (window == NULL) {
        dx->Release();
        return 2;
    }

    IDirect3DDevice9* d3d9Device = NULL;

    D3DPRESENT_PARAMETERS params;
    ZeroMemory(&params, sizeof(params));
    params.Windowed = TRUE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = window;
    params.BackBufferFormat = D3DFMT_UNKNOWN;

    //calling CreateDevice on the main thread seems to start causing random lua errors
    if (dx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &d3d9Device) != D3D_OK) {
        dx->Release();
        DestroyWindow(window);
        return 3;
    }

    g_CreateTextureAddr = ((DWORD_PTR*)(((DWORD_PTR*)d3d9Device)[0]))[23];

    d3d9Device->Release();
    dx->Release();
    DestroyWindow(window);

    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_GetVersion()
//    Returns: number
//*************************************************************************
LUA_FUNCTION(VRMOD_GetVersion) {
    LUA->PushNumber(13);
    return 1;
}

//*************************************************************************
//    Lua function: VRMOD_IsHMDPresent()
//    Returns: boolean
//*************************************************************************
LUA_FUNCTION(VRMOD_IsHMDPresent) {
    LUA->PushBool(vr::VR_IsHmdPresent());
    return 1;
}

//*************************************************************************
//    Lua function: VRMOD_Init()
//*************************************************************************
LUA_FUNCTION(VRMOD_Init) {
    vr::HmdError error = vr::VRInitError_None;

    g_pSystem = vr::VR_Init(&error, vr::VRApplication_Scene);
    if (error != vr::VRInitError_None) {
        LUA->ThrowError("VR_Init failed");
    }

    if (!vr::VRCompositor()) {
        LUA->ThrowError("VRCompositor failed");
    }

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
    g_horizontalFOVLeft = atan(w / 2.0f) * 180 / 3.141592654 * 2;
    //g_verticalFOV = atan(h / 2.0f) * 180 / 3.141592654 * 2;
    g_aspectRatioLeft = w / h;
    g_horizontalOffsetLeft = xoffset;
    g_verticalOffsetLeft = yoffset;

    proj = g_pSystem->GetProjectionMatrix(vr::Hmd_Eye::Eye_Right, 1, 10);
    xscale = proj.m[0][0];
    xoffset = proj.m[0][2];
    yscale = proj.m[1][1];
    yoffset = proj.m[1][2];
    tan_px = fabsf((1.0f - xoffset) / xscale);
    tan_nx = fabsf((-1.0f - xoffset) / xscale);
    tan_py = fabsf((1.0f - yoffset) / yscale);
    tan_ny = fabsf((-1.0f - yoffset) / yscale);
    w = tan_px + tan_nx;
    h = tan_py + tan_ny;
    g_horizontalFOVRight = atan(w / 2.0f) * 180 / 3.141592654 * 2;
    g_aspectRatioRight = w / h;
    g_horizontalOffsetRight = xoffset;
    g_verticalOffsetRight = yoffset;

    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_SetActionManifest(fileName)
//*************************************************************************
LUA_FUNCTION(VRMOD_SetActionManifest) {
    const char* fileName = LUA->CheckString(1);

    char currentDir[256];
    GetCurrentDirectory(256, currentDir);
    char path[256];
    sprintf_s(path, 256, "%s\\garrysmod\\data\\%s", currentDir, fileName);

    g_pInput = vr::VRInput();
    if (g_pInput->SetActionManifestPath(path) != vr::VRInputError_None) {
        LUA->ThrowError("SetActionManifestPath failed");
    }

    FILE* file = NULL;
    fopen_s(&file, path, "r");
    if (file == NULL) {
        LUA->ThrowError("failed to open action manifest");
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

    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_SetActiveActionSets(name, ...)
//*************************************************************************
LUA_FUNCTION(VRMOD_SetActiveActionSets) {
    g_activeActionSetCount = 0;
    for (int i = 0; i < 16; i++) {
        if (LUA->GetType(i + 1) == GarrysMod::Lua::Type::STRING) {
            const char* actionSetName = LUA->CheckString(i + 1);
            int actionSetIndex = -1;
            for (int j = 0; j < g_actionSetCount; j++) {
                if (strcmp(actionSetName, g_actionSets[j].name) == 0) {
                    actionSetIndex = j;
                    break;
                }
            }
            if (actionSetIndex == -1) {
                g_pInput->GetActionSetHandle(actionSetName, &g_actionSets[g_actionSetCount].handle);
                memcpy(g_actionSets[g_actionSetCount].name, actionSetName, strlen(actionSetName));
                actionSetIndex = g_actionSetCount;
                g_actionSetCount++;
            }
            g_activeActionSets[g_activeActionSetCount].ulActionSet = g_actionSets[actionSetIndex].handle;
            g_activeActionSetCount++;
        }
        else {
            break;
        }
    }
    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_GetViewParameters()
//    Returns: table
//*************************************************************************
LUA_FUNCTION(VRMOD_GetViewParameters) {
    LUA->CreateTable();

    LUA->PushNumber(g_horizontalFOVLeft);
    LUA->SetField(-2, "horizontalFOVLeft");

    LUA->PushNumber(g_horizontalFOVRight);
    LUA->SetField(-2, "horizontalFOVRight");

    LUA->PushNumber(g_aspectRatioLeft);
    LUA->SetField(-2, "aspectRatioLeft");

    LUA->PushNumber(g_aspectRatioRight);
    LUA->SetField(-2, "aspectRatioRight");

    uint32_t recommendedWidth = 0;
    uint32_t recommendedHeight = 0;
    g_pSystem->GetRecommendedRenderTargetSize(&recommendedWidth, &recommendedHeight);

    LUA->PushNumber(recommendedWidth);
    LUA->SetField(-2, "recommendedWidth");

    LUA->PushNumber(recommendedHeight);
    LUA->SetField(-2, "recommendedHeight");

    vr::HmdMatrix34_t eyeToHeadLeft = g_pSystem->GetEyeToHeadTransform(vr::Eye_Left);
    vr::HmdMatrix34_t eyeToHeadRight = g_pSystem->GetEyeToHeadTransform(vr::Eye_Right);
    Vector eyeToHeadTransformPos;
    eyeToHeadTransformPos.x = eyeToHeadLeft.m[0][3];
    eyeToHeadTransformPos.y = eyeToHeadLeft.m[1][3];
    eyeToHeadTransformPos.z = eyeToHeadLeft.m[2][3];
    LUA->PushVector(eyeToHeadTransformPos);
    LUA->SetField(-2, "eyeToHeadTransformPosLeft");

    eyeToHeadTransformPos.x = eyeToHeadRight.m[0][3];
    eyeToHeadTransformPos.y = eyeToHeadRight.m[1][3];
    eyeToHeadTransformPos.z = eyeToHeadRight.m[2][3];
    LUA->PushVector(eyeToHeadTransformPos);
    LUA->SetField(-2, "eyeToHeadTransformPosRight");

    return 1;
}

//*************************************************************************
//    Lua function: VRMOD_UpdatePosesAndActions()
//*************************************************************************
LUA_FUNCTION(VRMOD_UpdatePosesAndActions) {
    vr::VRCompositor()->WaitGetPoses(g_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
    g_pInput->UpdateActionState(g_activeActionSets, sizeof(vr::VRActiveActionSet_t), g_activeActionSetCount);
    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_GetPoses()
//    Returns: table
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
        else if (strcmp(g_actions[i].type, "pose") == 0) {
            g_pInput->GetPoseActionData(g_actions[i].handle, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
            pose = poseActionData.pose;
            strcpy_s(poseName, 64, g_actions[i].name);
        }
        else {
            continue;
        }
        //
        if (pose.bPoseIsValid) {

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
//    Lua function: VRMOD_GetActions()
//    Returns: table
//*************************************************************************
LUA_FUNCTION(VRMOD_GetActions) {
    vr::InputDigitalActionData_t digitalActionData;
    vr::InputAnalogActionData_t analogActionData;
    vr::VRSkeletalSummaryData_t skeletalSummaryData;

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
        else if (strcmp(g_actions[i].type, "skeleton") == 0) {
            g_pInput->GetSkeletalSummaryData(g_actions[i].handle, &skeletalSummaryData);
            LUA->CreateTable();
            LUA->CreateTable();
            for (int j = 0; j < 5; j++) {
                LUA->PushNumber(j + 1);
                LUA->PushNumber(skeletalSummaryData.flFingerCurl[j]);
                LUA->SetTable(-3);
            }
            LUA->SetField(-2, "fingerCurls");
            LUA->SetField(-2, g_actions[i].name);
        }
    }

    return 1;
}

//*************************************************************************
//    Lua function: VRMOD_ShareTextureBegin()
//*************************************************************************
LUA_FUNCTION(VRMOD_ShareTextureBegin) {
    HWND activeWindow = GetActiveWindow();
    if (activeWindow == NULL) {
        LUA->ThrowError("GetActiveWindow failed");
    }

    //hiding and restoring the game window is a workaround to d3d9 CreateDevice
    //failing on the second thread if the game is fullscreen
    ShowWindow(activeWindow, SW_HIDE);
    HANDLE thread = CreateThread(NULL, 0, FindCreateTexture, 0, 0, NULL);
    if (thread == NULL) {
        LUA->ThrowError("CreateThread failed");
    }
    WaitForSingleObject(thread, 1000);
    ShowWindow(activeWindow, SW_RESTORE);
    DWORD exitCode = 4;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    if (exitCode != 0) {
        if (exitCode == 1) {
            LUA->ThrowError("Direct3DCreate9 failed");
        }
        else if (exitCode == 2) {
            LUA->ThrowError("CreateWindowA failed");
        }
        else if (exitCode == 3) {
            LUA->ThrowError("CreateDevice failed");
        }
        else {
            LUA->ThrowError("GetExitCodeThread failed");
        }
    }

    if (g_CreateTextureAddr == NULL) {
        LUA->ThrowError("g_CreateTextureAddr is null");
    }

    g_CreateTextureOriginal = (CreateTexture)g_CreateTextureAddr;

    if (MH_Initialize() != MH_OK) {
        LUA->ThrowError("MH_Initialize failed");
    }

    if (MH_CreateHook((DWORD_PTR*)g_CreateTextureAddr, &CreateTextureHook, reinterpret_cast<void**>(&g_CreateTextureOriginal)) != MH_OK) {
        LUA->ThrowError("MH_CreateHook failed");
    }

    if (MH_EnableHook((DWORD_PTR*)g_CreateTextureAddr) != MH_OK) {
        LUA->ThrowError("MH_EnableHook failed");
    }

    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_ShareTextureFinish()
//*************************************************************************
LUA_FUNCTION(VRMOD_ShareTextureFinish) {
    if (g_sharedTexture == NULL) {
        LUA->ThrowError("g_sharedTexture is null");
    }

    if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &g_d3d11Device, NULL, NULL) != S_OK) {
        LUA->ThrowError("D3D11CreateDevice failed");
    }

    ID3D11Resource* res;
    if (FAILED(g_d3d11Device->OpenSharedResource(g_sharedTexture, __uuidof(ID3D11Resource), (void**)&res))) {
        LUA->ThrowError("OpenSharedResource failed");
    }

    if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_d3d11Texture))) {
        LUA->ThrowError("QueryInterface failed");
    }

    MH_DisableHook((DWORD_PTR*)g_CreateTextureAddr);
    MH_RemoveHook((DWORD_PTR*)g_CreateTextureAddr);
    if (MH_Uninitialize() != MH_OK) {
        LUA->ThrowError("MH_Uninitialize failed");
    }

    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_SubmitSharedTexture()
//*************************************************************************
LUA_FUNCTION(VRMOD_SubmitSharedTexture) {
    if (g_d3d11Texture == NULL)
        return 0;

    vr::Texture_t vrTexture = { g_d3d11Texture, vr::TextureType_DirectX, vr::ColorSpace_Auto };

    vr::VRTextureBounds_t textureBounds;

    //submit Left eye
    textureBounds.uMin = 0.0f + g_horizontalOffsetLeft * 0.25f;
    textureBounds.uMax = 0.5f + g_horizontalOffsetLeft * 0.25f;
    textureBounds.vMin = 0.0f - g_verticalOffsetLeft * 0.5f;
    textureBounds.vMax = 1.0f - g_verticalOffsetLeft * 0.5f;

    vr::VRCompositor()->Submit(vr::EVREye::Eye_Left, &vrTexture, &textureBounds);

    //submit Right eye
    textureBounds.uMin = 0.5f + g_horizontalOffsetRight * 0.25f;
    textureBounds.uMax = 1.0f + g_horizontalOffsetRight * 0.25f;
    textureBounds.vMin = 0.0f - g_verticalOffsetRight * 0.5f;
    textureBounds.vMax = 1.0f - g_verticalOffsetRight * 0.5f;

    vr::VRCompositor()->Submit(vr::EVREye::Eye_Right, &vrTexture, &textureBounds);

    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_Shutdown()
//*************************************************************************
LUA_FUNCTION(VRMOD_Shutdown) {
    if (g_pSystem != NULL) {
        vr::VR_Shutdown();
        g_pSystem = NULL;
    }
    if (g_d3d11Device != NULL) {
        g_d3d11Device->Release();
        g_d3d11Device = NULL;
    }
    g_d3d11Texture = NULL;
    g_sharedTexture = NULL;
    g_CreateTextureAddr = NULL;
    g_actionCount = 0;
    g_actionSetCount = 0;
    g_activeActionSetCount = 0;
    return 0;
}

//*************************************************************************
//    Lua function: VRMOD_TriggerHaptic(actionName, delay, duration, frequency, amplitude)
//*************************************************************************
LUA_FUNCTION(VRMOD_TriggerHaptic) {
    const char* actionName = LUA->CheckString(1);
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
//
//*************************************************************************
GMOD_MODULE_OPEN()
{

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_GetVersion");
    LUA->PushCFunction(VRMOD_GetVersion);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_IsHMDPresent");
    LUA->PushCFunction(VRMOD_IsHMDPresent);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_Init");
    LUA->PushCFunction(VRMOD_Init);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_SetActionManifest");
    LUA->PushCFunction(VRMOD_SetActionManifest);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_SetActiveActionSets");
    LUA->PushCFunction(VRMOD_SetActiveActionSets);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_GetViewParameters");
    LUA->PushCFunction(VRMOD_GetViewParameters);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_UpdatePosesAndActions");
    LUA->PushCFunction(VRMOD_UpdatePosesAndActions);
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
    LUA->PushString("VRMOD_ShareTextureBegin");
    LUA->PushCFunction(VRMOD_ShareTextureBegin);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_ShareTextureFinish");
    LUA->PushCFunction(VRMOD_ShareTextureFinish);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_SubmitSharedTexture");
    LUA->PushCFunction(VRMOD_SubmitSharedTexture);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_Shutdown");
    LUA->PushCFunction(VRMOD_Shutdown);
    LUA->SetTable(-3);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->PushString("VRMOD_TriggerHaptic");
    LUA->PushCFunction(VRMOD_TriggerHaptic);
    LUA->SetTable(-3);

    return 0;
}

GMOD_MODULE_CLOSE()
{
    return 0;
}
