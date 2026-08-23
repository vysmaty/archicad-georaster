// General platform settings required by the Archicad Add-On API.
#pragma once

#if defined(_MSC_VER) && !defined(WINDOWS)
#define WINDOWS
#endif

#if defined(WINDOWS)
#include "Win32Interface.hpp"
#endif

#if !defined(ACExtension)
#define ACExtension
#endif
