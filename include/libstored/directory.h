#ifndef __LIBSTORED_DIRECTORY_H
#define __LIBSTORED_DIRECTORY_H

#include <libstored/types.h>

#include <string>

/*
end ::= 0
char ::= [\x21..\x7e]
byte_high ::= [0x80..0xff]			# 7 lsb carry data
byte_low ::= [0..0x7f]				# 7 lsb carry data
int_bin ::= byte_high * byte_low	# little-endian
jmp_l ::= int_bin
jmp_g :: int_bin
var ::= type offset | (String | Blob) byte offset
type ::= [0x80..0xff]
offset ::= int_bin
expr ::= '/' expr | char jmp_l jmp_g expr expr ? expr ? | var | end
*/

#ifdef __cplusplus
namespace stored {

	struct directoryHelper {
		static inline size_t decodeOffset(uint8_t const*& p) {
			size_t v = 0;
			while(*p & 0x80) {
				v = (v << 7) | (*p & 0x7f);
				p++;
			}
			return v | *p++;
		}

		static void skipOffset(uint8_t const*& p) {
			while(*p++ & 0x80);
		}
	};

	template <typename Container>
	Variant<Container> find(Container& container, void* buffer, uint8_t const* directory, char const* name) {
		if(unlikely(!directory || !name))
			return Variant<Container>();

		uint8_t const* p = directory;
		while(true) {
			if(*p == '/') {
				// Skip till next /
				while(*name++ != '/') { if(!*name) return Variant<Container>(); }
				p++;
			} else if(*p == 0) {
				// end
				return Variant<Container>();
			} else if(*p >= 0x80) {
				// var
				Type::type type = (Type::type)(*p++ ^ 0x80);
				uint8_t len;
				if(!Type::isFixed(type))
					len = *p++;
				size_t offset = directoryHelper::decodeOffset(p);
				if(Type::isFunction(type))
					return Variant<Container>(container, type, offset);
				else
					return Variant<Container>(container, type, (char*)buffer + offset, len);
			} else if(!*name) {
				return Variant<Container>();
			} else {
				// match char
				int c = (int)*name - (int)*p++;
				if(c < 0) {
					// take jmp_l
					p += directoryHelper::decodeOffset(p) - 1;
				} else {
					directoryHelper::skipOffset(p);
					if(c > 0) {
						// take jmp_g
						p += directoryHelper::decodeOffset(p) - 1;
					} else {
						// equal
						directoryHelper::skipOffset(p);
						name++;
					}
				}
			}
		}
	}

	namespace impl {
		using stored::Type;
		using stored::directoryHelper;

		template <typename Container, typename F>
		void list(Container& container, void* buffer, uint8_t const* directory, F& f, std::string& name)
		{
			if(unlikely(!buffer || !directory))
				return;

			uint8_t const* p = directory;
			size_t erase = 0;

			while(true) {
				if(*p == '/') {
					// Hierarchy separator.
					name.push_back('/');
					erase++;
					p++;
				} else if(*p == 0) {
					// end
					break;
				} else if(*p >= 0x80) {
					// var
					Type::type type = (Type::type)(*p++ ^ 0x80);
					size_t len = 0;
					if(!Type::isFixed(type))
						len = (size_t)*p++;
					else
						len = Type::size(type);

					size_t offset = directoryHelper::decodeOffset(p);
					if(Type::isFunction(type))
						f((char const*)name.c_str(), type, (char*)offset, 0);
					else
						f((char const*)name.c_str(), type, (char*)buffer + offset, len);
					break;
				} else {
					// next char in name
					char c = *p++;

					// take jmp_l
					size_t jmp = directoryHelper::decodeOffset(p);
					list(container, buffer, p + jmp - 1, f, name);

					// take jmp_g
					jmp = directoryHelper::decodeOffset(p);
					list(container, buffer, p + jmp - 1, f, name);

					// resume with this char
					name.push_back(c);
					erase++;
				}
			}

			if(erase)
				name.erase(name.end() - erase, name.end());
		}
	}

	template <typename Container, typename F>
	void list(Container& container, void* buffer, uint8_t const* directory, F& f) {
		std::string name;
		impl::list<Container,F>(container, buffer, directory, f, name);
	}

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DIRECTORY_H
