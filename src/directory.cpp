#include <libstored/directory.h>

#include <string>

template <typename T>
static T decodeInt(uint8_t const*& p) {
	T v = 0;
	while(*p & 0x80u) {
		v = (v | (T)(*p & 0x7fu)) << 7u;
		p++;
	}
	return v | (T)*p++;
}

static void skipOffset(uint8_t const*& p) {
	while(*p++ & 0x80u);
}

namespace stored {
namespace impl {

Variant<> find(void* buffer, uint8_t const* directory, char const* name, size_t len) {
	if(unlikely(!directory || !name)) {
notfound:
		return Variant<>();
	}

	uint8_t const* p = directory;
	while(true) {
		if(*p == 0) {
			// end
			goto notfound;
		} else if(*p >= 0x80u) {
			// var
			Type::type type = (Type::type)(*p++ ^ 0x80u);
			size_t len = !Type::isFixed(type) ? decodeInt<size_t>(p) : Type::size(type);
			size_t offset = decodeInt<size_t>(p);
			if(Type::isFunction(type))
				return Variant<>(type, (unsigned int)offset);
			else
				return Variant<>(type, static_cast<char*>(buffer) + offset, len);
		} else if(!*name || len == 0) {
			goto notfound;
		} else if(*p <= 0x1f) {
			// skip
			for(uint8_t i = *p++; i > 0 && len > 0; i--, name++, len--) {
				switch(*name) {
				case '\0':
				case '/':
					goto notfound;
				default:;
				}
			}
		} else if(*p == '/') {
			// Skip till next /
			while(len-- > 0 && *name++ != '/')
				if(!*name)
					goto notfound;
			p++;
		} else {
			// match char
			int c = (int)*name - (int)*p++;
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
 * \ingroup libstored_directory
 */
void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg, char const* prefix) {
	std::string name;
	if(prefix)
		name = prefix;
	list(container, buffer, directory, f, arg, name);
}

} // namespace

