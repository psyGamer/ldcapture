#pragma once

#include "init.h"

typedef struct symbol_hook_t
{
    const char *symbolName;
    void *function;
    void **origFunction;
} symbol_hook_t;

void hook_symbol(void *function, void **origFunction, const char *symbolName);

void *load_orig_function(const char *origName);

#ifdef PLAT_LINUX

// name_fn_t:     Function pointer type to original functio
// orig_name:     Function pointer to original functio
// hook_symbol(): Let dlsym hook know of this overwrite
// name:          Overwrite for LD_PRELOAD or loaded from dlsym
// Load original if NULL
// Call init to ensure initializing even from LD_PRELOAD.
// init() is guarantied to be included since hook.h is included.
#define SYM_HOOK(ret, name, args, body)              \
    typedef ret (* name##_fn_t) args ;               \
    static name##_fn_t orig_##name ;                 \
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