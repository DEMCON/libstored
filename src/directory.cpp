// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/directory.h>

#include <string>

using stored::impl::decodeInt;

namespace stored {

/*!
 * \brief Implementation for stored::list().
 * \private
 */
static void
list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg,
     String::type* name)
{
	if(unlikely(!buffer || !directory))
		return;

	uint8_t const* p = directory;
	size_t erase = 0;

	while(true) {
		if(*p == 0) {
			// end
			break;
		} else if(*p >= 0x80) {
			// var
			Type::type type = (Type::type)(*p++ ^ 0x80U);
			size_t len = !Type::isFixed(type) ? decodeInt<size_t>(p) : Type::size(type);
			size_t offset = decodeInt<size_t>(p);
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
			char* b = Type::isFunction(type) ? reinterpret_cast<char*>(offset)
							 : static_cast<char*>(buffer) + offset;
			f(container, name ? name->c_str() : nullptr, type, b, len, arg);
			break;
		} else if(*p <= 0x1f) {
			// skip
			if(name)
				name->append(*p, '?');
			erase += *p;
			p++;
		} else if(*p == '/') {
			// Hierarchy separator.
			if(name)
				name->push_back('/');
			erase++;
			p++;
		} else {
			// next char in name
			char c = (char)*p++;

			// take jmp_l
			uintptr_t jmp = decodeInt<uintptr_t>(p);
			list(container, buffer, p + jmp - 1, f, arg, name);

			// take jmp_g
			jmp = decodeInt<uintptr_t>(p);
			list(container, buffer, p + jmp - 1, f, arg, name);

			// resume with this char
			if(name)
				name->push_back(c);
			erase++;
		}
	}

	if(name && erase)
		name->erase(name->end() - (long)erase, name->end());
}

/*!
 * \brief Iterates over all objects in the directory and invoke a callback for
 *	every object.
 * \param container the container that contains \p buffer and \p directory
 * \param buffer the buffer that holds the data of all variables
 * \param directory the binary directory, to be parsed
 * \param f the callback function
 * \param arg an arbitrary argument to be passed to \p f
 * \param prefix when not \c nullptr, a string that is prepended for the name
 *	that is supplied to \p f
 *
 * This function is not reentrant. Do not call it recursively.
 */
void list(
	void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg,
	char const* prefix)
{
	static STORED_thread_local String::type name;
	if(Config::AvoidDynamicMemory)
		name.reserve(
			(prefix ? strlen(prefix) * 2U : 0U)
			+ 128U); // Some arbitrary buffer, which should usually be sufficient.

	if(prefix)
		name = prefix;

	list(container, buffer, directory, f, arg, &name);
}

/*!
 * \brief Iterates over all objects in the directory and invoke a callback for
 *	every object.
 * \param container the container that contains \p buffer and \p directory
 * \param buffer the buffer that holds the data of all variables
 * \param directory the binary directory, to be parsed
 * \param nameBuffer use the provided buffer to assemble the object's names in.
 *	Set to \c nullptr to skip name generation. In that case, \p f will
 *	receive \c nullptr as name.
 * \param f the callback function
 * \param arg an arbitrary argument to be passed to \p f
 * \param prefix when not \c nullptr, a string that is prepended for the name
 *	that is supplied to \p f
 *
 * This function is not reentrant, unless \p nameBuffer is specified as \c
 * nullptr.
 */
void list(
	void* container, void* buffer, uint8_t const* directory, String::type* nameBuffer,
	ListCallbackArg* f, void* arg, char const* prefix)
{
	if(nameBuffer && prefix)
		*nameBuffer = prefix;

	list(container, buffer, directory, f, arg, nameBuffer);
}

} // namespace stored
