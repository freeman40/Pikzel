// Platform detection
#ifdef _WIN32
	/* Windows x64/x86 */
#ifdef _WIN64
	/* Windows x64  */
#define PKZL_PLATFORM_WINDOWS
#else
	/* Windows x86 */
#error "x86 Builds are not supported!"
#endif
#endif

// TODO: others platforms here...

#ifndef PKZL_PLATFORM_WINDOWS
#error "Pikzel is not supported on non-windows platforms yet!"
#endif

#ifdef PKZL_DEBUG
#ifdef PKZL_PLATFORM_WINDOWS
#define PKZL_DEBUG_BREAK __debugbreak()
#else
#define PKZL_DEBUG_BREAK
#endif
#define PKZL_ENABLE_ASSERTS
#endif

#include "Log.h"
#include "Instrumentor.h"

#ifdef PKZL_ENABLE_ASSERTS
#define CORE_ASSERT(x, ...) { if(!(x)) { CORE_LOG_ERROR(__VA_ARGS__); PKZL_DEBUG_BREAK; } }
#define ASSERT(x, ...) { if(!(x)) { LOG_ERROR(__VA_ARGS__); PKZL_DEBUG_BREAK; } }
#else
#define CORE_ASSERT(x, ...)
#define ASSERT(x, ...)
#endif
