#include "ApiLoader.h"
#include "AgentInfo.h"
#include "utils.h"

AgentInfo::AgentInfo()
{
	SYSTEM_PROCESSOR_INFORMATION SystemInfo = { 0 };
	OSVERSIONINFOEXW OSVersion = { 0 };
	OSVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

	ApiNt->NtQuerySystemInformation(SystemProcessorInformation, &SystemInfo, sizeof(SYSTEM_PROCESSOR_INFORMATION), 0);
	ApiNt->RtlGetVersion((PRTL_OSVERSIONINFOW) &OSVersion);

	BOOL isWow64 = FALSE;
	ApiWin->IsWow64Process((HANDLE)-1, &isWow64);

	this->agent_id      = GenerateRandom32();
	this->acp           = ApiWin->GetACP();
	this->oemcp         = ApiWin->GetOEMCP();
	this->gmt_offest    = GetGmtOffset();
	this->pid           = (WORD) NtCurrentTeb()->ClientId.UniqueProcess;
	this->tid           = (WORD)NtCurrentTeb()->ClientId.UniqueThread;
	this->elevated      = IsElevate();
	this->arch64        = (sizeof(void*) != 4);
	this->sys64         = this->arch64 || isWow64;
	this->build_number  = OSVersion.dwBuildNumber;
	this->major_version = OSVersion.dwMajorVersion;
	this->minor_version = OSVersion.dwMinorVersion;
	this->is_server     = OSVersion.wProductType != VER_NT_WORKSTATION;
	this->internal_ip   = GetInternalIpLong();
	this->username      = _GetUserName();
	this->domain_name   = _GetDomainName();
	this->computer_name = _GetHostName();
	this->process_name  = _GetProcessName();
}
