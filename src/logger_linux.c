#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAX_MSG_LENGTH 32767

void log_output(log_level_t level, const char *message, ...)
{
    const char *levelStrings[6] = {
        "(ldcapture/fatal): ", 
        "(ldcapture/error): ", 
        "(ldcapture/warn):  ", 
        "(ldcapture/info):  ", 
        "(ldcapture/debug): ", 
        "(ldcapture/trace): "
    };
    const char *colorStrings[6] = {"1;41", "1;31", "0;33", "0;32", "0;34", "0;37"};

    // Technically imposes a 32k character limit on a single log entry, but...
    // DON'T DO THAT!
    char formatMessage[MAX_MSG_LENGTH];
    memset(formatMessage, 0, sizeof(formatMessage));

    // Format original message.
    // NOTE: Oddly enough, MS's headers override the GCC/Clang va_list type with a "typedef char* va_list" in some
    // cases, and as a result throws a strange error here. The workaround for now is to just use __builtin_va_list,
    // which is the type GCC/Clang's va_start expects.
    __builtin_va_list argPtr;
    va_start(argPtr, message);
    vsnprintf(formatMessage, MAX_MSG_LENGTH, message, argPtr);
    va_end(argPtr);

    char outputMessage[MAX_MSG_LENGTH];
    sprintf(outputMessage, "%s%s\n", levelStrings[level], formatMessage);

    printf("\033[%sm%s\033[0m", colorStrings[level], outputMessage);
}