#include "WeChatPC.h"
#include "Common.h"
#include <vector>
#include <stdio.h>

static HMODULE g_Module = 0;
static ZWQUERYSYSTEMINFORMATION ZwQuerySystemInformation = 0;
static NTQUERYOBJECT NtQueryObject = 0;

//ͨ��ע����ȡ΢�ų���·��
static wchar_t* GetWeChatPCPath()
{
	static wchar_t path[256] = { 0 };
	//printf("%d\r\n", lstrlenW(path));
	if (path[0] != '\0')return path;
	HKEY hKey = NULL;
	if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, LR"(Software\Tencent\WXWork)", &hKey)) {
		return nullptr;
	}

	DWORD lpcbData = 256;
	if (ERROR_SUCCESS != RegQueryValueEx(hKey, LR"(Executable)", NULL, NULL, (LPBYTE)path, &lpcbData)) {
		RegCloseKey(hKey);
		return nullptr;
	}

	//wcscat_s(path, 256, LR"(\WXWork.exe)");
	return path;
}

//���ƾ��
static HANDLE DuplicateHandleEx(DWORD pid, HANDLE handle, DWORD flags)
{
	HANDLE hHandle = NULL;

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess) {
		if (!DuplicateHandle(hProcess, (HANDLE)handle, GetCurrentProcess(), &hHandle, NULL, FALSE, /*DUPLICATE_SAME_ACCESS*/flags)) {
			hHandle = NULL;
		}
		CloseHandle(hProcess);
	}
	return hHandle;
}

//Ȩ������
static BOOL ElevatePrivileges()
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	tkp.PrivilegeCount = 1;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return FALSE;
	LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)){
		return FALSE;
	}

	return TRUE;
}

//ö��Ŀ������б�
std::vector<unsigned long> TraverseWechatProcesses(const wchar_t *tragetName = LR"(WXWork.exe)")
{
	ULONG bufferSize = 0;

	std::vector<unsigned long> set;
	//��ȡ�����ڴ��С
	NTSTATUS status = ZwQuerySystemInformation(SystemProcessesAndThreadsInformation, NULL, 0, &bufferSize);
	if (status == STATUS_INFO_LENGTH_MISMATCH) {
		if (bufferSize) {
			PVOID pBuffer = new BYTE[bufferSize];
			status = ZwQuerySystemInformation(SystemProcessesAndThreadsInformation, pBuffer, bufferSize, NULL);
			if (status == STATUS_SUCCESS) {
				PSYSTEM_PROCESSES processEntry = (PSYSTEM_PROCESSES)pBuffer;
				do{
					if (processEntry->ProcessName.Length > 0 && wcscmp(processEntry->ProcessName.Buffer, tragetName) == 0) {
						//wprintf(L"%ws\r\n", processEntry->ProcessName.Buffer);
						set.push_back(processEntry->ProcessId);
					}
					processEntry = (PSYSTEM_PROCESSES)((BYTE*)processEntry + processEntry->NextEntryDelta);
				} while (processEntry->NextEntryDelta != 0);

			}
			delete[] pBuffer;
			pBuffer = NULL;
			return set;
		}
	}
	return set;
}

//ö��ȫ�־�����رշ���ָ��Ŀ��Ļ�����
static void DetachTargetHandle(const wchar_t *handleType = LR"(Mutant)", const wchar_t *handleName = LR"(Tencent.WeWork.ExclusiveObject)")
{
	ULONG bufferSize = NULL;
	PVOID pBuffer = NULL;

	std::vector<unsigned long> set = TraverseWechatProcesses();
	do{
		pBuffer = VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE);
		if (!pBuffer)break;

		NTSTATUS status = ZwQuerySystemInformation(SystemHandleInformation, pBuffer, 0x1000, &bufferSize);
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
		Restart:
			VirtualFree(pBuffer, NULL, MEM_RELEASE);
			pBuffer = NULL;
			if (bufferSize * 2 > 0x4000000/*MaxSize*/) {
				break;
			}

			pBuffer = VirtualAlloc(NULL, (SIZE_T)bufferSize * 2, MEM_COMMIT, PAGE_READWRITE);
			if (!pBuffer)break;

			status = ZwQuerySystemInformation(SystemHandleInformation, pBuffer, bufferSize * 2, NULL);
			if (status == STATUS_SUCCESS) {

				DWORD dwFlags = 0;
				char szName[512] = { 0 };
				char szType[128] = { 0 };

				PSYSTEM_HANDLE_INFORMATIONS pHandleInfo = (PSYSTEM_HANDLE_INFORMATIONS)pBuffer;

				for (ULONG idx = 0; idx < pHandleInfo->NumberOfHandles; idx++)
				{
					for (size_t i = 0; i < set.size(); ++i) {

					
					if (pHandleInfo->Handles[idx].UniqueProcessId == set[i]) {
						//printf("%8x\r\n", pHandleInfo->Handles[idx].UniqueProcessId);
						HANDLE hHandle = DuplicateHandleEx(pHandleInfo->Handles[idx].UniqueProcessId, (HANDLE)pHandleInfo->Handles[idx].HandleValue, DUPLICATE_SAME_ACCESS);
						if (hHandle == NULL) continue;

						status = NtQueryObject(hHandle, ObjectNameInformation, szName, 512, &dwFlags);
						if (!NT_SUCCESS(status)) {
							CloseHandle(hHandle);
							continue;
						}

						status = NtQueryObject(hHandle, ObjectTypeInformation, szType, 128, &dwFlags);
						if (!NT_SUCCESS(status)) {
							CloseHandle(hHandle);
							continue;
						}

						POBJECT_NAME_INFORMATION pNameName = (POBJECT_NAME_INFORMATION)szName;
						POBJECT_NAME_INFORMATION pNameType = (POBJECT_NAME_INFORMATION)szType;


						//printf("%d\t%wZ\t%wZ\n", pHandleInfo->Handles[idx].UniqueProcessId, pNameType, pNameName);
						if (pNameName->Name.Buffer
							&& pNameType->Name.Buffer
							&& wcscmp((wchar_t*)pNameType->Name.Buffer, handleType) == 0
							&& wcsstr((wchar_t*)pNameName->Name.Buffer, handleName)) {
							printf("121");
							CloseHandle(hHandle);

							hHandle = DuplicateHandleEx(pHandleInfo->Handles[idx].UniqueProcessId, (HANDLE)pHandleInfo->Handles[idx].HandleValue, DUPLICATE_CLOSE_SOURCE);

							if (hHandle) {
								CloseHandle(hHandle);
							}
							//printf("%8x\t%wZ\t%wZ\n", pHandleInfo->Handles[idx].UniqueProcessId, pNameType, pNameName);
						}
						else {
							CloseHandle(hHandle);
						}
					}
					}

				}

			}
			else if (status == STATUS_INFO_LENGTH_MISMATCH) {
				goto Restart;
			}
		}
		else {
			printf("%8x\r\n", status);
			break;
		}
	} while (false);

	if (pBuffer) {
		VirtualFree(pBuffer, 0, MEM_RELEASE);
		pBuffer = NULL;
	}
}

//���캯�� ���غ�����ַ
WeChatPC::WeChatPC()
{
	g_Module = GetModuleHandle(L"ntdll.dll");
	if (g_Module == NULL) {
		throw R"(ntdd.dll ģ��δ����)";
	}

	ZwQuerySystemInformation = ZwQuerySystemInformation = (ZWQUERYSYSTEMINFORMATION)GetProcAddress(g_Module, "ZwQuerySystemInformation");
	if (ZwQuerySystemInformation == NULL) {
		throw R"(ZwQuerySystemInformation ������ַ��ȡʧ��)";
	}

	NtQueryObject = (NTQUERYOBJECT)GetProcAddress(g_Module, "NtQueryObject");
	if (NtQueryObject == NULL) {
		throw R"(NtQueryObject ������ַ��ȡʧ��)";
	}
}

WeChatPC::~WeChatPC()
{
	FreeModule(g_Module);
}

//����΢��
void WeChatPC::Start()
{
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };

	DetachTargetHandle();

	CreateProcess(NULL, GetWeChatPCPath(), NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	DWORD pid = pi.dwProcessId;
}
