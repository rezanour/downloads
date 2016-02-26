#pragma once

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#include <functional>
#include <string>
#include <map>
