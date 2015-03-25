#pragma once

#include <Windows.h>

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <float.h>

#include <memory>
#include <vector>

// Fast vector math with SSE support
#include <DirectXMath.h>
using namespace DirectX;

// RAII wrappers
#include <wrl.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

// Image loading library
#include <wincodec.h>
