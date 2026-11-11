// Compatibility shims for building the stock rerelease sources with MinGW-w64 GCC.
// Force-included via CMake (-include) so stock id files stay unmodified.
#pragma once

#ifdef __MINGW32__
#include <cmath>
// MSVC exposes these in the global namespace; libstdc++ only in std::.
using std::isinf;
using std::isnan;
#endif
