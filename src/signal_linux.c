#include "base.h"

#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

#include "init.h"

#define IMPL_HANDLE_SIGNAL(sig, printStack) \
    void handle_signal_##sig() \
    { \
        DEBUG("Got exit signal " #sig); \
        shutdown_ldcapture(); \
        if (!is_shutdown_done_ldcapture()) DEBUG("Shutting down..."); \
        while (!is_shutdown_done_ldcapture()); \
        DEBUG("DONE"); \
        if (printStack) { \
            DEBUG("-- Stacktrace START --"); \
            u32 cnt = 64; \
            void **buf = calloc(cnt, sizeof(void *)); \
            backtrace(buf, cnt); \
            char **syms = backtrace_symbols(buf, cnt); \
            for (i32 i = 0; i < cnt; i++) \
            { \
                if (buf[i] == NULL) break; \
                DEBUG("%i: %s", i, syms[i]); \
            } \
            DEBUG("-- Stacktrace END --"); \
        } \
        _exit(EXIT_FAILURE); \
    }

IMPL_HANDLE_SIGNAL(SIGABRT, true);
IMPL_HANDLE_SIGNAL(SIGFPE, true);
IMPL_HANDLE_SIGNAL(SIGILL, true);
IMPL_HANDLE_SIGNAL(SIGINT, false);
IMPL_HANDLE_SIGNAL(SIGSEGV, true);
IMPL_HANDLE_SIGNAL(SIGTERM, false);

void init_signal_linux()
{
    signal(SIGABRT, handle_signal_SIGABRT);
    signal(SIGFPE, handle_signal_SIGFPE);
    signal(SIGILL, handle_signal_SIGILL);
    signal(SIGINT, handle_signal_SIGINT);
    signal(SIGSEGV, handle_signal_SIGSEGV);
    signal(SIGTERM, handle_signal_SIGTERM);
}