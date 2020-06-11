#ifndef __LIBSTORED_DIRECTORY_H
#define __LIBSTORED_DIRECTORY_H

#include <libstored/types.h>

/*
end ::= 0
char ::= [\x21..\x7e]
byte_high ::= [0x80..0xff]
byte_low ::= [0..0x7f]
int_bin ::= byte_high * byte_low	# little-endian
jmp_l ::= int_bin
jmp_g :: int_bin
var ::= type offset | (String | Blob) byte offset
type ::= [0x80..0xff]
offset ::= int_bin
expr ::= '/' expr | char jmp_l jmp_g expr expr ? expr ? | var | end  # if jmp is 0, this is effectively end
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
				size_t offset = directoryHelper::decodeOffset(++p);
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

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DIRECTORY_H
