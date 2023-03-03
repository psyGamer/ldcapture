#ifdef PLAT_LINUX

#include "hook.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define __USE_GNU
#include <dlfcn.h>

// #include "video_opengl.h"

static int hookedSymbolIndex = 0;
static symbol_hook_t hookedSymbols[1024];

// Let's hope this doesn't catch fire..
typedef void *(*dlsym_fn_t)(void *, const char *);
static dlsym_fn_t orig_dlsym = NULL;

static void init_dlsym()
{
    // First use the versioned dlsym function, which is untouched
    orig_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    // With the untouced versioned, get the newest one.
    orig_dlsym = orig_dlsym(RTLD_NEXT, "dlsym");

    init_ldcapture(); // Before orig_dlsym gets loaded, nothing excpet LD_PRELOAD could've executed.
}

void *dlsym(void *handle, const char *name) {
    if (orig_dlsym == NULL) init_dlsym();

    for (int i = 0; i < hookedSymbolIndex; i++)
    {
        if (strcmp(hookedSymbols[i].symbolName, name) == 0)
        {
            *hookedSymbols[i].origFunction = orig_dlsym(handle, name);
            return hookedSymbols[i].function;
        }
    }

    return orig_dlsym(handle, name);
}

void hook_symbol(void *function, void **origFunction, const char *symbolName)
{
    symbol_hook_t hook = { .symbolName = symbolName, .function = function, .origFunction = origFunction };
    hookedSymbols[hookedSymbolIndex++] = hook;
}

void *load_orig_function(const char *origName)
{
    if (orig_dlsym == NULL) init_dlsym();

    void *orig = orig_dlsym(RTLD_NEXT, origName);
    if (orig == NULL)
        fprintf(stderr, "Failed loading original function %s: %s\n", origName, dlerror());

    return orig;
}

#endif // PLAT_LINUX