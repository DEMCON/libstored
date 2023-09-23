#ifndef LIBSTORED_CONFIG_H
#define LIBSTORED_CONFIG_H
// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/macros.h>

#ifdef __cplusplus
#	include <cstddef>
#	include <memory>

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
#	ifndef NDEBUG
		true;
#	else
		false;
#	endif

	/*!
	 * \brief When \c true, enable #stored_assert() checks.
	 */
	static bool const EnableAssert =
#	ifndef NDEBUG
		Debug;
#	else
		false;
#	endif

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
#	ifdef STORED_LITTLE_ENDIAN
		true;
#	else
		false;
#	endif

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

	/*!
	 * \brief When \c true, avoid dynamic memory reallocation where possible.
	 *
	 * The Allocator will still be used, but reallocation to dynamically
	 * sized buffers is avoided.  This implies that worst-case allocation
	 * may be done at startup.
	 */
	static bool const AvoidDynamicMemory =
#	if defined(STORED_OS_BAREMETAL) || defined(STORED_OS_GENERIC)
		true;
#	else
		false;
#	endif

	/*! \brief When \c true, stored::Debugger implements the read capability. */
	static bool const DebuggerRead = true;
	/*! \brief When \c true, stored::Debugger implements the write capability. */
	static bool const DebuggerWrite = true;
	/*! \brief When \c true, stored::Debugger implements the echo capability. */
	static bool const DebuggerEcho = true;
	/*! \brief When \c true, stored::Debugger implements the list capability. */
	static bool const DebuggerList = true;
	/*! \brief When \c true, stored::Debugger always lists the store prefix,
	 *         even if there is only one store mapped. */
	static bool const DebuggerListPrefixAlways = false;
	/*!
	 * \brief When not 0, stored::Debugger implements the alias capability.
	 *
	 * The defined number is the number of aliases that are supported at the same time.
	 */
	// Value is max number of aliases. Default is effectively no limit.
	static int const DebuggerAlias = 0x100;
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
	static bool const DebuggerReadMem =
#	if defined(STORED_OS_BAREMETAL)
		Debug;
#	else
		false;
#	endif

	/*! \brief When \c true, stored::Debugger implements the write memory capability. */
	static bool const DebuggerWriteMem =
#	if defined(STORED_OS_BAREMETAL)
		Debug;
#	else
		false;
#	endif

	/*!
	 * \brief When not 0, stored::Debugger implements the streams capability.
	 *
	 * The defined number is the number of concurrent streams that are supported.
	 */
	static int const DebuggerStreams =
		2; // by default two: one for the application, one for tracing
	/*! \brief Size of one stream buffer in bytes. */
	static size_t const DebuggerStreamBuffer = 1024;
	/*!
	 * \brief The maximum (expected) size the stream buffer may overflow.
	 *
	 * The trace uses a stream buffer. As long the buffer contents are
	 * below DebuggerStreamBuffer, another sample may be added. This may
	 * make the buffer overflow, resulting in a dynamic reallocation. To
	 * avoid realloc, a trace sample (after compression) should fit in
	 * DebuggerStreamBufferOverflow, which is a preallocated space on top
	 * of DebuggerStreamBuffer. As the trace sample size is application-
	 * dependent, this should be set appropriately. When set to small,
	 * realloc will happen anyway.
	 */
	static size_t const DebuggerStreamBufferOverflow =
#	ifndef DOXYGEN
		// Seems to be to hard to parse by doxygen/sphinx.
		AvoidDynamicMemory ? DebuggerStreamBuffer / 8 : 0;
#	else
		0;
#	endif

	/*! \brief When \c true, stored::Debugger implements the trace capability. */
	static bool const DebuggerTrace = DebuggerStreams > 0 && DebuggerMacro > 0;

	/*!
	 * \brief When \c true, all streams (including) trace are compressed using
	 *	stored::CompressLayer.
	 */
	static bool const CompressStreams =
#	ifdef STORED_HAVE_HEATSHRINK
		true;
#	else
		false;
#	endif

	/*!
	 * \brief Allocator to be used for all dynamic memory allocations.
	 *
	 * Define a similar struct with \c type member to override the default
	 * allocator.
	 *
	 * C++11's <tt>template &lt;typename T&gt; using Allocator = std::allocator&lt;T&gt;;</tt>
	 * would be nicer, but this construct works for all versions of C++.
	 */
	template <typename T>
	struct Allocator {
		typedef std::allocator<T> type;
	};

	/*!
	 * \brief Allow unaligned memory access.
	 */
	static bool const UnalignedAccess =
#	if defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)
		true;
#	else
		false;
#	endif
};
} // namespace stored
#endif // __cplusplus

#include "stored_config.h"

#endif // LIBSTORED_CONFIG_H
