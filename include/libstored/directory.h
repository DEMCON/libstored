#ifndef __LIBSTORED_DIRECTORY_H
#define __LIBSTORED_DIRECTORY_H

#include <libstored/types.h>

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
	Variant<Container> find(Container& container, uint8_t const* directory, char const* name) {
		if(unlikely(!directory || !name))
			return Variant<Container>();

		uint8_t const* p = directory;
		while(true) {
			if(*p == '/') {
				// Skip till next /
				while(*name != '/') { if(!*name++) return Variant<Container>(); }
				p++;
			} else if(*p == 0) {
				// end
				return Variant<Container>();
			} else if(*p >= 0x80) {
				// var
				uint8_t type = *p++ ^ 0x80;
				uint8_t len;
				if(!Type::isFixed(type))
					len = *p++;
				size_t offset = directoryHelper::decodeOffset(++p);
				if(Type::isFunction(type))
					return Variant<Container>(container, type, offset);
				else
					return Variant<Container>(container, type, data + offset, len);
			} else {
				// match char
				int c = (int)*name - (int)*p++;
				if(c < 0) {
					// take jmp_l
					p += directoryHelper::decodeOffset(p);
				} else {
                    directoryHelper::skipOffset(p);
					if(c > 0) {
						// take jmp_g
						p += directoryHelper::decodeOffset(p);
					} else {
						// equal
                        directoryHelper::skipOffset(p);
					}
				}
			}
		}

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DIRECTORY_H
