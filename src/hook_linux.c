#ifdef PLAT_LINUX

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define __USE_GNU
#include <dlfcn.h>

#include "video_opengl.h"

static bool allowHooking = true;

void *load_orig_function(const char *origName)
{
    // Make sure we don't return a hooked version.
    allowHooking = false;

    void *orig = dlsym(RTLD_NEXT, origName);
    if (orig == NULL)
        fprintf(stderr, "Failed loading original function %s: %s\n", origName, dlerror());

    allowHooking = true;

    return orig;
}

// Let's hope this doesn't catch fire..
typedef void *(*dlsym_fn_t)(void *, const char *);

void *dlsym(void *handle, const char *name) {
    static dlsym_fn_t orig_dlsym = NULL;

    if (orig_dlsym == NULL)
    {
        // First use the versioned dlsym function, which is untouched
        orig_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
        // With the untouced versioned, get the newest one.
        orig_dlsym = orig_dlsym(RTLD_NEXT, "dlsym");
    }

    if (!allowHooking) return orig_dlsym(handle, name);

    // fprintf(stdout, "Loading symbol: %s from %p\n", name, handle);
    if (strcmp(name, "glXGetProcAddressARB") == 0)
    {
        fprintf(stdout, "Loading symbol: %s from %p (HOOKED)\n", name, handle);
        return hook_glXGetProcAddressARB;
    }

    return orig_dlsym(handle, name);
}

#endif // PLAT_LINUX