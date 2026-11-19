#pragma once

// The only engine file that includes Tracy (decision 0003). Empty unless APEX_ENABLE_PROFILING is on.
#if defined(APEX_ENABLE_PROFILING)
#include <tracy/Tracy.hpp>

#define APEX_PROFILE_FRAME() FrameMark
// name must be a string literal. Declares a local: one scope per block.
#define APEX_PROFILE_SCOPE(name) ZoneScopedN(name)
#else
#define APEX_PROFILE_FRAME() static_cast<void>(0)
#define APEX_PROFILE_SCOPE(name) static_cast<void>(0)
#endif
