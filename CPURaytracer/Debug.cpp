#include "Precomp.h"
#include "Debug.h"

#if defined(_DEBUG)

void __cdecl _DebugWrite(const wchar_t* format, ...)
{
    wchar_t message[2048] = {};

    va_list args;
    va_start(args, format);
    vswprintf_s(message, format, args);
    va_end(args);

    OutputDebugString(message);
}

#endif
