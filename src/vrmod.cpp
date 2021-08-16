#include <gmod/Interface.h>
#include <openvr/openvr.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <d3d9.h>
#include <d3d11.h>
#define PATH_MAX MAX_PATH
#else
#include <GL/gl.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <unistd.h>
#endif

#define MAX_STR_LEN     256
#define MAX_ACTIONS     64
#define MAX_ACTIONSETS  16
#define PI_F            3.141592654f

enum EActionType{
    ActionType_Pose         = 439,
    ActionType_Vector1      = 708,
    ActionType_Vector2      = 709,
    ActionType_Boolean      = 736,
    ActionType_Skeleton     = 869,
    ActionType_Vibration    = 974,
};

enum ELuaRefIndex{
    LuaRefIndex_EmptyTable,
    LuaRefIndex_PoseTable,
    LuaRefIndex_HmdPose,
    LuaRefIndex_ActionTable,
    LuaRefIndex_Max,
};

typedef struct {
    vr::VRActionHandle_t handle;
    char fullname[MAX_STR_LEN];
    int luaRefs[2];
    char* name;
    int type;
}action;

typedef struct {
    vr::VRActionSetHandle_t handle;
    char name[MAX_STR_LEN];
}actionSet;

vr::IVRSystem*          g_pSystem = NULL;
vr::IVRInput*           g_pInput = NULL;
vr::TrackedDevicePose_t g_poses[vr::k_unMaxTrackedDeviceCount];
actionSet               g_actionSets[MAX_ACTIONSETS];
int                     g_actionSetCount = 0;
vr::VRActiveActionSet_t g_activeActionSets[MAX_ACTIONSETS];
int                     g_activeActionSetCount = 0;
action                  g_actions[MAX_ACTIONS];
int                     g_actionCount = 0;
char                    g_errorString[MAX_STR_LEN];
vr::VRTextureBounds_t   g_textureBoundsLeft;
vr::VRTextureBounds_t   g_textureBoundsRight;
vr::Texture_t           g_vrTexture;
int                     g_luaRefs[LuaRefIndex_Max];
int                     g_luaRefCount = 0;

char                    g_createTextureOrigBytes[14];
#ifdef _WIN32
typedef HRESULT         (APIENTRY* CreateTexture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
CreateTexture           g_createTexture = NULL;
ID3D11Device*           g_d3d11Device = NULL;
ID3D11Texture2D*        g_d3d11Texture = NULL;
HANDLE                  g_sharedTexture = NULL;
IDirect3DDevice9*       g_pD3D9Device = NULL;
typedef void*           (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
HRESULT APIENTRY CreateTextureHook(IDirect3DDevice9* pDevice, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** tex, HANDLE* shared_handle) {
    if (WriteProcessMemory(GetCurrentProcess(), g_createTexture, g_createTextureOrigBytes, 14, NULL) == 0)
        MessageBoxA(NULL, "WriteProcessMemory from hook failed", "", NULL);
    if (g_sharedTexture == NULL) {
        shared_handle = &g_sharedTexture;
        pool = D3DPOOL_DEFAULT;
    }
    return g_createTexture(pDevice, w, h, levels, usage, format, pool, tex, shared_handle);
};
#else
typedef struct{
    void ClearEntryPoints();
    uint64_t m_nTotalGLCycles, m_nTotalGLCalls;
    int unknown1;
    int unknown2; 
    int m_nOpenGLVersionMajor; 
    int m_nOpenGLVersionMinor;  
    int m_nOpenGLVersionPatch;
    bool m_bHave_OpenGL;
    char *m_pGLDriverStrings[4];
    int m_nDriverProvider;        
    void *firstFunc;
}COpenGLEntryPoints;
typedef void *(*GL_GetProcAddressCallbackFunc_t)(const char *, bool &, const bool, void *);
typedef COpenGLEntryPoints*(*GetOpenGLEntryPoints_t)(GL_GetProcAddressCallbackFunc_t callback);
typedef void            (*glGenTextures_t)(GLsizei n, GLuint *textures);
void*                   g_createTexture = NULL;
GLuint                  g_sharedTexture = GL_INVALID_VALUE;
COpenGLEntryPoints*     g_GL = NULL;
void CreateTextureHook(GLsizei n, GLuint *textures) {
    memcpy((void*)g_createTexture, (void*)g_createTextureOrigBytes, 14);
    ((glGenTextures_t)g_createTexture)(n, textures);
    g_sharedTexture = textures[0];
    return;
}
#endif

LUA_FUNCTION(GetVersion) {
    LUA->PushNumber(23);
    return 1;
}

LUA_FUNCTION(IsHMDPresent) {
    LUA->PushBool(vr::VR_IsHmdPresent());
    return 1;
}

LUA_FUNCTION(Init) {
    if (g_pSystem != NULL)
        LUA->ThrowError("Already initialized");
    vr::HmdError error = vr::VRInitError_None;
    g_pSystem = vr::VR_Init(&error, vr::VRApplication_Scene);
    if (error != vr::VRInitError_None) {
        snprintf(g_errorString, MAX_STR_LEN, "VR_Init failed: %s", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        LUA->ThrowError(g_errorString);
    }
    if (!vr::VRCompositor())
        LUA->ThrowError("VRCompositor failed");
    for(int i = 0; i < LuaRefIndex_Max; i++){
        LUA->CreateTable();
        g_luaRefs[i] = LUA->ReferenceCreate();
        g_luaRefCount++;
    }
#ifdef _WIN32
    HMODULE hMod = GetModuleHandleA("shaderapidx9.dll");
    if (hMod == NULL) LUA->ThrowError("GetModuleHandleA failed");
    CreateInterfaceFn CreateInterface = (CreateInterfaceFn)GetProcAddress(hMod, "CreateInterface");
    if (CreateInterface == NULL) LUA->ThrowError("GetProcAddress failed");
# ifdef _WIN64
    DWORD_PTR fnAddr = ((DWORD_PTR**)CreateInterface("ShaderDevice001", NULL))[0][5];
    g_pD3D9Device = *(IDirect3DDevice9**)(fnAddr + 8 + (*(DWORD_PTR*)(fnAddr + 3) & 0xFFFFFFFF));
# else
    g_pD3D9Device = **(IDirect3DDevice9***)(((DWORD_PTR**)CreateInterface("ShaderDevice001", NULL))[0][5] + 2);
# endif
    g_createTexture = ((CreateTexture**)g_pD3D9Device)[0][23];
#else
# ifdef __x86_64__
    void *lib = dlopen("libtogl_client.so", RTLD_NOW | RTLD_NOLOAD);
# else
    void *lib = dlopen("libtogl.so", RTLD_NOW | RTLD_NOLOAD);
# endif
    if(lib==NULL)
        LUA->ThrowError("dlopen fail");
    GetOpenGLEntryPoints_t GetOpenGLEntryPoints = (GetOpenGLEntryPoints_t)dlsym(lib, "GetOpenGLEntryPoints");
    if(GetOpenGLEntryPoints==NULL)
        LUA->ThrowError("dlsym fail");
    g_GL = GetOpenGLEntryPoints(NULL);
    dlclose(lib);
# ifdef __x86_64__
    g_createTexture = *((void**)&g_GL->firstFunc+50);
# else
    g_createTexture = *((void**)&g_GL->firstFunc+48);
# endif
#endif
    return 0;
}

LUA_FUNCTION(SetActionManifest) {
    const char* fileName = LUA->CheckString(1);
    char path[PATH_MAX];
    char currentDir[PATH_MAX];
#ifdef _WIN32
    GetCurrentDirectory(PATH_MAX, currentDir);
#else
    if(getcwd(currentDir, PATH_MAX) == NULL)
        LUA->ThrowError("getcwd failed");
#endif
    if (snprintf(path, PATH_MAX, "%s/garrysmod/data/%s", currentDir, fileName) >= PATH_MAX)
        LUA->ThrowError("SetActionManifest path too long");
    g_pInput = vr::VRInput();
    if (g_pInput->SetActionManifestPath(path) != vr::VRInputError_None)
        LUA->ThrowError("SetActionManifestPath failed");
    FILE* file = fopen(path, "r");
    if (file == NULL)
        LUA->ThrowError("failed to open action manifest");
    memset(g_actions, 0, sizeof(g_actions));
    char word[MAX_STR_LEN];
    char fmt1[MAX_STR_LEN], fmt2[MAX_STR_LEN];
    snprintf(fmt1, MAX_STR_LEN, "%%*[^\"]\"%%%i[^\"]\"", MAX_STR_LEN-1);
    snprintf(fmt2, MAX_STR_LEN, "%%%i[^\"]\"", MAX_STR_LEN-1);
    while (fscanf(file, fmt1, word) == 1 && strcmp(word, "actions") != 0);
    while (fscanf(file, fmt2, word) == 1) {
        if (strchr(word, ']') != nullptr)
            break;
        if (strcmp(word, "name") == 0) {
            if (fscanf(file, fmt1, g_actions[g_actionCount].fullname) != 1)
                break;
            g_actions[g_actionCount].name = g_actions[g_actionCount].fullname;
            for (unsigned int i = 0; i < strlen(g_actions[g_actionCount].fullname); i++) {
                if (g_actions[g_actionCount].fullname[i] == '/')
                    g_actions[g_actionCount].name = g_actions[g_actionCount].fullname + i + 1;
            }
            g_pInput->GetActionHandle(g_actions[g_actionCount].fullname, &(g_actions[g_actionCount].handle));
        }
        if (strcmp(word, "type") == 0) {
            char typeStr[MAX_STR_LEN] = {0};
            if (fscanf(file, fmt1, typeStr) != 1)
                break;
            for (int i = 0; typeStr[i]; i++)
                g_actions[g_actionCount].type += typeStr[i];
        }
        if (g_actions[g_actionCount].fullname[0] && g_actions[g_actionCount].type) {
            for(int i = 0; i < 2; i++){
                LUA->CreateTable();
                g_actions[g_actionCount].luaRefs[i] = LUA->ReferenceCreate();
            }
            g_actionCount++;
            if (g_actionCount == MAX_ACTIONS)
                break;
        }
    }
    fclose(file);
    return 0;
}

LUA_FUNCTION(SetActiveActionSets) {
    g_activeActionSetCount = 0;
    for (int i = 0; i < MAX_ACTIONSETS; i++) {
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

void PushMatrixAsTable(GarrysMod::Lua::ILuaBase* LUA, float* mtx, unsigned int rows, unsigned int cols) {
    LUA->CreateTable();
    for (unsigned int row = 0; row < rows; row++) {
        LUA->PushNumber(row + 1);
        LUA->CreateTable();
        for (unsigned int col = 0; col < cols; col++) {
            LUA->PushNumber(col+1);
            LUA->PushNumber(mtx[row * cols + col]);
            LUA->SetTable(-3);
        }
        LUA->SetTable(-3);
    }
}

LUA_FUNCTION(GetDisplayInfo) {
    float fNearZ = (float)LUA->CheckNumber(1);
    float fFarZ = (float)LUA->CheckNumber(2);
    uint32_t recommendedWidth = 0;
    uint32_t recommendedHeight = 0;
    g_pSystem->GetRecommendedRenderTargetSize(&recommendedWidth, &recommendedHeight);
    vr::HmdMatrix44_t projLeft = g_pSystem->GetProjectionMatrix(vr::Hmd_Eye::Eye_Left, fNearZ, fFarZ);
    vr::HmdMatrix44_t projRight = g_pSystem->GetProjectionMatrix(vr::Hmd_Eye::Eye_Right, fNearZ, fFarZ);
    vr::HmdMatrix34_t transformLeft = g_pSystem->GetEyeToHeadTransform(vr::Eye_Left);
    vr::HmdMatrix34_t transformRight = g_pSystem->GetEyeToHeadTransform(vr::Eye_Right);
    LUA->CreateTable();
    PushMatrixAsTable(LUA, (float*)&projLeft, 4, 4);
    LUA->SetField(-2, "ProjectionLeft");
    PushMatrixAsTable(LUA, (float*)&projRight, 4, 4);
    LUA->SetField(-2, "ProjectionRight");
    PushMatrixAsTable(LUA, (float*)&transformLeft, 3, 4);
    LUA->SetField(-2, "TransformLeft");
    PushMatrixAsTable(LUA, (float*)&transformRight, 3, 4);
    LUA->SetField(-2, "TransformRight");
    LUA->PushNumber(recommendedWidth);
    LUA->SetField(-2, "RecommendedWidth");
    LUA->PushNumber(recommendedHeight);
    LUA->SetField(-2, "RecommendedHeight");
    return 1;
}

LUA_FUNCTION(UpdatePosesAndActions) {
    vr::VRCompositor()->WaitGetPoses(g_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
    g_pInput->UpdateActionState(g_activeActionSets, sizeof(vr::VRActiveActionSet_t), g_activeActionSetCount);
    return 0;
}

LUA_FUNCTION(GetPoses) {
    vr::InputPoseActionData_t poseActionData;
    vr::TrackedDevicePose_t pose = g_poses[0];
    char* poseName = (char*)"hmd";
    int poseRef = g_luaRefs[LuaRefIndex_HmdPose];
    LUA->ReferencePush(g_luaRefs[LuaRefIndex_PoseTable]);
    for (int i = -1; i < g_actionCount; i++) {
        if (i != -1){
            if (g_actions[i].type == ActionType_Pose) {
                g_pInput->GetPoseActionData(g_actions[i].handle, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
                pose = poseActionData.pose;
                poseName = g_actions[i].name;
                poseRef = g_actions[i].luaRefs[0];
            } else continue;
        }
        if (pose.bPoseIsValid) {
            vr::HmdMatrix34_t mat = pose.mDeviceToAbsoluteTracking;
            Vector pos;
            Vector vel;
            QAngle ang;
            QAngle angvel;
            pos.x = -mat.m[2][3];
            pos.y = -mat.m[0][3];
            pos.z = mat.m[1][3];
            ang.x = asinf(mat.m[1][2]) * (180.0f / PI_F);
            ang.y = atan2f(mat.m[0][2], mat.m[2][2]) * (180.0f / PI_F);
            ang.z = atan2f(-mat.m[1][0], mat.m[1][1]) * (180.0f / PI_F);
            vel.x = -pose.vVelocity.v[2];
            vel.y = -pose.vVelocity.v[0];
            vel.z = pose.vVelocity.v[1];
            angvel.x = -pose.vAngularVelocity.v[2] * (180.0f / PI_F);
            angvel.y = -pose.vAngularVelocity.v[0] * (180.0f / PI_F);
            angvel.z = pose.vAngularVelocity.v[1] * (180.0f / PI_F);
            LUA->ReferencePush(poseRef);
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

LUA_FUNCTION(GetActions) {
    vr::InputDigitalActionData_t digitalActionData;
    vr::InputAnalogActionData_t analogActionData;
    vr::VRSkeletalSummaryData_t skeletalSummaryData;
    char* changedActionNames[MAX_ACTIONS];
    bool changedActionStates[MAX_ACTIONS];
    int changedActionCount = 0;
    LUA->ReferencePush(g_luaRefs[LuaRefIndex_ActionTable]);
    for (int i = 0; i < g_actionCount; i++) {
        if (g_actions[i].type == ActionType_Boolean) {
            LUA->PushBool((g_pInput->GetDigitalActionData(g_actions[i].handle, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState));
            LUA->SetField(-2, g_actions[i].name);
            if(digitalActionData.bChanged){
                changedActionNames[changedActionCount] = g_actions[i].name;
                changedActionStates[changedActionCount] = digitalActionData.bState;
                changedActionCount++;
            }
        }
        else if (g_actions[i].type == ActionType_Vector1) {
            g_pInput->GetAnalogActionData(g_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
            LUA->PushNumber(analogActionData.x);
            LUA->SetField(-2, g_actions[i].name);
        }
        else if (g_actions[i].type == ActionType_Vector2) {
            LUA->ReferencePush(g_actions[i].luaRefs[0]);
            g_pInput->GetAnalogActionData(g_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
            LUA->PushNumber(analogActionData.x);
            LUA->SetField(-2, "x");
            LUA->PushNumber(analogActionData.y);
            LUA->SetField(-2, "y");
            LUA->SetField(-2, g_actions[i].name);
        }
        else if (g_actions[i].type == ActionType_Skeleton) {
            g_pInput->GetSkeletalSummaryData(g_actions[i].handle, &skeletalSummaryData);
            LUA->ReferencePush(g_actions[i].luaRefs[0]);
            LUA->ReferencePush(g_actions[i].luaRefs[1]);
            for (int j = 0; j < 5; j++) {
                LUA->PushNumber(j + 1);
                LUA->PushNumber(skeletalSummaryData.flFingerCurl[j]);
                LUA->SetTable(-3);
            }
            LUA->SetField(-2, "fingerCurls");
            LUA->SetField(-2, g_actions[i].name);
        }
    }
    if (changedActionCount == 0){
        LUA->ReferencePush(g_luaRefs[LuaRefIndex_EmptyTable]);
    }else{
        LUA->CreateTable();
        for(int i = 0; i < changedActionCount; i++){
            LUA->PushBool(changedActionStates[i]);
            LUA->SetField(-2,changedActionNames[i]);
        }
    }
    return 2;
}

LUA_FUNCTION(ShareTextureBegin) {
    char patch[] = "\x68\x0\x0\x0\x0\xC3\x44\x24\x04\x0\x0\x0\x0\xC3";
    *(uint32_t*)(patch + 1) = (uint32_t)((uintptr_t)CreateTextureHook);
#if defined _WIN64 || defined __x86_64__
    patch[5] = '\xC7';
    *(uint32_t*)(patch + 9) = (uint32_t)((uintptr_t)CreateTextureHook >> 32);
#endif
#ifdef _WIN32
    if (ReadProcessMemory(GetCurrentProcess(), g_createTexture, g_createTextureOrigBytes, 14, NULL) == 0)
        LUA->ThrowError("ReadProcessMemory failed");
    if (WriteProcessMemory(GetCurrentProcess(), g_createTexture, patch, 14, NULL) == 0)
        LUA->ThrowError("WriteProcessMemory failed");
#else
    uintptr_t alignedAddr = (uintptr_t)g_createTexture & ~(getpagesize()-1);
    if(mprotect((void*)alignedAddr, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
        LUA->ThrowError("mprotect fail");
    memcpy((void*)g_createTextureOrigBytes, (void*)g_createTexture, 14);
    memcpy((void*)g_createTexture, (void*)patch, 14);
#endif
    return 0;
}

LUA_FUNCTION(ShareTextureFinish) {
#ifdef _WIN32
    if (g_sharedTexture == NULL)
        LUA->ThrowError("g_sharedTexture is null");
    if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &g_d3d11Device, NULL, NULL) != S_OK)
        LUA->ThrowError("D3D11CreateDevice failed");
    ID3D11Resource* res;
    if (FAILED(g_d3d11Device->OpenSharedResource(g_sharedTexture, __uuidof(ID3D11Resource), (void**)&res)))
        LUA->ThrowError("OpenSharedResource failed");
    if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_d3d11Texture)))
        LUA->ThrowError("QueryInterface failed");
    g_vrTexture.handle = g_d3d11Texture;
    g_vrTexture.eType = vr::TextureType_DirectX;
#else
    if (g_sharedTexture == GL_INVALID_VALUE)
        LUA->ThrowError("g_sharedTexture is invalid");
    g_vrTexture.handle = (void*)(uintptr_t)g_sharedTexture;
    g_vrTexture.eType = vr::TextureType_OpenGL;
#endif
    g_vrTexture.eColorSpace = vr::ColorSpace_Auto;
    return 0;
}

LUA_FUNCTION(SetSubmitTextureBounds) {
    g_textureBoundsLeft.uMin = (float)LUA->CheckNumber(1);
    g_textureBoundsLeft.vMin = (float)LUA->CheckNumber(2);
    g_textureBoundsLeft.uMax = (float)LUA->CheckNumber(3);
    g_textureBoundsLeft.vMax = (float)LUA->CheckNumber(4);
    g_textureBoundsRight.uMin = (float)LUA->CheckNumber(5);
    g_textureBoundsRight.vMin = (float)LUA->CheckNumber(6);
    g_textureBoundsRight.uMax = (float)LUA->CheckNumber(7);
    g_textureBoundsRight.vMax = (float)LUA->CheckNumber(8);
    return 0;
}

LUA_FUNCTION(SubmitSharedTexture) {
#ifdef _WIN32
    if (g_d3d11Texture == NULL)
        return 0;
    IDirect3DQuery9* pEventQuery = nullptr;
    g_pD3D9Device->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);
    if (pEventQuery != nullptr)
    {
        pEventQuery->Issue(D3DISSUE_END);
        while (pEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK);
        pEventQuery->Release();
    }
#endif
    vr::VRCompositor()->Submit(vr::EVREye::Eye_Left, &g_vrTexture, &g_textureBoundsLeft);
    vr::VRCompositor()->Submit(vr::EVREye::Eye_Right, &g_vrTexture, &g_textureBoundsRight);
    return 0;
}

LUA_FUNCTION(Shutdown) {
    if (g_pSystem != NULL) {
        vr::VR_Shutdown();
        g_pSystem = NULL;
    }
    for(int i = 0; i < g_luaRefCount; i++)
        LUA->ReferenceFree(g_luaRefs[i]);
    g_luaRefCount = 0;
    for(int i = 0; i < g_actionCount; i++)
        for(int j = 0; j < 2; j++)
            LUA->ReferenceFree(g_actions[i].luaRefs[j]);
    g_actionCount = 0;
    g_actionSetCount = 0;
    g_activeActionSetCount = 0;
#ifdef _WIN32
    if (g_d3d11Device != NULL) {
        g_d3d11Device->Release();
        g_d3d11Device = NULL;
    }
    g_d3d11Texture = NULL;
    g_pD3D9Device = NULL;
    g_sharedTexture = NULL;
#else
    g_sharedTexture = GL_INVALID_VALUE;
#endif
    return 0;
}

LUA_FUNCTION(TriggerHaptic) {
    const char* actionName = LUA->CheckString(1);
    for (int i = 0; i < g_actionCount; i++) {
        if (strcmp(g_actions[i].name, actionName) == 0) {
            g_pInput->TriggerHapticVibrationAction(g_actions[i].handle, (float)LUA->CheckNumber(2), (float)LUA->CheckNumber(3), (float)LUA->CheckNumber(4), (float)LUA->CheckNumber(5), vr::k_ulInvalidInputValueHandle);
            break;
        }
    }
    return 0;
}

LUA_FUNCTION(GetTrackedDeviceNames) {
    LUA->CreateTable();
    int tableIndex = 1;
    char name[MAX_STR_LEN];
    for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
        if (g_pSystem->GetStringTrackedDeviceProperty(i, vr::Prop_ControllerType_String, name, MAX_STR_LEN) > 1) {
            LUA->PushNumber(tableIndex);
            LUA->PushString(name);
            LUA->SetTable(-3);
            tableIndex++;
        }
    }
    return 1;
}

LUA_FUNCTION(RunInstaller) {
#ifdef _WIN32
    HWND activeWindow = GetActiveWindow();
    char path[PATH_MAX];
    char currentDir[PATH_MAX];
    GetCurrentDirectory(PATH_MAX, currentDir);
    snprintf(path, PATH_MAX, "%s\\vrmod_installer.bat", currentDir);
    snprintf(g_errorString, MAX_STR_LEN, "Run %s ?", path);
    if(MessageBoxA(activeWindow, g_errorString, "VRMod", MB_YESNO) == IDYES){
        ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOW);
        if(activeWindow != NULL)
            ShowWindow(activeWindow, SW_MINIMIZE);
    }
#endif
    return 0;
}

GMOD_MODULE_OPEN(){
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "vrmod");
    if (!LUA->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
        LUA->Pop(1);
        LUA->CreateTable();
    }
    LUA->PushCFunction(GetVersion);
    LUA->SetField(-2, "GetVersion");
    LUA->PushCFunction(IsHMDPresent);
    LUA->SetField(-2, "IsHMDPresent");
    LUA->PushCFunction(Init);
    LUA->SetField(-2, "Init");
    LUA->PushCFunction(SetActionManifest);
    LUA->SetField(-2, "SetActionManifest");
    LUA->PushCFunction(SetActiveActionSets);
    LUA->SetField(-2, "SetActiveActionSets");
    LUA->PushCFunction(GetDisplayInfo);
    LUA->SetField(-2, "GetDisplayInfo");
    LUA->PushCFunction(UpdatePosesAndActions);
    LUA->SetField(-2, "UpdatePosesAndActions");
    LUA->PushCFunction(GetPoses);
    LUA->SetField(-2, "GetPoses");
    LUA->PushCFunction(GetActions);
    LUA->SetField(-2, "GetActions");
    LUA->PushCFunction(ShareTextureBegin);
    LUA->SetField(-2, "ShareTextureBegin");
    LUA->PushCFunction(ShareTextureFinish);
    LUA->SetField(-2, "ShareTextureFinish");
    LUA->PushCFunction(SetSubmitTextureBounds);
    LUA->SetField(-2, "SetSubmitTextureBounds");
    LUA->PushCFunction(SubmitSharedTexture);
    LUA->SetField(-2, "SubmitSharedTexture");
    LUA->PushCFunction(Shutdown);
    LUA->SetField(-2, "Shutdown");
    LUA->PushCFunction(TriggerHaptic);
    LUA->SetField(-2, "TriggerHaptic");
    LUA->PushCFunction(GetTrackedDeviceNames);
    LUA->SetField(-2, "GetTrackedDeviceNames");
    LUA->PushCFunction(RunInstaller);
    LUA->SetField(-2, "RunInstaller");
    LUA->SetField(-2, "vrmod");
    return 0;
}

GMOD_MODULE_CLOSE(){
    return 0;
}
