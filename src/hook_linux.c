#include "base.h"

#ifdef PLATFORM_LINUX

#define __USE_GNU
#include <dlfcn.h>
#include <link.h>

#include "api.h"
#include "hook.h"

#define MAX_HOOKED_SYMBOLS 1024
static i32 hooked_symbol_index = 0;
static symbol_hook_t hooked_symbols[MAX_HOOKED_SYMBOLS];

// Let's hope this doesn't catch fire..
static void* (*orig_dlsym)(void* , const char*) = NULL;

static void init_dlsym()
{
    // First use the versioned dlsym function, which is untouched
    orig_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    // With the untouced versioned, get the newest one.
    orig_dlsym = orig_dlsym(RTLD_NEXT, "dlsym");

    init_ldcapture(); // Before orig_dlsym gets loaded, nothing excpet LD_PRELOAD could've executed.
}

static bool catch_api_symbol(const char* name, void* *outApiSymbol)
{
    if (strcmp(name, "ldcapture_StartRecording") == 0)
    {
        TRACE("Loaded API function: ldcapture_StartRecording");
        *outApiSymbol = ldcapture_StartRecording;
        return true;
    }
    if (strcmp(name, "ldcapture_StopRecording") == 0)
    {
        TRACE("Loaded API function: ldcapture_StopRecording");
        *outApiSymbol = ldcapture_StopRecording;
        return true;
    }
    if (strcmp(name, "ldcapture_StopRecordingAfter") == 0)
    {
        TRACE("Loaded API function: ldcapture_StopRecordingAfter");
        *outApiSymbol = ldcapture_StopRecordingAfter;
        return true;
    }

    // Slightly hacky way of invoking these functions by just loading a symbol
    if (strcmp(name, "INVOKE_ldcapture_StartRecording") == 0)
    {
        TRACE("Invoked API function: INVOKE_ldcapture_StartRecording");
        *outApiSymbol = NULL;
        ldcapture_StartRecording();
        return true;
    }
    else if (strcmp(name, "INVOKE_ldcapture_StopRecording") == 0)
    {
        TRACE("Invoked API function: ldcapture_StopRecording");
        *outApiSymbol = NULL;
        ldcapture_StopRecording();
        return true;
    }
    
    return false;
}

void* dlsym(void* handle, const char* name) {
    if (orig_dlsym == NULL) init_dlsym();

    void* apiSym = NULL;
    if (catch_api_symbol(name, &apiSym))
        return apiSym;

    // If something tries load dlsym with dlsym (IDK why you'd every do this, but some programs do it, so...)
    if (strcmp(name, "dlsym") == 0)
    {
        return dlsym;
    }

    for (i32 i = 0; i < hooked_symbol_index; i++)
    {
        symbol_hook_t* hook = &hooked_symbols[i];
        if (strcmp(hook->symbol_name, name) == 0)
        {
            if (*hook->original_address == NULL)* hook->original_address = orig_dlsym(handle, name);
            if (hook->address != NULL) return hook->address;
            TRACE("Hooked %s: New: %p | Old: %p", hook->symbol_name, hook->address, hook->original_address &&* hook->original_address);
        }
    }

    return orig_dlsym(handle, name);
}

void hook_symbol(void* address, void* *originalAddress, const char* symbolName)
{
    if (hooked_symbol_index >= MAX_HOOKED_SYMBOLS)
    {
        FATAL("Reached maximum amount of symbol hooks (%i), can't add more!", MAX_HOOKED_SYMBOLS);
        exit(EXIT_FAILURE);
        return;
    }

    symbol_hook_t hook = { .symbol_name = symbolName, .address = address, .original_address = originalAddress };
    hooked_symbols[hooked_symbol_index++] = hook;
}

void* load_orig_function(const char* origName)
{
    if (orig_dlsym == NULL) init_dlsym();

    void* orig = orig_dlsym(RTLD_NEXT, origName);
    if (orig == NULL)
        ERROR("Failed loading original function %s: %s", origName, dlerror());
#if LOG_TRACE_ENABLED == 1
    else
        TRACE("Successfully loaded original function %s", origName);
#endif

    return orig;
}

library_handle_t shared_library_open(const char* libraryName)
{
    // On Linux a shared library usually has the format lib<name>.so
    // If the name is longer than 1024 characters, I have different concerns..
    char actualLibraryName[1024];
    sprintf(actualLibraryName, "lib%s.so.10", libraryName);
    
    library_handle_t handle = dlopen(actualLibraryName, RTLD_LAZY);

    if (handle == NULL)
        ERROR("Failed loading library %s: %s", actualLibraryName, dlerror());
#if LOG_TRACE_ENABLED == 1
    else
        TRACE("Successfully loaded library %s", actualLibraryName);
#endif

    return handle;
}

void shared_library_close(library_handle_t handle)
{
    struct link_map* map;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);

    if (dlclose(handle) != 0)
    {
        if (map == NULL || map->l_name == NULL)
            ERROR("Failed closing library <unknown>(%p): %s", handle, dlerror());
        else
            ERROR("Failed closing library %s(%p): %s", map->l_name, handle, dlerror());
    }
#if LOG_TRACE_ENABLED == 1
    else
    {
        if (map == NULL || map->l_name == NULL)
            TRACE("Successfully closed library <unknown>(%p)", handle);
        else
            TRACE("Successfully closed library %s(%p)", map->l_name, handle);
    }
#endif

}

void* shared_library_get_symbol(library_handle_t handle, const char* symbolName)
{
    if (orig_dlsym == NULL) init_dlsym();

    struct link_map* map;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);

    void* symbol = orig_dlsym(handle, symbolName);
    if (symbol == NULL)
    {
        if (map == NULL || map->l_name == NULL)
            ERROR("Failed loading symbol %s from <unknown>(%p): %s", symbolName, handle, dlerror());
        else
            ERROR("Failed loading symbol %s from %s(%p): %s", symbolName, map->l_name, handle, dlerror());
    }
#if LOG_TRACE_ENABLED == 1
    else
    {
        if (map == NULL || map->l_name == NULL)
            TRACE("Successfully loaded symbol %s from <unknown>(%p)", symbolName, handle);
        else
            TRACE("Successfully loaded symbol %s from %s(%p)", symbolName, map->l_name, handle);
    }
#endif

    return symbol;
}

#endif // PLATFORM_LINUX