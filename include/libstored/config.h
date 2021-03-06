#ifndef LIBSTORED_CONFIG_H
#define LIBSTORED_CONFIG_H
/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
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

namespace stored {
	/*!
	 * \brief Default configuration.
	 *
	 * All configuration options are set at compile-time.
	 *
	 * To override defaults, copy the file stored_config.h to your own project,
	 * set the configuration values properly, and make sure that that file is
	 * in the include path before the libstored one.
	 */
	struct DefaultConfig {
		/*!
		 * \brief When \c true, compile as debug binary.
		 *
		 * This may include additional (and possibly slow) code for debugging,
		 * which can safely left out for release builds.
		 */
		static bool const Debug =
#ifndef NDEBUG
			true;
#else
			false;
#endif

		/*!
		 * \brief When \c true, enable #stored_assert() checks.
		 */
		static bool const EnableAssert =
#ifndef NDEBUG
			Debug;
#else
			false;
#endif

		/*!
		 * \brief Indicate if the store's buffer is in little endian.
		 *
		 * Usually, you would use the same endianness as the host,
		 * but as the Synchronizer does not swap endianness for the data,
		 * synchronization between different CPU types is not possible.
		 * In that case, one on both sides should save its store differently.
		 *
		 * Make sure that this flag corresponds to the endianness setting of
		 * the generator (-b flag).
		 */
		static bool const StoreInLittleEndian =
#ifdef STORED_LITTLE_ENDIAN
			true;
#else
			false;
#endif

		/*!
		 * \brief When \c true, include full name directory listing support.
		 *
		 * If \c false, a listing can be still be requested, but the names may
		 * be abbreviated.
		 */
		static bool const FullNames = true;
		/*!
		 * \brief When \c true, enable calls to \c hook...() functions of the store.
		 *
		 * This may be required for additional synchronization, but may add
		 * overhead for every object access.
		 */
		static bool const EnableHooks = true;

		/*! \brief When \c true, stored::Debugger implements the read capability. */
		static bool const DebuggerRead = true;
		/*! \brief When \c true, stored::Debugger implements the write capability. */
		static bool const DebuggerWrite = true;
		/*! \brief When \c true, stored::Debugger implements the echo capability. */
		static bool const DebuggerEcho = true;
		/*! \brief When \c true, stored::Debugger implements the list capability. */
		static bool const DebuggerList = true;
		/*!
		 * \brief When not 0, stored::Debugger implements the alias capability.
		 *
		 * The defined number is the number of aliases that are supported at the same time.
		 */
		static int const DebuggerAlias = 0x100; // Value is max number of aliases. Default is effectively no limit.
		/*!
		 * \brief When not 0, stored::Debugger implements the macro capability.
		 *
		 * The defined number is the total amount of memory that can be used
		 * for all macro definitions (excluding data structure overhead of the
		 * implementation).
		 */
		static int const DebuggerMacro = 0x1000;
		/*! \brief When \c true, stored::Debugger implements the identification capability. */
		static bool const DebuggerIdentification = true;
		/*! \brief When \c true, stored::Debugger implements the version capability. */
		static int const DebuggerVersion = 2;
		/*! \brief When \c true, stored::Debugger implements the read memory capability. */
		static bool const DebuggerReadMem = true;
		/*! \brief When \c true, stored::Debugger implements the write memory capability. */
		static bool const DebuggerWriteMem = true;
		/*!
		 * \brief When not 0, stored::Debugger implements the streams capability.
		 *
		 * The defined number is the number of concurrent streams that are supported.
		 */
		static int const DebuggerStreams = 2; // by default two: one for the application, one for tracing
		/*! \brief Size of one stream buffer in bytes. */
		static size_t const DebuggerStreamBuffer = 1024;

		/*! \brief When \c true, stored::Debugger implements the trace capability. */
		static bool const DebuggerTrace = DebuggerStreams > 0 && DebuggerMacro > 0;

		/*! \brief When \c true, all streams (including) trace are compressed using stored::CompressLayer. */
		static bool const CompressStreams =
#ifdef STORED_HAVE_HEATSHRINK
			true;
#else
			false;
#endif

	};
} // namespace
#endif // __cplusplus

#include "stored_config.h"

#endif // LIBSTORED_CONFIG_H

