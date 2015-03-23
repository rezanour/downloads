#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

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

// For RAII wrappers
#include <wrl.h>
using namespace Microsoft::WRL;

#include "DirectXTex\DirectXTex\DirectXTex.h"
