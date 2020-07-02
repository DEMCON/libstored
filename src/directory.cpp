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

#include <libstored/directory.h>

#include <string>

/*!
 * \brief Decodes an Unsigned VLQ number.
 * \param p a pointer to the buffer to decode, which is incremented till after the decoded value
 * \return the decoded value
 */
template <typename T>
static T decodeInt(uint8_t const*& p) {
	T v = 0;
	while(*p & 0x80u) {
		v = (v | (T)(*p & 0x7fu)) << 7u;
		p++;
	}
	return v | (T)*p++;
}

/*!
 * \brief Skips an Unsigned VLQ number, which encodes a buffer offset.
 * \param p a pointer to the offset to be skipped, which is increment till after the offset
 */
static void skipOffset(uint8_t const*& p) {
	while(*p++ & 0x80u);
}

namespace stored {
namespace impl {

/*!
 * \brief Finds an object in a directory.
 * 
 * Don't call this function, use stored::find() instead.
 *
 * \param buffer the buffer with variable's data
 * \param directory the binary directory description
 * \param name the name to find, can be abbreviated as long as it is unambiguous
 * \param len the maximum length of \p name to parse
 * \return a container-independent variant, which is not valid when not found
 * \ingroup libstored_directory
 * \see #stored::find()
 * \private
 */
Variant<> find(void* buffer, uint8_t const* directory, char const* name, size_t len) {
	if(unlikely(!directory || !name)) {
notfound:
		return Variant<>();
	}

	uint8_t const* p = directory;
	while(true) {
		bool nameEnd = !*name || len == 0;
		if(*p == 0) {
			// end
			goto notfound;
		} else if(*p >= 0x80u) {
			// var
			Type::type type = (Type::type)(*p++ ^ 0x80u);
			size_t datalen = !Type::isFixed(type) ? decodeInt<size_t>(p) : Type::size(type);
			size_t offset = decodeInt<size_t>(p);
			if(Type::isFunction(type))
				return Variant<>(type, (unsigned int)offset);
			else
				return Variant<>(type, static_cast<char*>(buffer) + offset, datalen);
		} else if(*p <= 0x1f) {
			// skip
			if(nameEnd)
				goto notfound;

			uint8_t skip;
			for(skip = *p++; skip > 0 && len > 0 && *name; skip--, name++, len--) {
				switch(*name) {
				case '/':
					goto notfound;
				default:;
				}
			}

			if(skip > 0)
				// Premature end of name
				goto notfound;
		} else if(*p == '/') {
			// Skip till next /
			if(nameEnd)
				goto notfound;

			while(len-- > 0 && *name++ != '/')
				if(!*name)
					goto notfound;
			p++;
		} else {
			// match char
			int c = (nameEnd ? 0 : (int)*name) - (int)*p++;
			if(c < 0) {
				// take jmp_l
				p += decodeInt<uintptr_t>(p) - 1;
			} else {
				skipOffset(p);
				if(c > 0) {
					// take jmp_g
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
}

} // namespace impl

/*!
 * \brief Implementation for stored::list().
 * \private
 */
static void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg, std::string& name)
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
			char* b = Type::isFunction(type) ? nullptr : static_cast<char*>(buffer);
			f(container, name.c_str(), type, b + offset, len, arg);
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
 * \ingroup libstored_directory
 */
void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg, char const* prefix) {
	std::string name;
	if(prefix)
		name = prefix;
	list(container, buffer, directory, f, arg, name);
}

} // namespace

