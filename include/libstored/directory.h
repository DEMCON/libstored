#ifndef LIBSTORED_DIRECTORY_H
#define LIBSTORED_DIRECTORY_H
// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/types.h>
#include <libstored/util.h>

#ifdef __cplusplus
namespace stored {

namespace impl {
/*!
 * \brief Decodes an Unsigned VLQ number.
 * \param p a pointer to the buffer to decode, which is incremented till after the decoded value
 * \return the decoded value
 * \private
 */
template <typename T>
constexpr14 T decodeInt(uint8_t const*& p) noexcept
{
	T v = 0;
	while(*p & 0x80U) {
		v = (v | (T)(*p & 0x7fU)) << 7U;
		p++;
	}
	return v | (T)*p++;
}

/*!
 * \brief Skips an Unsigned VLQ number, which encodes a buffer offset.
 * \param p a pointer to the offset to be skipped, which is increment till after the offset
 * \private
 */
constexpr14 void skipOffset(uint8_t const*& p) noexcept
{
	while(*p++ & 0x80U)
		;
}

/*!
 * \brief Finds an object in a directory.
 *
 * Don't call this function, use stored::find() instead.
 *
 * \param directory the binary directory description
 * \param name the name to find, can be abbreviated as long as it is unambiguous
 * \param len the maximum length of \p name to parse
 * \return a container-independent variant, which is not valid when not found
 * \see #stored::find()
 * \private
 */
#  if defined(STORED_ENABLE_UBSAN) \
	  && (defined(STORED_COMPILER_GCC) || defined(STORED_COMPILER_CLANG))
// Somehow, ubsan thinks that we are working outside of the directory definition.
#    if GCC_VERSION < 80000L
#      pragma GCC diagnostic push
#      pragma GCC diagnostic ignored "-Wattributes"
#    endif
#    if defined(STORED_COMPILER_CLANG)
#      pragma clang diagnostic push
#      pragma clang diagnostic ignored "-Wunknown-sanitizers"
#    endif
__attribute__((no_sanitize("pointer-overflow")))
#  endif
constexpr14 Variant<>
find(uint8_t const* directory, char const* name,
     size_t len = std::numeric_limits<size_t>::max()) noexcept
{
	Variant<> const notfound;

	if(unlikely(!directory || !name))
		return notfound;

	uint8_t const* p = directory;
	while(true) {
		bool nameEnd = len == 0 || !*name;
		if(*p == 0) {
			// end
			break;
		} else if(*p >= 0x80U) {
			// var
			Type::type type = (Type::type)(*p++ ^ 0x80U);
			size_t datalen =
				!Type::isFixed(type) ? decodeInt<size_t>(p) : Type::size(type);
			size_t offset = decodeInt<size_t>(p);
			return Variant<>(type, (uintptr_t)offset, datalen);
		} else if(*p <= 0x1f) {
			// skip
			if(nameEnd)
				break;

			uint8_t skip = 0;
			for(skip = *p++; skip > 0 && len > 0 && *name && *name != '/';
			    skip--, name++, len--)
				;

			if(skip > 0)
				// Premature end of name
				break;
		} else if(*p == '/') {
			// Skip till next /
			while(len > 0 && *name) {
				len--;
				if(*name++ == '/')
					break;
			}

			p++;
		} else {
			// match char
			int c = (nameEnd ? 0 : (int)*name) - (int)*p++;
			if(c < 0) {
				// take jmp_l

				// This early-break is not required. However, clang and
				// MSVC seems to have issues in evaluating constexpr,
				// specifically when evaluating the end marker.
				if(*p == 0)
					break;

				p += decodeInt<uintptr_t>(p) - 1;
			} else {
				skipOffset(p);
				if(c > 0) {
					// take jmp_g

					// See above.
					if(*p == 0)
						break;

					p += decodeInt<uintptr_t>(p) - 1;
				} else {
					// equal
					skipOffset(p);
					name++;
					len--;
				}
			}
		}
	}

	return Variant<>();
}
#  if defined(STORED_ENABLE_UBSAN) \
	  && (defined(STORED_COMPILER_GCC) || defined(STORED_COMPILER_CLANG))
#    if defined(STORED_COMPILER_CLANG)
#      pragma clang diagnostic pop
#    endif
#    if GCC_VERSION < 80000L
#      pragma GCC diagnostic pop
#    endif
#  endif
} // namespace impl

/*!
 * \brief Finds an object in a directory.
 * \param container the container that has the \p buffer and \p directory.
 *	Specify the actual (lowest) subclass of the store.
 * \param directory the binary directory description
 * \param name the name to find, can be abbreviated as long as it is
 *	unambiguous
 * \param len the maximum length of \p name to parse
 * \return a variant, which is not valid when not found
 */
template <typename Container>
constexpr14 Variant<Container>
find(Container& container, uint8_t const* directory, char const* name,
     size_t len = std::numeric_limits<size_t>::max()) noexcept
{
	return impl::find(directory, name, len).apply<Container>(container);
}

/*!
 * \brief Function prototype for the callback of #list().
 *
 * It receives the pointer to its container, the name of the object,
 * the type of the object, the pointer to the buffer, the length of the buffer,
 * and the \c arg parameter of \c list().
 */
typedef void(ListCallbackArg)(void*, char const*, Type::type, void*, size_t, void*);

void list(
	void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f,
	void* arg = nullptr, char const* prefix = nullptr);

void list(
	void* container, void* buffer, uint8_t const* directory, String::type* nameBuffer,
	ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr);

#  if STORED_cplusplus >= 201103L
/*!
 * \brief Iterates over all objects in the directory and invoke a callback for every object.
 * \param container the container that contains \p buffer and \p directory
 * \param buffer the buffer that holds the data of all variables
 * \param directory the binary directory, to be parsed
 * \param f the callback function, which receives the container, name, type, buffer, and size
 */
template <typename Container, typename F>
SFINAE_IS_FUNCTION(F, void(Container*, char const*, Type::type, void*, size_t), void)
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
list(Container* container, void* buffer, uint8_t const* directory, F&& f)
{
	auto cb = [](void* container_, char const* name, Type::type type, void* buffer_, size_t len,
		     void* f_) {
		(*static_cast<typename std::decay<F>::type*>(f_))(
			(Container*)container_, name, type, buffer_, len);
	};
	list(container, buffer, directory, static_cast<ListCallbackArg*>(cb), &f);
}
#  endif

} // namespace stored
#endif // __cplusplus
#endif // LIBSTORED_DIRECTORY_H
