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

#if STORED_cplusplus >= 201103L
	/*!
	 * \brief Iterates over all objects in the directory and invoke a callback for every object.
	 * \param container the container that contains \p buffer and \p directory
	 * \param buffer the buffer that holds the data of all variables
	 * \param directory the binary directory, to be parsed
	 * \param f the callback function, which receives the container, name, type, buffer, and size
	 */
	template <typename Container, typename F>
	SFINAE_IS_FUNCTION(F, void(Container*, char const*, Type::type, void*, size_t), void)
	list(Container* container, void* buffer, uint8_t const* directory, F&& f) {
		typedef void(ListCallback)(Container*, char const*, Type::type, void*, size_t);
		std::function<ListCallback> f_ = f;
		auto cb = [](void* container_, char const* name, Type::type type, void* buffer_, size_t len, void* f__) {
			(*(std::function<ListCallback>*)f__)((Container*)container_, name, type, buffer_, len);
		};
		list(container, buffer, directory, static_cast<ListCallbackArg*>(cb), &f_);
	}
#endif

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DIRECTORY_H
