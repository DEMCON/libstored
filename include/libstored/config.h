#ifndef __LIBSTORED_CONFIG_H
#define __LIBSTORED_CONFIG_H

#include <libstored/macros.h>

#ifdef __cplusplus
#include <stddef.h>
#include <stdint.h>

namespace stored {
	struct DefaultConfig {
		static bool const Debug = 
#ifndef NDEBUG
			true;
#else
			false;
#endif

		static bool const EnableAssert = 
#ifndef NDEBUG
			Debug;
#else
			false;
#endif

		static bool const FullNames = true;
		static bool const EnableHooks = true;
		static bool const HookSetOnChangeOnly = false;

		static bool const DebuggerRead = true;
		static bool const DebuggerWrite = true;
		static bool const DebuggerEcho = true;
		static bool const DebuggerList = true;
		static int const DebuggerAlias = 0x100; // Value is max number of aliases. efault is effectively no limit.
		static int const DebuggerMacro = 0x1000; // Value is max total length of macros.
	};
} // namespace
#endif // __cplusplus

#include "stored_config.h"

#endif // __LIBSTORED_CONFIG_H

