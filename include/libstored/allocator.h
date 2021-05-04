#ifndef LIBSTORED_ALLOCATOR_H
#define LIBSTORED_ALLOCATOR_H
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

#include <libstored/config.h>

#ifdef __cplusplus
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace stored {

	/*!
	 * \brief Wrapper for Config::Allocator::type::allocate().
	 */
	template <typename T>
	__attribute__((warn_unused_result)) static inline T* allocate(size_t n = 1) {
#if STORED_cplusplus >= 201103L
		using Allocator = typename Config::Allocator<T>::type;
		Allocator a;
		return std::allocator_traits<Allocator>::allocate(a, n);
#else
		Config::Allocator<T>::type allocator;
		return allocator.allocate(n);
#endif
	}

	/*!
	 * \brief Wrapper for Config::Allocator::type::deallocate().
	 */
	template <typename T>
	static inline void deallocate(T* p, size_t n = 1) noexcept {
#if STORED_cplusplus >= 201103L
		using Allocator = typename Config::Allocator<T>::type;
		Allocator a;
		std::allocator_traits<Allocator>::deallocate(a, p, n);
#else
		Config::Allocator<T>::type allocator;
		allocator.deallocate(n);
#endif
	}

	/*!
	 * \brief Wrapper for Config::Allocator::type::deallocate() after destroying the given object.
	 */
	template <typename T>
	static inline void cleanup(T* p) noexcept {
		if(!p)
			return;

#if STORED_cplusplus >= 201103L
		using Allocator = typename Config::Allocator<T>::type;
		Allocator a;
		std::allocator_traits<Allocator>::destroy(a, p);
		std::allocator_traits<Allocator>::deallocate(a, p, 1U);
#else
		x.~T();
		deallocate<T>(&x);
#endif
	}

	/*!
	 * \brief libstored-allocator-aware \c std::deque.
	 */
	template <typename T>
	struct Deque {
		typedef typename std::deque<T, typename Config::Allocator<T>::type> type;
	};

	/*!
	 * \brief libstored-allocator-aware \c std::map.
	 */
	template <typename Key, typename T, typename Compare = std::less<Key> >
	struct Map {
		typedef typename std::map<Key, T, Compare, typename Config::Allocator<std::pair<Key const, T> >::type> type;
	};

	/*!
	 * \brief libstored-allocator-aware \c std::string.
	 */
	struct String {
		// The wrapping struct is not required, but consistent with the other types.
		typedef std::basic_string<char, std::char_traits<char>, Config::Allocator<char>::type> type;
	};

	/*!
	 * \brief libstored-allocator-aware \c std::vector.
	 */
	template <typename T>
	struct Vector {
		typedef typename std::vector<T, typename Config::Allocator<T>::type> type;
	};

} // namespace
#endif // __cplusplus
#endif // LIBSTORED_ALLOCATOR_H
