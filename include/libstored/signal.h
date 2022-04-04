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

#if defined(__cplusplus) && STORED_cplusplus >= 201103L

#	include <libstored/allocator.h>
#	include <libstored/util.h>
#	include <libstored/types.h>

#	include <type_traits>
#	include <utility>

namespace stored {

namespace impl {

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
constexpr T noKey() noexcept
{
	return static_cast<T>(-1);
}

template <typename T, typename std::enable_if<!std::is_arithmetic<T>::value, int>::type = 0>
constexpr T noKey() noexcept
{
	return T{};
}

} // namespace impl

template <typename Key = void*, typename Token = void*>
class Signal {
public:
	using callback_type = void();
	using key_type = Key;
	using token_type = Token;

	using Callable_type = typename Callable<callback_type>::type;
	using Connection = std::pair<token_type, Callable_type>;
	using ConnectionMap = typename UnorderedMultiMap<key_type, Connection>::type;
	using size_type = typename ConnectionMap::size_type;

	static constexpr key_type NoKey = impl::noKey<Key>();
	static constexpr token_type NoToken = Token{};

	Signal() = default;

	explicit Signal(size_type bucket_count)
		: m_connections{bucket_count}
	{}

	template <typename F, SFINAE_IS_FUNCTION(F, callback_type, int) = 0>
	void connect(key_type key, F&& f, token_type token = NoToken)
	{
		stored_assert(key != NoKey);
		connect_(key, std::forward<F>(f), token);
	}

	template <typename F, SFINAE_IS_FUNCTION(F, callback_type, int) = 0>
	void connect(F&& f, token_type token = NoToken)
	{
		connect_(NoKey, std::forward<F>(f), token);
	}

	bool connected(key_type key) const
	{
		return m_connections.find(key) != m_connections.end();
	}

	bool connected() const noexcept
	{
		return !m_connections.empty();
	}

	void disconnect()
	{
		m_connections.clear();
	}

	void disconnect(key_type key)
	{
		stored_assert(key != NoKey);

		// Ignore token; erase all for given key.
		m_connections.erase(key);
	}

	void disconnect(key_type key, token_type token)
	{
		stored_assert(key != NoKey);

		// Only erase specific key/token.
		auto range = m_connections.equal_range(key);

		for(auto it = range.first; it != range.second;)
			if(it->second.first == token)
				// Erase does not invalidate other iterators.
				m_connections.erase(it++);
			else
				++it;
	}

	void call(Key key) const
	{
		stored_assert(key != NoKey);

		// The lookup is linear in the number of connected
		// functions with this key.
		auto range = m_connections.equal_range(key);
		for(auto it = range.first; it != range.second; ++it)
			it->second.second();
	}

	void operator()(Key key) const
	{
		call(key);
	}

	void call() const
	{
		for(size_type b = 0; b < m_connections.bucket_count(); ++b)
			for(auto it = m_connections.begin(b); it != m_connections.end(b); ++it)
				it->second.second();
	}

	void operator()() const
	{
		call();
	}

	void reserve(size_type count)
	{
		m_connections.reserve(count);
	}

protected:
	template <typename F>
	void connect_(key_type key, F&& f, token_type token)
	{
		m_connections.emplace(key, Connection{token, Callable_type{std::forward<F>(f)}});
	}

private:
	ConnectionMap m_connections;
};

/*!
 * \brief A wrapper that allows calling a function when a variable changes.
 *
 * It maintains a single std::unordered_multiset from a registered variable key
 * to a function.
 */
template <typename Base>
class Signalling : public Base {
	STORE_WRAPPER_CLASS(Signalling, Base)
public:
	using Key = typename Base::Key;
	using Signal_type = Signal<Key>;
	using Token = typename Signal_type::token_type;
	using callback_type = typename Signal_type::callback_type;

protected:
	template <typename... Arg>
	explicit Signalling(Arg&&... arg)
		: Base{std::forward<Arg>(arg)...}
	{
		static_assert(Config::EnableHooks, "Hooks are required for Signalling to work");
	}

public:
	template <
		typename Store, typename Implementation, typename T, size_t offset, size_t size_,
		typename F, SFINAE_IS_FUNCTION(F, callback_type, int) = 0,
		typename std::enable_if<std::is_base_of<Store, Base>::value, int>::type = 0>
	void
	connect(impl::StoreVariable<Store, Implementation, T, offset, size_>& var, F&& f,
		Token token = Signal_type::NoToken)
	{
		// Check validity and if we actually own this variable.
		stored_assert(this->bufferToKey(var.variant().buffer()) == var.key());
		m_signal.connect(var.key(), std::forward<F>(f), token);
	}

	template <
		typename Store, typename Implementation, Type::type type_, size_t offset,
		size_t size_, typename F, SFINAE_IS_FUNCTION(F, callback_type, int) = 0,
		typename std::enable_if<std::is_base_of<Store, Base>::value, int>::type = 0>
	void
	connect(impl::StoreVariantV<Store, Implementation, type_, offset, size_>& var, F&& f,
		Token token = Signal_type::NoToken)
	{
		// Check validity and if we actually own this variable.
		stored_assert(this->bufferToKey(var.buffer()) == var.key());
		m_signal.connect(var.key(), std::forward<F>(f), token);
	}

	template <typename Store, typename Implementation, typename T, size_t offset, size_t size_>
	void disconnect(
		impl::StoreVariable<Store, Implementation, T, offset, size_>& var,
		Token token = Signal_type::NoToken)
	{
		m_signal.disconnect(var.key(), token);
	}

	template <
		typename Store, typename Implementation, Type::type type_, size_t offset,
		size_t size_>
	void disconnect(
		impl::StoreVariantV<Store, Implementation, type_, offset, size_>& var,
		Token token = Signal_type::NoToken)
	{
		m_signal.disconnect(var.key(), token);
	}

	void __hookExitX(Type::type type, void* buffer, size_t len, bool changed) noexcept
	{
		if(changed) {
			// The lookup is linear in the number of connected
			// functions with this key.
			m_signal(this->bufferToKey(buffer));
		}

		Base::__hookExitX(type, buffer, len, changed);
	}

private:
	Signal_type m_signal;
};

} // namespace stored

#endif // __cplusplus
#endif // LIBSTORED_SIGNAL_H
