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