#ifndef LIBSTORED_SIGNAL_H
#define LIBSTORED_SIGNAL_H
/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
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

#include <libstored/macros.h>

#if defined(__cplusplus) && STORED_cplusplus >= 201103L && defined(STORED_DRAFT_API)

#	include <libstored/allocator.h>
#	include <libstored/util.h>
#	include <libstored/types.h>

#	include <type_traits>
#	include <utility>

namespace stored {

/*!
 * \brief A wrapper that allow calling a function when a variable changes.
 *
 * It maintains a single std::unordered_multiset from a registered variable key
 * to a function.
 */
template <typename Base>
class Signalling : public Base {
	STORE_WRAPPER_CLASS(Signalling, Base)
public:
	using Callback_type = void();
	using Key = typename Base::Key;

protected:
	using Callable_type = Callable<Callback_type>::type;
	using Connection_type = std::pair<void*, Callable_type>;
	using ConnectionMap = typename UnorderedMultiMap<Key, Connection_type>::type;

	template <typename... Arg>
	explicit Signalling(Arg&&... arg)
		: Base{std::forward<Arg>(arg)...}
	{}

public:
	template <
		typename Store, typename Implementation, typename T, size_t offset, size_t size_,
		typename F, SFINAE_IS_FUNCTION(F, void(), int) = 0,
		typename std::enable_if<std::is_base_of<Store, Base>::value, int>::type = 0>
	void
	connect(impl::StoreVariable<Store, Implementation, T, offset, size_>& var, F&& f,
		void* token = nullptr)
	{
		// Check validity and if we actually own this variable.
		stored_assert(this->bufferToKey(var.variant().buffer()) == var.key());
		connect(var.key(), std::forward<F>(f), token);
	}

	template <
		typename Store, typename Implementation, Type::type type_, size_t offset,
		size_t size_, typename F, SFINAE_IS_FUNCTION(F, void(), int) = 0,
		typename std::enable_if<std::is_base_of<Store, Base>::value, int>::type = 0>
	void
	connect(impl::StoreVariantV<Store, Implementation, type_, offset, size_>& var, F&& f,
		void* token = nullptr)
	{
		// Check validity and if we actually own this variable.
		stored_assert(this->bufferToKey(var.buffer()) == var.key());
		connect(var.key(), std::forward<F>(f), token);
	}

	template <typename F>
	void connect(Key key, F&& f, void* token = nullptr)
	{
		m_connections.emplace(key, Connection_type{token, std::forward<F>(f)});
	}

	template <typename Store, typename Implementation, typename T, size_t offset, size_t size_>
	void disconnect(
		impl::StoreVariable<Store, Implementation, T, offset, size_>& var,
		void* token = nullptr)
	{
		disconnect(var.key(), token);
	}

	template <
		typename Store, typename Implementation, Type::type type_, size_t offset,
		size_t size_>
	void disconnect(
		impl::StoreVariantV<Store, Implementation, type_, offset, size_>& var,
		void* token = nullptr)
	{
		disconnect(var.key(), token);
	}

	void disconnect(Key key, void* token = nullptr)
	{
		if(!token) {
			// Ignore token; erase all.
			m_connections.erase(key);
		} else {
			auto range = m_connections.equal_range(key);

			for(auto it = range.first; it != range.second;)
				if(it->second.first == token)
					// Erase does not invalidate other iterators.
					m_connections.erase(it++);
				else
					++it;
		}
	}

	void __hookExitX(Type::type type, void* buffer, size_t len, bool changed) noexcept
	{
		if(changed) {
			// The lookup is linear in the number of connected
			// functions with this key.
			auto range = m_connections.equal_range(this->bufferToKey(buffer));
			for(auto it = range.first; it != range.second; ++it)
				it->second.second();
		}

		Base::__hookExitX(type, buffer, len, changed);
	}

private:
	ConnectionMap m_connections;
};

} // namespace stored

#endif // __cplusplus
#endif // LIBSTORED_SIGNAL_H
