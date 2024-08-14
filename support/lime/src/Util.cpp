#include <vLime/Util.h>

#include <iostream>

namespace lime::log {

void logDefault(std::string_view const msg)
{
    std::cout << msg;
    if (msg.back() != '\n')
        std::cout << '\n';
}

static void (*logCallbackInfo)(std::string_view) { &logDefault };
static void (*logCallbackDebug)(std::string_view) { &logDefault };
static void (*logCallbackError)(std::string_view) { &logDefault };

void info(std::string_view const msg)
{
    if (logCallbackInfo)
        logCallbackInfo(msg);
}

void debug(std::string_view const msg)
{
    if (logCallbackDebug)
        logCallbackDebug(msg);
}

void error(std::string_view const msg)
{
    if (logCallbackError)
        logCallbackError(msg);
}

void SetCallbacks(void (*callbackInfo)(std::string_view), void (*callbackDebug)(std::string_view), void (*callbackError)(std::string_view))
{
    logCallbackInfo = callbackInfo;
    logCallbackDebug = callbackDebug;
    logCallbackError = callbackError;
}

void LogSetCallbackDefaults()
{
    logCallbackInfo = &logDefault;
    logCallbackDebug = &logDefault;
    logCallbackError = &logDefault;
}

}
