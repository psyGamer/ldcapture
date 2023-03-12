#pragma once

#include "base.h"
#include "init.h"

typedef struct symbol_hook_t
{
    const char *symbol_name;

    void *address;
    void **original_address;
} symbol_hook_t;

typedef void *library_handle_t;

void hook_symbol(void *address, void **originalAddress, const char *symbolName);

void *load_orig_function(const char *origName);

library_handle_t shared_library_open(const char *libraryName);
void shared_library_close(library_handle_t handle);
void *shared_library_get_symbol(library_handle_t handle, const char *symbolName);

#ifdef PLATFORM_LINUX

// Setup everything for dlsym hook or LD_PRELOAD

#define SYM_HOOK(ret, name, args, body)              \
    typedef ret (* name##_fn_t) args ;               \
    static name##_fn_t orig_##name = NULL;           \
    ret name args                                    \
    {                                                \
        if (orig_##name == NULL)                     \
        {                                            \
            orig_##name = load_orig_function(#name); \
            init_ldcapture();                        \
        }                                            \
        body                                         \
    }

#define LOAD_SYM_HOOK(name) \
    hook_symbol(name, (void **)&orig_##name, #name)

#endif