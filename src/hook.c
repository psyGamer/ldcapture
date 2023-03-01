#include "inject.h"

#include <stdio.h>
#include <dlfcn.h>

static void *load_original_function(const char* realName)
{
	void *real = dlsym(RTLD_NEXT, realName);
	if (real == NULL)
		fprintf(stderr, "Error in dlsym: %s\n", dlerror());
	return real;
}