#include <napi.h>
#include <iostream>
#include <windows.h>

typedef struct {
	Napi::Env env;
	Napi::Function fn;
	Napi::Object json;
} NapiCall;

char json_extra[240];

void Stdout2Nodejs(INT32 code, INT32 index, INT32 total, char* extra = json_extra) {
	std::cout << "##{"
		<< "\"code\":" << code
		<< ","
		<< "\"index\":" << index
		<< ","
		<< "\"total\":" << total
		<< ","
		<< "\"extra\":" << "\"" << extra << "\""
		<< "}##" << std::endl;
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

extern "C" {
	typedef bool (CALLBACK* pAclasSDKInitialize)(char* s);
	typedef bool (CALLBACK* pGetDevicesInfo)(UINT32 Addr, UINT32 Port, UINT32 ProtocolType, DeviceInfo* info);
	typedef void (WINAPI* FP)(UINT32 Errorcode, UINT32 index, UINT32 Total, NapiCall* call); // Last args is custome pointer
	typedef HANDLE (CALLBACK* pAclasSDKExecTask)(UINT32 Addr, UINT32 Port, UINT32 ProtocolType, UINT32 ProceType, UINT32 DataType, char* FileName, FP fp, NapiCall* call);
	typedef HANDLE (CALLBACK* pAclasSDKWaitForTask)(HANDLE handle);
	typedef int(CALLBACK* pAclasSDKSyncExecTask)(char* Addr, UINT32 Port, UINT32 ProtocolType, UINT32 ProceType, UINT32 DataType, char* FileName);
}

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
		std::cout << "Initialize success" << std::endl;
	}
	else {
		std::cout << "Initialize failed" << std::endl;
		return;
	}

	// Get Device Information
	pGetDevicesInfo getDevicesInfo = (pGetDevicesInfo)GetProcAddress(hModule, "AclasSDK_GetDevicesInfo");
	DeviceInfo* info = (DeviceInfo*)malloc(sizeof(DeviceInfo));
	UINT addr = MakeHost2Dword(Host); // "192.168.1.2"
	BOOL ref = getDevicesInfo(addr, 0, 0, info);

	std::cout << info->name << std::endl;

	pAclasSDKExecTask execTask = (pAclasSDKExecTask)GetProcAddress(hModule, "AclasSDK_ExecTaskA");
	pAclasSDKWaitForTask waitForTask = (pAclasSDKWaitForTask)GetProcAddress(hModule, "AclasSDK_WaitForTask");
	HANDLE handle = waitForTask(execTask(addr, info->Port, info->ProtocolType, 0, ProceType, FileName, onprogress, call));
	
	// Release resource
	GetProcAddress(hModule, "AclasSDK_Finalize");
}
// -----------------------------------------------

void RunCallback(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	Napi::Function cb = info[1].As<Napi::Function>();
	Napi::Object json = info[0].As<Napi::Object>();

	Napi::Value host = json["host"];
	Napi::Value proceType = json["type"];
	Napi::Value filename = json["filename"];
	Napi::Value dllPath = json["dll_path"];

	char DllPath[200];
	char Host[100];
	UINT32 ProceType;
	char FileName[200];

	NapiCall* call = (NapiCall*)malloc(sizeof(NapiCall));
	call->env = env;
	call->fn = cb;
	call->json = json;

	napi_get_value_string_utf8(env, dllPath, DllPath, sizeof(DllPath), NULL);
	napi_get_value_string_utf8(env, host, Host, sizeof(Host), NULL);
	napi_get_value_uint32(env, proceType, &ProceType);
	napi_get_value_string_utf8(env, filename, FileName, sizeof(FileName), NULL);
	Napi::Value extra = call->json["extra"];
	napi_get_value_string_utf8(call->env, extra, json_extra, sizeof(json_extra), NULL);

	Start(
		DllPath,
		Host,
		ProceType, // 0x0000,
		FileName,
		call
	);
	// cb.Call(env.Global(), { json });
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	return Napi::Function::New(env, RunCallback);
}

NODE_API_MODULE(aclas, Init)
