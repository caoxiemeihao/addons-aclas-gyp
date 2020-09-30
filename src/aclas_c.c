#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <node_api.h>

#define CHECK(expr) \
  { \
    if ((expr) == 0) { \
      fprintf(stderr, "%s:%d: failed assertion `%s'\n", __FILE__, __LINE__, #expr); \
      fflush(stderr); \
      abort(); \
    } \
  }

typedef struct {
	napi_env env;
	napi_value fn;
	napi_value json;
} NapiCall;

char json_extra[240];

void Stdout2Nodejs(INT32 code, INT32 index, INT32 total) {
	char* str = "##{";
	str = strcat(str, "\"code\":");
	char* str_code = strcat("\"code\":", code);
	str = strcat(str, str_code);
	char* str_index = strcat("\"index\":", index);
	str = strcat(str, str_index);
	char* str_total = strcat("\"total\":", total);
	str = strcat(str, str_total);
	char* str_extra = strcat("\"extra\":\"", json_extra);
	str_extra = strcat(str_extra, "\"");
	str = strcat(str, str_extra);

	printf("%s\n", str);
}

// -----------------------------------------------
typedef struct AclasDevice {
	UINT32  ProtocolType;
	UINT32  Addr;
	UINT32  Port; // 5002
	UCHAR   name[16];
	UINT32  ID;
	UINT32  Version;
	BYTE    Country;
	BYTE    DepartmentID;
	BYTE    KeyType;
	UINT64  PrinterDot;
	LONG64  PrnStartDate;
	UINT32  LabelPage;
	UINT32  PrinterNo;
	USHORT  PLUStorage;
	USHORT  HotKeyCount;
	USHORT  NutritionStorage;
	USHORT  DiscountStorage;
	USHORT  Note1Storage;
	USHORT  Note2Storage;
	USHORT  Note3Storage;
	USHORT  Note4Storage;
	BYTE    stroge[177];
} DeviceInfo;

typedef bool (CALLBACK* pAclasSDKInitialize)(char* s);
typedef bool (CALLBACK* pGetDevicesInfo)(UINT32 Addr, UINT32 Port, UINT32 ProtocolType, DeviceInfo* info);
typedef void (WINAPI* FP)(UINT32 Errorcode, UINT32 index, UINT32 Total, NapiCall* call);
typedef HANDLE(CALLBACK* pAclasSDKExecTask)(UINT32 Addr, UINT32 Port, UINT32 ProtocolType, UINT32 ProceType, UINT32 DataType, char* FileName, FP fp, NapiCall* call);
typedef HANDLE(CALLBACK* pAclasSDKWaitForTask)(HANDLE handle);
typedef int(CALLBACK* pAclasSDKSyncExecTask)(char* Addr, UINT32 Port, UINT32 ProtocolType, UINT32 ProceType, UINT32 DataType, char* FileName);

void WINAPI onprogress(UINT32 Errorcode, UINT32 Index, UINT32 Total, NapiCall* call) {
	// Fatal error in HandleScope::HandleScope
	// Entering the V8 API without proper locking in place
	// call->fn.Call(call->env.Global(), { call->json });

	Stdout2Nodejs(Errorcode, Index, Total);

	switch (Errorcode) {
	case 0x0000:
		// std::cout << "complete" << std::endl;
		break;
	case 0x0001:
		// std::cout << Index << "/" << Total << std::endl;
		break;
	}
}

UINT MakeHost2Dword(char* host) {
	UINT result;
	UINT a[4];
	char* p1 = NULL;
	char str[20];
	strcpy(str, host);
	p1 = strtok(str, ".");
	a[0] = atoi(p1);
	result = a[0] << 24;
	for (int i = 1; i < 4; i++)
	{
		p1 = strtok(NULL, ".");
		a[i] = atoi(p1);
		result += a[i] << ((3 - i) * 8);
	}
	return result;
}

void Start(char* DllPath, char* Host, UINT32 ProceType, char* FileName, NapiCall* call) {
	HMODULE hModule = LoadLibrary(DllPath/* "AclasSDK.dll" */);

	if (!hModule) return;

	// Initialize
	pAclasSDKInitialize Initialize = (pAclasSDKInitialize)GetProcAddress(hModule, "AclasSDK_Initialize");
	char* str = NULL;
	BOOL sta = Initialize(str);

	if (sta) {
		printf("%s\n", "Initialize success");
	}
	else {
		printf("%s\n", "Initialize failed");
		return;
	}

	// Get Device Information
	pGetDevicesInfo getDevicesInfo = (pGetDevicesInfo)GetProcAddress(hModule, "AclasSDK_GetDevicesInfo");
	DeviceInfo* info = (DeviceInfo*)malloc(sizeof(DeviceInfo));
	UINT addr = MakeHost2Dword(Host); // "192.168.1.2"
	BOOL ref = getDevicesInfo(addr, 0, 0, info);

	printf("%s\n", info->name);

	pAclasSDKExecTask execTask = (pAclasSDKExecTask)GetProcAddress(hModule, "AclasSDK_ExecTaskA");
	pAclasSDKWaitForTask waitForTask = (pAclasSDKWaitForTask)GetProcAddress(hModule, "AclasSDK_WaitForTask");
	HANDLE handle = waitForTask(execTask(addr, info->Port, info->ProtocolType, 0, ProceType, FileName, onprogress, call));

	// Release resource
	GetProcAddress(hModule, "AclasSDK_Finalize");
}
// -----------------------------------------------

void runTask(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2];

	CHECK(napi_ok == napi_get_cb_info(env, info, &argc, args, NULL, NULL)); // 取出两个入参 json、callback
	
	napi_value json = args[0]; // 下发电子称参数
	napi_value callback = args[1]; // 下发电子称实时信息回调(因为 onprogress 异步的原因，现在通讯用 stdout)

	napi_value host_key, host;
	napi_value type_key, type;
	napi_value filename_key, filename;
	napi_value dllPath_key, dllPath;
	napi_value extra_key, extra;

	CHECK(napi_ok == napi_create_string_utf8(env, "host", NAPI_AUTO_LENGTH, host_key));
	CHECK(napi_ok == napi_create_string_utf8(env, "type", NAPI_AUTO_LENGTH, type_key));
	CHECK(napi_ok == napi_create_string_utf8(env, "filename", NAPI_AUTO_LENGTH, filename_key));
	CHECK(napi_ok == napi_create_string_utf8(env, "dll_path", NAPI_AUTO_LENGTH, dllPath_key));
	CHECK(napi_ok == napi_create_string_utf8(env, "extra", NAPI_AUTO_LENGTH, extra_key));

	CHECK(napi_ok == napi_get_property(env, json, host_key, host));
	CHECK(napi_ok == napi_get_property(env, json, type_key, type));
	CHECK(napi_ok == napi_get_property(env, json, filename_key, filename));
	CHECK(napi_ok == napi_get_property(env, json, dllPath_key, dllPath));
	CHECK(napi_ok == napi_get_property(env, json, extra_key, extra));

	char DllPath[200];
	char Host[100];
	UINT32 ProceType;
	char FileName[200];

	NapiCall* call = (NapiCall*)malloc(sizeof(NapiCall));
	call->env = env;
	call->fn = callback;
	call->json = json;

	napi_get_value_string_utf8(env, dllPath, DllPath, sizeof(DllPath), NULL);
	napi_get_value_string_utf8(env, host, Host, sizeof(Host), NULL);
	napi_get_value_uint32(env, type, &ProceType);
	napi_get_value_string_utf8(env, filename, FileName, sizeof(FileName), NULL);
	napi_get_value_string_utf8(env, extra, json_extra, sizeof(json_extra), NULL);

	Start(
		DllPath,
		Host,
		ProceType, // 0x0000,
		FileName,
		call
	);
}

napi_value Init(napi_env env, napi_value exports) {
	// 描述 hello 属性
	napi_property_descriptor desc = {
		"runTask",
		NULL,
		runTask,
		NULL,
		NULL,
		NULL,
		napi_default,
		NULL };
	// 将 hello 挂载到 exports 上面 === require('hello.node').hello;
	napi_define_properties(env, exports, 1, &desc);

	return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
