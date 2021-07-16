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

#include <libstored/directory.h>

#include <string>

using stored::impl::decodeInt;

namespace stored {

/*!
 * \brief Implementation for stored::list().
 * \private
 */
static void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg, String::type& name)
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
			Type::type type = (Type::type)(*p++ ^ 0x80u);
			size_t len = !Type::isFixed(type) ? decodeInt<size_t>(p) : Type::size(type);
			size_t offset = decodeInt<size_t>(p);
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			char* b = Type::isFunction(type) ? reinterpret_cast<char*>(offset) : static_cast<char*>(buffer) + offset;
			f(container, name.c_str(), type, b, len, arg);
			break;
		} else if(*p <= 0x1f) {
			// skip
			name.append(*p, '?');
			erase += *p;
			p++;
		} else if(*p == '/') {
			// Hierarchy separator.
			name.push_back('/');
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
			name.push_back(c);
			erase++;
		}
	}

	if(erase)
		name.erase(name.end() - (long)erase, name.end());
}

/*!
 * \brief Iterates over all objects in the directory and invoke a callback for every object.
 * \param container the container that contains \p buffer and \p directory
 * \param buffer the buffer that holds the data of all variables
 * \param directory the binary directory, to be parsed
 * \param f the callback function
 * \param arg an arbitrary argument to be passed to \p f
 * \param prefix when not \c nullptr, a string that is prepended for the name that is supplied to \p f
 *
 * This function is not reentrant. Do not call it recursively.
 */
void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg, char const* prefix) {
	static String::type name;
	if(Config::AvoidDynamicMemory)
		name.reserve((prefix ? strlen(prefix) * 2U : 0U) + 128U); // Some arbitrary buffer, which should usually be sufficient.

	if(prefix)
		name = prefix;

	list(container, buffer, directory, f, arg, name);
}

} // namespace

