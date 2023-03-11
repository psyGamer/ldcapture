#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

#include "init.h"

#define IMPL_HANDLE_SIGNAL(sig, printStack) \
    void handle_signal_##sig() \
    { \
        printf("Got exit signal " #sig "\n"); \
        shutdown_ldcapture(); \
        if (!is_shutdown_done_ldcapture()) printf("Shutting down..."); \
        while (!is_shutdown_done_ldcapture()); \
        printf("   DONE\n"); \
        if (printStack) { \
            printf("-- Stacktrace START --\n"); \
            unsigned int cnt = 64; \
            void **buf = calloc(cnt, sizeof(void *)); \
            backtrace(buf, cnt); \
            char **syms = backtrace_symbols(buf, cnt); \
            for (int i = 0; i < cnt; i++) \
            { \
                if (buf[i] == NULL) break; \
                printf("%i: %s\n", i, syms[i]); \
            } \
            printf("-- Stacktrace END --\n"); \
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