#if defined(BUILD_SVC)
char* getServiceName()
{
	return (char*) SERVICE_NAME;
}
#endif

char* getProfile()
{
	return (char*) PROFILE;
}

unsigned int getProfileSize()
{
	return PROFILE_SIZE;
}

int isIatHidingEnabled()
{
#if defined(IAT_HIDING)
	return 1;
#else
	return 0;
#endif
}

int isBofStompEnabled()
{
#if defined(USE_BOF_STOMP)
	return 1;
#else
	return 0;
#endif
}

char* getBofStompDll()
{
#if defined(BOF_STOMP_DLL_NAME)
	return (char*) BOF_STOMP_DLL_NAME;
#else
	return (char*)"wmp.dll";
#endif
}

int getBofStompMethod()
{
#if defined(BOF_STOMP_METHOD)
	return BOF_STOMP_METHOD;
#else
	return 0;
#endif
}