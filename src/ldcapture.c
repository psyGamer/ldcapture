#include <stdio.h>
#include <dlfcn.h>

// static void* (*real_malloc)(size_t) = NULL;

// static void ldcapture_init()
// {
// 	real_malloc = dlsym(RTLD_NEXT, "malloc");
// 	if (real_malloc == NULL)
// 	{
// 		fprintf(stderr, "Error in dlsym: %s\n", dlerror());
// 	}
// }

// void *malloc(size_t size)
// {
// 	if (real_malloc == NULL)
// 	{
// 		ldcapture_init();
// 	}

// 	void *p = NULL;
// 	fprintf(stderr, "malloc(%zu) = ", size);
// 	p = real_malloc(size);
// 	fprintf(stderr, "%p\n", p);
// 	return p;
// }

static void *load_original_function(const char* realName)
{
	void *real = dlsym(RTLD_NEXT, realName);
	if (real == NULL)
		fprintf(stderr, "Error in dlsym: %s\n", dlerror());
	return real;
}

static void* (*real_malloc)(size_t) = NULL;
void *malloc(size_t size)
{
	if (real_malloc == NULL) real_malloc = load_original_function("malloc");

	void *p = NULL;
	fprintf(stderr, "malloc(%zu) = ", size);
	p = real_malloc(size);
	fprintf(stderr, "%p\n", p);
	return p;
}