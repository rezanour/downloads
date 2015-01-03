#pragma once

#include <Windows.h>
#include <stdint.h>
#include <math.h>
#include <memory>
#include <vector>
#include <map>

#include <d3d11.h>
#include <wincodec.h>   // WIC, for image format support
#include <wrl.h>        // COM smart pointer and other useful RAII types

#include "Vector2.h"
#include "Matrix2.h"
#include "AABB.h"

#define USE_FIXED_TIMESTEP

// Helper for COM error handling
inline void ThrowCOMError(const char* statement, HRESULT error)
{
    char message[2048] = {};
    sprintf_s(message, "%s failed with 0x%08x.", statement, error);
    throw std::exception(message);
}

#define CHECKHR(x) { HRESULT _hr = (x); if (FAILED(_hr)) { ThrowCOMError(#x, _hr); } }