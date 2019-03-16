#include "GarrysMod/Lua/Interface.h"
#include <stdio.h>
#include <Windows.h>
#include <gl/GL.h>
#include "GL/glext.h"
#include "GL/wglext.h"
#include <openvr.h>

#pragma comment(lib, "opengl32.lib")

//functions to be loaded from dlls
typedef void(*msg)(char const* pMsg, ...);
msg Msg = nullptr;

//*************************************************************************
//                             gl extensions
//*************************************************************************
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLDETACHSHADERPROC glDetachShader;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1IPROC glUniform1i;

void loadExtensions() {
	glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
	glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
	glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
	glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
	glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
	glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
	glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
	glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
	glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
	glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
	glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
	glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
	glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
	glDetachShader = (PFNGLDETACHSHADERPROC)wglGetProcAddress("glDetachShader");
	glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
	glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
	glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)wglGetProcAddress("glFramebufferTexture");
	glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
	glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
	glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
}

//*************************************************************************
//                             gl helper functions
//*************************************************************************
int compileShader(GLuint shader, const GLchar* source) {
	GLint success = 0;
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE) {
		GLint logSize = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
		GLchar* log = (GLchar*)malloc(logSize);
		glGetShaderInfoLog(shader, logSize, NULL, log);
		Msg(log);
		free(log);
		return -1;
	}
	return 0;
}

int createShaderProgram(GLuint* shaderProgram, int shaderCount, ...) {
	GLuint* shaders = (GLuint*)malloc(shaderCount * sizeof(GLuint));
	va_list ap;
	va_start(ap, shaderCount);
	for (int i = 0; i < shaderCount; i++) {
		shaders[i] = glCreateShader(va_arg(ap, GLenum));
		if (compileShader(shaders[i], va_arg(ap, const GLchar*)) != 0)
			return -1;
	}
	va_end(ap);
	*shaderProgram = glCreateProgram();
	for (int i = 0; i < shaderCount; i++)
		glAttachShader(*shaderProgram, shaders[i]);
	glLinkProgram(*shaderProgram);
	GLint isLinked = 0;
	glGetProgramiv(*shaderProgram, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE) {
		GLint maxLength = 0;
		glGetProgramiv(*shaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
		GLchar* log = (GLchar*)malloc(maxLength);
		glGetProgramInfoLog(*shaderProgram, maxLength, NULL, log);
		Msg(log);
		free(log);
		return -1;
	}
	for (int i = 0; i < shaderCount; i++) {
		glDetachShader(*shaderProgram, shaders[i]);
		glDeleteShader(shaders[i]);
	}
	free(shaders);
	return 0;
}

GLuint createTexture(GLsizei width, GLsizei height, const GLvoid* data) {
	GLuint textureObjects[1];
	glGenTextures(1, textureObjects);
	glBindTexture(GL_TEXTURE_2D, textureObjects[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return textureObjects[0];
}

//*************************************************************************
//                             shader sources
//*************************************************************************

const char vertexShaderSource[] =
"#version 330 core\n"
"in vec3 vertexPosition;"
"in vec2 vertexUV;"
"uniform float xoffset;"
"out vec2 UV;"
"void main()"
"{"
"	gl_Position = vec4(vertexPosition.x + xoffset, vertexPosition.y, vertexPosition.z, 1);"
"	UV = vertexUV;"
"}";

const char fragmentShaderSource[] =
"#version 330 core\n"
"in vec2 UV;"
"uniform sampler2D textureSampler;"
"out vec4 color;"
"void main()"
"{"
"	color = texture(textureSampler, UV).rgba;"
"}";

//*************************************************************************
//                             other globals
//*************************************************************************

//openvr related
vr::IVRSystem * pSystem = NULL;
vr::IVRInput * pInput = NULL;
vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];

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

//gl related
GLuint vertexArrayObjects[2];
GLuint textureFull;
GLuint textureLeft;
GLuint textureRight;
GLuint xOffsetUniformLoc;

//capture related
HDC gameWindowHDC;
HDC bitmapHDC;
void* bitmapData;

//other
HDC GLWindowDC = NULL;
int mirrorInitialized = 0;
int gameWindowWidth = 0;
int gameWindowHeight = 0;
float horizontalFOV = 0;
float horizontalOffset = 0;
float calculatedHorizontalOffset = 0;
uint32_t recommendedWidth = 0;
uint32_t recommendedHeight = 0;

//*************************************************************************
//                             mirrorInit
//*************************************************************************

void mirrorInit() {
	if (mirrorInitialized)
		return;

	//create window for gl context
	HINSTANCE hInstance = GetModuleHandle(0);
	WNDCLASS wc = {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = DefWindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "vr_mirror";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClass(&wc);
	HWND windowHandle = CreateWindow("vr_mirror", "vr_mirror", WS_BORDER | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 512, 512, NULL, NULL, hInstance, NULL);
	if (windowHandle == NULL) {
		Msg("Error: CreateWindow\n");
		return;
	}
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,
		8,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};
	GLWindowDC = GetDC(windowHandle);
	SetPixelFormat(GLWindowDC, ChoosePixelFormat(GLWindowDC, &pfd), &pfd);

	//create gl context
	HGLRC GLContext = wglCreateContext(GLWindowDC);
	wglMakeCurrent(GLWindowDC, GLContext);

	//load gl extensions
	loadExtensions();

	//create and compile shaders
	GLuint shaderProgram;
	if (createShaderProgram(&shaderProgram, 2, GL_VERTEX_SHADER, vertexShaderSource, GL_FRAGMENT_SHADER, fragmentShaderSource) != 0) {
		Msg("Error: createShaderProgram\n");
		return;
	}
	glUseProgram(shaderProgram);

	xOffsetUniformLoc = glGetUniformLocation(shaderProgram, "xoffset");

	//create vaos+vbos for left/right eye quads
	float vertexData[] =
	{
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f
	};
	float uvDataLeft[] =
	{
		0.0f, 0.0f,
		0.5f, 0.0f,
		0.5f, 1.0f,
		0.5f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f
	};
	float uvDataRight[] =
	{
		0.5f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 1.0f,
		0.5f, 1.0f,
		0.5f, 0.0f
	};

	glGenVertexArrays(2, vertexArrayObjects);
	GLuint buffers[3];
	glGenBuffers(3, buffers);
	//set up left quad
	glBindVertexArray(vertexArrayObjects[0]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvDataLeft), uvDataLeft, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
	//set up right quad
	glBindVertexArray(vertexArrayObjects[1]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvDataRight), uvDataRight, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

	//find gmod window, set up stuff for capturing with BitBlt
	HWND hwnd = FindWindow(NULL, "Garry's Mod");
	if (hwnd == NULL) {
		Msg("Failed to find game window\n");
		return;
	}
	RECT rect;
	if (GetClientRect(hwnd, &rect) == 0) {
		Msg("Error: GetClientRect\n");
		return;
	}
	gameWindowWidth = rect.right;
	gameWindowHeight = rect.bottom;
	gameWindowHDC = GetDC(hwnd);
	if (gameWindowHDC == NULL) {
		Msg("Error: game window GetDC\n");
		return;
	}
	bitmapHDC = CreateCompatibleDC(gameWindowHDC);
	BITMAPINFOHEADER bmi = { 0 };
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biPlanes = 1;
	bmi.biBitCount = 32;
	bmi.biWidth = gameWindowWidth;
	bmi.biHeight = gameWindowHeight;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = 0;
	HBITMAP bmp = CreateDIBSection(gameWindowHDC, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &bitmapData, NULL, 0);
	SelectObject(bitmapHDC, bmp);

	//create textures
	textureFull = createTexture(gameWindowWidth, gameWindowHeight, NULL);
	textureLeft = createTexture(gameWindowWidth / 2, gameWindowHeight, NULL);
	textureRight = createTexture(gameWindowWidth / 2, gameWindowHeight, NULL);

	//create a second framebuffer for RT
	GLuint frameBufferObjects[1];
	glGenFramebuffers(1, frameBufferObjects);
	//make it active
	glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObjects[0]);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	//set viewport to half width because were using this to render to left/right eye textures only
	glViewport(0, 0, gameWindowWidth / 2, gameWindowHeight);

	mirrorInitialized = 1;
	return;
}

//*************************************************************************
//                           LUA VRMOD_MirrorFrame
//*************************************************************************
LUA_FUNCTION(VRMOD_MirrorFrame) {
	if (!mirrorInitialized)
		return 0;
	//capture game window to bitmap
	BitBlt(bitmapHDC, 0, 0, gameWindowWidth, gameWindowHeight, gameWindowHDC, 0, 0, SRCCOPY);

	//copy bitmap data to gl texture
	glBindTexture(GL_TEXTURE_2D, textureFull);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gameWindowWidth, gameWindowHeight, GL_BGRA, GL_UNSIGNED_BYTE, (unsigned char*)bitmapData);

	//update left eye texture
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureLeft, 0);
	glUniform1f(xOffsetUniformLoc, horizontalOffset);
	glBindVertexArray(vertexArrayObjects[0]);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	//update right eye texture
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureRight, 0);
	glUniform1f(xOffsetUniformLoc, -horizontalOffset);
	glBindVertexArray(vertexArrayObjects[1]);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	//submit textures to vr compositor
	vr::Texture_t leftEyeTexture = { (void*)(uintptr_t)textureLeft, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
	vr::Texture_t rightEyeTexture = { (void*)(uintptr_t)textureRight, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);


	//following is from the openvr opengl example, seems to reduce jitter
	glFinish();

	SwapBuffers(GLWindowDC);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glFlush();
	glFinish();
	//

	return 0;
}

//*************************************************************************
//                             LUA VRMOD_Init
//*************************************************************************
LUA_FUNCTION(VRMOD_Init) {

	vr::HmdError error = vr::VRInitError_None;

	Msg("VR_Init... ");
	pSystem = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None) {
		LUA->PushNumber(-1);
		return 1;
	}
	Msg("OK\n");

	Msg("VRCompositor... ");
	if (!vr::VRCompositor()) {
		Msg("error\n");
		LUA->PushNumber(-1);
		return 1;
	}
	Msg("OK\n");

	Msg("MirrorInit... ");
	mirrorInit();
	if (!mirrorInitialized) {
		LUA->PushNumber(-1);
		return 1;
	}
	Msg("OK\n");

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

	//prepare action related stuff
	char dir[256];
	GetCurrentDirectory(256, dir);
	char path[256];
	sprintf_s(path, 256, "%s\\garrysmod\\lua\\bin\\vrmod_action_manifest.json", dir);

	pInput = vr::VRInput();
	if (pInput->SetActionManifestPath(path) != vr::VRInputError_None) {
		Msg("Error: vrmod_action_manifest.json not found!");
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
	LUA->PushNumber(2);
	return 1;
}

//*************************************************************************
//
//*************************************************************************
GMOD_MODULE_OPEN()
{
	HMODULE tier0 = GetModuleHandle("tier0.dll");
	Msg = (msg)GetProcAddress(tier0, "Msg");

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
