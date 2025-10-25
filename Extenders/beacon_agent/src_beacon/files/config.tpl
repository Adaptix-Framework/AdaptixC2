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
