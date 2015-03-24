#pragma once

#if defined(_DEBUG)

void __cdecl _DebugWrite(const wchar_t* format, ...);
#define Log(format, ...) { _DebugWrite(format L"\n", __VA_ARGS__); }
#define LogError(format, ...) { _DebugWrite(L"ERROR: " format L"\n", __VA_ARGS__); assert(false); }

#else

#define _DebugWrite(...) {}
#define Log(format, ...) {}
#define LogError(format, ...) {}

#endif
