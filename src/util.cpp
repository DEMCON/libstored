// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/poller.h>
#include <libstored/util.h>
#include <libstored/version.h>

#include <cstring>

#if STORED_cplusplus < 201103L
#	include <inttypes.h>
#else
#	include <cinttypes>
#endif

namespace stored {

/*!
 * \brief Like \c ::strncpy(), but without padding and returning the length of the string.
 */
size_t strncpy(char* __restrict__ dst, char const* __restrict__ src, size_t len) noexcept
{
	if(len == 0)
		return 0;

	stored_assert(dst);
	stored_assert(src);

	size_t res = 0;
	for(; res < len && src[res]; res++)
		dst[res] = src[res];

	return res;
}

/*!
 * \brief Like \c ::strncmp(), but handles non zero-terminated strings.
 */
int strncmp(
	char const* __restrict__ str1, size_t len1, char const* __restrict__ str2,
	size_t len2) noexcept
{
	stored_assert(str1);
	stored_assert(str2);

	size_t i = 0;
	for(; i < len1 && i < len2; i++) {
		char c1 = str1[i];
		char c2 = str2[i];

		if(c1 < c2)
			return -1;
		else if(c1 > c2)
			return 1;
		else if(c1 == 0)
			// early-terminate same strings
			return 0;
	}

	if(len1 == len2)
		// same strings
		return 0;
	else if(i == len1)
		// str1 was shortest
		return str2[i] ? -1 : 0;
	else
		// str2 was shortest
		return str1[i] ? 1 : 0;
}

/*!
 * \brief Swap endianness of the given buffer.
 */
void swap_endian(void* buffer, size_t len) noexcept
{
	char* buffer_ = static_cast<char*>(buffer);
	for(size_t i = 0; i < len / 2; i++) {
		char c = buffer_[i];
		buffer_[i] = buffer_[len - i - 1];
		buffer_[len - i - 1] = c;
	}
}

/*!
 * \brief \c memcpy() with endianness swapping.
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void memcpy_swap(void* __restrict__ dst, void const* __restrict__ src, size_t len) noexcept
{
	char* dst_ = static_cast<char*>(dst);
	char const* src_ = static_cast<char const*>(src);

	for(size_t i = 0; i < len; i++)
		dst_[i] = src_[len - i - 1];
}

/*!
 * \brief memcmp() with endianness swapping.
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int memcmp_swap(void const* a, void const* b, size_t len) noexcept
{
	unsigned char const* a_ = static_cast<unsigned char const*>(a);
	unsigned char const* b_ = static_cast<unsigned char const*>(b);

	size_t i = 0;
	for(; i < len; i++)
		if(a_[i] != b_[len - i - 1])
			goto diff;

	return 0;

diff:
	return a_[i] < b_[i] ? -1 : 1;
}

/*!
 * \brief Converts the given buffer to a string literal.
 *
 * This comes in handy for verbose output of binary data, like protocol messages.
 */
String::type string_literal(void const* buffer, size_t len, char const* prefix)
{
	String::type s;
	if(Config::AvoidDynamicMemory)
		s.reserve((prefix ? strlen(prefix) : 0U) + len * 4U);

	if(prefix)
		s += prefix;

	uint8_t const* b = static_cast<uint8_t const*>(buffer);
	char buf[16] = {};
	for(size_t i = 0; i < len; i++) {
		switch(b[i]) {
		case '\0':
			s += "\\0";
			break;
		case '\r':
			s += "\\r";
			break;
		case '\n':
			s += "\\n";
			break;
		case '\t':
			s += "\\t";
			break;
		case '\\':
			s += "\\\\";
			break;
		default:
			if(b[i] < 0x20 || b[i] >= 0x7f) {
				// Embedded systems may not have a fancy printf, so limit the
				// formatting specifier somewhat.
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
				// flawfinder: ignore
				if(snprintf(buf, sizeof(buf) - 1, "\\x%02x", (unsigned int)b[i])
				   > 0)
					s += buf;
			} else {
				s += (char)b[i];
			}
		}
	}

	return s;
}

/*!
 * \brief Return a single-line string that contains relevant configuration information of libstored.
 */
char const* banner() noexcept
{
	return "libstored " STORED_VERSION
#if STORED_cplusplus < 201103L
	       " C++98"
#elif STORED_cplusplus == 201103L
	       " C++11"
#elif STORED_cplusplus == 201402L
	       " C++14"
#elif STORED_cplusplus == 201703L
	       " C++17"
#endif
#ifdef STORED_COMPILER_CLANG
	       " clang"
#endif
#ifdef STORED_COMPILER_GCC
	       " gcc"
#endif
#ifdef STORED_COMPILER_ARMCC
	       " armcc"
#endif
#ifdef STORED_COMPILER_MSVC
	       " msvc"
#endif
#ifdef STORED_OS_WINDOWS
	       " win"
#endif
#ifdef STORED_OS_LINUX
	       " linux"
#endif
#ifdef STORED_OS_OSC
	       " osx"
#endif
#ifdef STORED_OS_BAREMETAL
	       " baremetal"
#endif
#ifdef STORED_OS_GENERIC
	       " generic"
#endif
#ifdef STORED_LITTLE_ENDIAN
	       " le"
#endif
#ifdef STORED_BIG_ENDIAN
	       " be"
#endif
#if defined(STORED_HAVE_VALGRIND) && !defined(NVALGRIND)
	       " valgrind"
#endif
#ifdef STORED_HAVE_ZTH
	       " zth"
#endif
#ifdef STORED_HAVE_ZMQ
	       " zmq"
#endif
#ifdef STORED_HAVE_QT
#	if STORED_HAVE_QT == 5
	       " qt5"
#	elif STORED_HAVE_QT == 6
	       " qt6"
#	else
	       " qt"
#	endif
#endif
#ifdef STORED_POLL_ZTH_WFMO
	       " poll=zth-wfmo"
#endif
#ifdef STORED_POLL_WFMO
	       " poll=wfmo"
#endif
#ifdef STORED_POLL_ZTH_ZMQ
	       " poll=zth-zmq"
#endif
#ifdef STORED_POLL_ZMQ
	       " poll=zmq"
#endif
#ifdef STORED_POLL_ZTH_POLL
	       " poll=zth-poll"
#endif
#ifdef STORED_POLL_POLL
	       " poll=poll"
#endif
#ifdef STORED_POLL_ZTH_LOOP
	       " poll=zth-loop"
#endif
#ifdef STORED_POLL_LOOP
	       " poll=loop"
#endif
#ifdef STORED_DRAFT_API
	       " draft"
#endif
#ifdef STORED_ENABLE_ASAN
	       " asan"
#endif
#ifdef STORED_ENABLE_LSAN
	       " lsan"
#endif
#ifdef STORED_ENABLE_UBSAN
	       " ubsan"
#endif
#ifdef _DEBUG
	       " debug"
#endif
		;
}

} // namespace stored
