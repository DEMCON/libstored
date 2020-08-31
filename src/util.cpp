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

#include <libstored/util.h>

#include <cstring>

namespace stored {

/*!
 * \brief Like \c ::strncpy(), but without padding and returning the length of the string.
 */
size_t strncpy(char* __restrict__ dst, char const* __restrict__ src, size_t len) {
	if(len == 0)
		return 0;

	stored_assert(dst);
	stored_assert(src);

	size_t res = 0;
	for(; res < len && src[res]; res++)
		dst[res] = src[res];

	return res;
}

/*!
 * \brief Like \c ::strncmp(), but handles non zero-terminated strings.
 */
int strncmp(char const* __restrict__ str1, size_t len1, char const* __restrict__ str2, size_t len2) {
	stored_assert(str1);
	stored_assert(str2);

	size_t i = 0;
	for(; i < len1 && i < len2; i++) {
		char c1 = str1[i];
		char c2 = str2[i];

		if(c1 < c2)
			return -1;
		else if(c1 > c2)
			return 1;
		else if(c1 == 0)
			// early-terminate same strings
			return 0;
	}

	if(len1 == len2)
		// same strings
		return 0;
	else if(i == len1)
		// str1 was shortest
		return str2[i] ? -1 : 0;
	else
		// str2 was shortest
		return str1[i] ? 1 : 0;
}

} // namespace

