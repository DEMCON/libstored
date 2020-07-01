#ifndef __LIBSTORED_DIRECTORY_H
#define __LIBSTORED_DIRECTORY_H
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
 * \code{.unparsed}
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
 *
 * \ingroup libstored
 */

#ifdef __cplusplus
namespace stored {

	namespace impl {
		Variant<> find(void* buffer, uint8_t const* directory, char const* name, size_t len = std::numeric_limits<size_t>::max());
	}

	/*!
	 * \brief Finds an object in a directory.
	 * \param container the container that has the \p buffer and \p directory. Specify the actual (lowest) subclass of the store.
	 * \param buffer the buffer with variable's data
	 * \param directory the binary directory description
	 * \param name the name to find, can be abbreviated as long as it is unambiguous
	 * \param len the maximum length of \p name to parse
	 * \return a variant, which is not valid when not found
	 * \ingroup libstored_directory
	 */
	template <typename Container>
	Variant<Container> find(Container& container, void* buffer, uint8_t const* directory, char const* name, size_t len = std::numeric_limits<size_t>::max()) {
		return impl::find(buffer, directory, name, len).apply<Container>(container);
	}

	/*!
	 * \brief Function prototype for the callback of #list().
	 *
	 * It receives the pointer to its container, the name of the object,
	 * the type of the object, the pointer to the buffer, the length of the buffer,
	 * and the \c arg parameter of \c list().
	 */
	typedef void(ListCallbackArg)(void*, char const*, Type::type, void*, size_t, void*);
	
	void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr);

#if __cplusplus >= 201103L
	/*!
	 * \brief Iterates over all objects in the directory and invoke a callback for every object.
	 * \param container the container that contains \p buffer and \p directory
	 * \param buffer the buffer that holds the data of all variables
	 * \param directory the binary directory, to be parsed
	 * \param f the callback function, which receives the container, name, type, buffer, and size
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

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DIRECTORY_H
