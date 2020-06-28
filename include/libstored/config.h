#ifndef __LIBSTORED_CONFIG_H
#define __LIBSTORED_CONFIG_H
/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libstored/macros.h>

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>

/*!
 * \defgroup libstored_config config
 * \brief Static configuration of the libstored library.
 * \ingroup libstored
 */

namespace stored {
	/*!
	 * \ingroup libstored_config
	 */
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
		static bool const DebuggerIdentification = true;
		static int const DebuggerVersion = 2;
		static bool const DebuggerReadMem = true;
		static bool const DebuggerWriteMem = true;
		static int const DebuggerStreams = 1;
		static size_t const DebuggerStreamBuffer = 1024;
	};
} // namespace
#endif // __cplusplus

#include "stored_config.h"

#endif // __LIBSTORED_CONFIG_H

