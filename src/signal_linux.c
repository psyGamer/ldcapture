#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "init.h"

#define IMPL_HANDLE_SIGNAL(sig) \
    void handle_signal_##sig() \
    { \
        printf("Got exit signal " #sig "\n"); \
        shutdown_ldcapture(); \
        if (!is_shutdown_done_ldcapture()) printf("Spinner until shutdown is done..."); \
        while (!is_shutdown_done_ldcapture()) { } \
        printf("   DONE\n"); \
        _exit(EXIT_FAILURE); \
    }

// void handle_signal()
// {
//     printf("Got exit signal\n");
//     shutdown_ldcapture();

//     // Spin while wait for shutdown to complete
//     if (!is_shutdown_done_ldcapture()) printf("Spinner until shutdown is done");
//     while (!is_shutdown_done_ldcapture()) { printf("."); }
//     printf("   DONE\n");

//     _exit(EXIT_FAILURE);
// }

IMPL_HANDLE_SIGNAL(SIGABRT);
IMPL_HANDLE_SIGNAL(SIGFPE);
IMPL_HANDLE_SIGNAL(SIGILL);
IMPL_HANDLE_SIGNAL(SIGINT);
IMPL_HANDLE_SIGNAL(SIGSEGV);
IMPL_HANDLE_SIGNAL(SIGTERM);

void init_signal_linux()
{
    printf("Initing signals\n");
    // signal(SIGABRT, handle_signal);
    // signal(SIGFPE, handle_signal);
    // signal(SIGILL, handle_signal);
    // signal(SIGINT, handle_signal);
    // signal(SIGSEGV, handle_signal);
    // signal(SIGTERM, handle_signal);
    signal(SIGABRT, handle_signal_SIGABRT);
    signal(SIGFPE, handle_signal_SIGFPE);
    signal(SIGILL, handle_signal_SIGILL);
    signal(SIGINT, handle_signal_SIGINT);
    signal(SIGSEGV, handle_signal_SIGSEGV);
    signal(SIGTERM, handle_signal_SIGTERM);
}