#ifdef PLAT_LINUX

#include "hook.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define __USE_GNU
#include <dlfcn.h>
#include <link.h>

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
        symbol_hook_t *hook = &hookedSymbols[i];
        if (strcmp(hook->symbolName, name) == 0)
        {
            if (*hook->originalAddress == NULL) *hook->originalAddress = orig_dlsym(handle, name);
            if (hook->address != NULL) return hook->address;
        }
    }

    return orig_dlsym(handle, name);
}

void hook_symbol(void *address, void **originalAddress, const char *symbolName)
{
    if (hookedSymbolIndex >= 1024)
    {
        fprintf(stderr, "Reached maximum amount of symbol hooks (1024), can't add more!\n");
        return;
    }

    symbol_hook_t hook = { .symbolName = symbolName, .address = address, .originalAddress = originalAddress };
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

library_handle_t shared_library_open(const char *libraryName)
{
    // On Linux a shared library usually has the format lib<name>.so
    // If the name is longer than 1024 characters, I have different concerns..
    char actualLibraryName[1024];
    sprintf(actualLibraryName, "lib%s.so.10", libraryName);
    
    library_handle_t handle = dlopen(actualLibraryName, RTLD_LAZY);

    if (handle == NULL)
        fprintf(stderr, "Failed loading library %s: %s\n", actualLibraryName, dlerror());

    return handle;
}

void shared_library_close(library_handle_t handle)
{
    dlclose(handle);
}

void *shared_library_get_symbol(library_handle_t handle, const char *symbolName)
{
    if (orig_dlsym == NULL) init_dlsym();

    struct link_map *map;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);

    void *symbol = orig_dlsym(handle, symbolName);
    if (symbol == NULL)
    {
        if (map == NULL || map->l_name == NULL)
            fprintf(stderr, "Failed loading symbol %s from <unknown>(%p): %s\n", symbolName, handle, dlerror());
        else
            fprintf(stderr, "Failed loading symbol %s from %s(%p): %s\n", symbolName, map->l_name, handle, dlerror());
    }

    return symbol;
}

#endif // PLAT_LINUX