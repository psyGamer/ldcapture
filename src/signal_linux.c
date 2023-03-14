#include "base.h"

#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

#define MAX_BACKTRACE_DEPTH 64

static struct sigaction orig_SIGABRT_act;
static struct sigaction orig_SIGFPE_act;
static struct sigaction orig_SIGILL_act;
static struct sigaction orig_SIGINT_act;
static struct sigaction orig_SIGSEGV_act;
static struct sigaction orig_SIGTERM_act;

static void sigact(i32 signal, siginfo_t* info, void* ucontext)
{
    const char* signalName = NULL;
    struct sigaction* origAct;
    bool printStacktrace = false;

    switch (signal)
    {
    case SIGABRT:
        signalName = "SIGABRT";
        origAct = &orig_SIGABRT_act;
        printStacktrace = true;
        break;
    case SIGFPE:
        signalName = "SIGFPE";
        origAct = &orig_SIGFPE_act;
        printStacktrace = true;
        break;
    case SIGILL:
        signalName = "SIGILL";
        origAct = &orig_SIGILL_act;
        printStacktrace = true;
        break;
    case SIGINT:
        signalName = "SIGINT";
        origAct = &orig_SIGINT_act;
        break;
    case SIGSEGV:
        signalName = "SIGSEGV";
        origAct = &orig_SIGSEGV_act;
        printStacktrace = true;
        break;
    case SIGTERM:
        signalName = "SIGTERM";
        origAct = &orig_SIGTERM_act;
        break;
    }

    if (signalName != NULL)
        DEBUG("Caught signal: %s", signalName);
    else
        DEBUG("Caught signal: %i", signal);

    if (printStacktrace)
    {
        DEBUG("-- Stacktrace START --");

        void* *buffer = calloc(MAX_BACKTRACE_DEPTH, sizeof(void*));
        backtrace(buffer, MAX_BACKTRACE_DEPTH); \
        char* *symbols = backtrace_symbols(buffer, MAX_BACKTRACE_DEPTH);

        for (i32 i = 0; i < MAX_BACKTRACE_DEPTH; i++)
        {
            if (buffer[i] == NULL) continue;
            DEBUG("%i: %s", i, symbols[i]);
        }

        DEBUG("-- Stacktrace END --");
    }


    if (origAct->sa_flags & SA_SIGINFO)
    {
        if (origAct->sa_sigaction == NULL) goto exit;
        origAct->sa_sigaction(signal, info, ucontext);
        return;
    }
    else
    {
        if (origAct->sa_handler == NULL) goto exit;
        origAct->sa_handler(signal);
        return;
    }

exit:
    DEBUG("Exiting...");
    _exit(signal);
}

void init_signal_linux()
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigact;

    sigaction(SIGABRT, &act, &orig_SIGABRT_act);
    sigaction(SIGFPE,  &act, &orig_SIGFPE_act);
    sigaction(SIGILL,  &act, &orig_SIGILL_act);
    sigaction(SIGINT,  &act, &orig_SIGINT_act);
    sigaction(SIGSEGV, &act, &orig_SIGSEGV_act);
    sigaction(SIGTERM, &act, &orig_SIGTERM_act);
}