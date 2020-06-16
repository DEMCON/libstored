#ifndef __LIBSTORED_DIRECTORY_H
#define __LIBSTORED_DIRECTORY_H

#include <libstored/types.h>
#include <libstored/util.h>

/*!
 * \defgroup libstored_directory directory
 * \brief Directory with names, types and buffer offsets.
 *
 * The directory is a description in binary. While parsing the pointer starts at the 
 * beginning of the directory and scans over the bytes. While scanning, a name is searched.
 * In principle, the directory is a binary tree of characters the name must match.
 *
 * It is using the following grammar:
 *
 * \code
 * directory ::= expr
 *
 * expr ::=
 *      # Hierarchy separator: skip all characters of the name until a '/' is encountered.
 *      '/' expr |
 *      # Match current char in the name. If it compress less or greater, add the jmp_l or
 *      # jmp_g to the pointer. Otherwise continue with the first expression.
 *      # If there is no object for a specific jump, jmp_* can be 0, in which case the 
 *      # expr_* is omitted.
 *      char jmp_l jmp_g expr expr_l ? expr_g ? |
 *      # Skip the next n non-/ characters of the name.
 *      skip expr |
 *      # A variable has been reached for the given name.
 *      var |
 *      # No variable exists with the given name.
 *      end
 *      # Note that expr does never start with \x7f (DEL).
 *      # Let's say, it is reserved for now.
 *
 * char ::= [\x20..\x2e,\x30..\x7e]     # printable ASCII, except '/'
 * int ::= bytehigh * bytelow           # Unsigned VLQ
 * byte ::= [0..0xff]
 * bytehigh ::= [0x80..0xff]			# 7 lsb carry data
 * bytelow ::= [0..0x7f]				# 7 lsb carry data
 *
 * # End of directory marker.
 * end ::= 0
 *
 * # The jmp is added to the pointer at the position of the last byte of the int.
 * # So, a jmp value of 0 effectively results in end.
 * jmp ::= int
 *
 * var ::= (String | Blob) size offset | type offset
 * type ::= [0x80..0xff]                # This is stored::Type::type with 0x80 or'ed into it.
 * size ::= int
 * offset ::= int
 *
 * skip ::= [1..0x1f]
 * \endcode
 */

#ifdef __cplusplus
namespace stored {

	namespace impl {
		Variant<> find(void* buffer, uint8_t const* directory, char const* name, size_t len = std::numeric_limits<size_t>::max());
	}

	/*!
	 * \ingroup libstored_directory
	 */
	template <typename Container>
	Variant<Container> find(Container& container, void* buffer, uint8_t const* directory, char const* name, size_t len = std::numeric_limits<size_t>::max()) {
		return impl::find(buffer, directory, name, len).apply<Container>(container);
	}

	typedef void(ListCallbackArg)(void*, char const*, Type::type, void*, size_t, void*);
	typedef void(ListCallback)(void*, char const*, Type::type, void*, size_t);

#if __cplusplus >= 201103L
	/*!
	 * \ingroup libstored_directory
	 */
	template <typename Container, typename F>
	SFINAE_IS_FUNCTION(F, void(Container*, char const*, Type::type, void*, size_t), void)
	list(Container* container, void* buffer, uint8_t const* directory, F& f) {
		auto cb = [](void* container, char const* name, Type::type type, void* buffer, size_t len, void* f) {
			(*(F*)f)((Container*)container, name, type, buffer, len);
		};
		list(container, buffer, directory, static_cast<ListCallbackArg*>(cb), &f);
	}
#endif

	void list(void* container, void* buffer, uint8_t const* directory, ListCallback* f);
	void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr);

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DIRECTORY_H
