#ifndef LIBSTORED_ALLOCATOR_H
#define LIBSTORED_ALLOCATOR_H
// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/config.h>

#ifdef __cplusplus
#  include <algorithm>
#  include <deque>
#  include <list>
#  include <map>
#  include <memory>
#  include <set>
#  include <string>
#  include <vector>

#  if STORED_cplusplus >= 201103L
#    include <array>
#    include <functional>
#    include <unordered_map>
#    include <utility>
#  endif

namespace stored {

/*!
 * \brief Wrapper for Config::Allocator::type::allocate().
 */
template <typename T>
__attribute__((warn_unused_result)) static inline T* allocate(size_t n = 1)
{
#  if STORED_cplusplus >= 201103L
	using Allocator = typename Config::Allocator<T>::type;
	Allocator a;
	return std::allocator_traits<Allocator>::allocate(a, n);
#  else
	typename Config::Allocator<T>::type allocator;
	return allocator.allocate(n);
#  endif
}

/*!
 * \brief Wrapper for Config::Allocator::type::deallocate().
 */
template <typename T>
static inline void deallocate(T* p, size_t n = 1) noexcept
{
#  if STORED_cplusplus >= 201103L
	using Allocator = typename Config::Allocator<T>::type;
	Allocator a;
	std::allocator_traits<Allocator>::deallocate(a, p, n);
#  else
	typename Config::Allocator<T>::type allocator;
	allocator.deallocate(p, n);
#  endif
}

/*!
 * \brief Define new/delete operators for a class, which are allocator-aware.
 *
 * Put a call to this macro in the private section of your class
 * definition.  Additionally, make sure to have a virtual destructor to
 * deallocate subclasses properly.
 *
 * \param T the type of the class for which the operators are to be defined
 */
// cppcheck-suppress-macro duplInheritedMember
#  define STORED_CLASS_NEW_DELETE(T)                                          \
  public:                                                                     \
	  void* operator new(std::size_t n)                                   \
	  {                                                                   \
		  STORED_UNUSED(n)                                            \
		  stored_assert(n == sizeof(T));                              \
		  return ::stored::allocate<T>();                             \
	  }                                                                   \
	  void* operator new(std::size_t n, void* ptr) /* NOLINT */           \
	  {                                                                   \
		  STORED_UNUSED(n)                                            \
		  stored_assert(n == sizeof(T));                              \
		  return ptr;                                                 \
	  }                                                                   \
	  void operator delete(void* ptr)                                     \
	  {                                                                   \
		  ::stored::deallocate<T>(static_cast<T*>(ptr)); /* NOLINT */ \
	  }                                                                   \
                                                                              \
  private:

/*!
 * \brief Wrapper for Config::Allocator::type::deallocate() after destroying the given object.
 */
template <typename T>
static inline void cleanup(T* p) noexcept
{
	if(!p)
		return;

#  if STORED_cplusplus >= 201103L
	using Allocator = typename Config::Allocator<T>::type;
	Allocator a;
	std::allocator_traits<Allocator>::destroy(a, p);
	std::allocator_traits<Allocator>::deallocate(a, p, 1U);
#  else
	p->~T();
	deallocate<T>(p);
#  endif
}

/*!
 * \brief libstored-allocator-aware \c std::function-like type.
 */
#  if STORED_cplusplus < 201103L
template <typename F>
struct Callable {
	typedef F* type;
};
#  else // STORED_cplusplus >= 201103L
namespace impl {

// std::max() is not constexpr in C++11. So, implement it here (for what we need).
template <typename T>
constexpr T max(T a, T b) noexcept
{
	return b < a ? a : b;
}

template <typename T>
constexpr T max(T a, T b, T c) noexcept
{
	return max<T>(a, max<T>(b, c));
}

template <typename T>
struct CallableType {
	using type = typename std::decay<T>::type;
};

template <typename T>
struct CallableType<T&> {
	using type = T&;
};

template <>
struct CallableType<std::nullptr_t> {};

template <typename T>
struct CallableArgType {
	using type = typename std::remove_reference<T>::type const&;
};

template <typename T>
struct CallableArgType<T&> {
	using type = T&;
};

template <typename T>
struct CallableArgType<T&&> {
	using type = T&&;
};

template <typename R, typename... Args>
class Callable {
protected:
	// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
	class Base {
	public:
		virtual ~Base() noexcept = default;
		virtual R operator()(typename CallableArgType<Args>::type... args) const = 0;
		// NOLINTNEXTLINE(hicpp-explicit-conversions)
		virtual operator bool() const
		{
			return true;
		}
		virtual void clone(void* buffer) const = 0;
		virtual void move(void* buffer)
		{
			clone(buffer);
		}
	};

	// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
	class Reset final : public Base {
	public:
		// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
		virtual ~Reset() noexcept override = default;

		R operator()(typename CallableArgType<Args>::type... /*args*/) const final
		{
#    ifdef STORED_cpp_exceptions
			throw std::bad_function_call();
#    else
			std::terminate();
#    endif
		}

		// NOLINTNEXTLINE(hicpp-explicit-conversions)
		operator bool() const final
		{
			return false;
		}

		void clone(void* buffer) const final
		{
			new(buffer) Reset;
		}
	};

	template <typename F, bool Dummy = false>
	// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
	class Wrapper final : public Base {
	public:
		virtual ~Wrapper() noexcept override = default;

		template <
			typename F_,
			typename std::enable_if<
				!std::is_same<typename std::decay<F_>::type, Wrapper>::value,
				int>::type = 0>
		// NOLINTNEXTLINE(misc-forwarding-reference-overload,bugprone-forwarding-reference-overload)
		explicit Wrapper(F_&& f)
			: m_f{std::forward<F_>(f)}
		{}

		R operator()(typename CallableArgType<Args>::type... args) const final
		{
			return m_f(std::forward<typename CallableArgType<Args>::type>(args)...);
		}

		void clone(void* buffer) const final
		{
			new(buffer) Wrapper(m_f);
		}

		void move(void* buffer) final
		{
			new(buffer) Wrapper(std::move(m_f));
		}

	private:
		F m_f;
	};

	template <bool Dummy>
	// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
	class Wrapper<R(Args...), Dummy> final : public Base {
	public:
		virtual ~Wrapper() noexcept override = default;

		template <typename F_>
		explicit Wrapper(R (*f)(Args...)) noexcept
			: m_f{f}
		{}

		R operator()(typename CallableArgType<Args>::type... args) const final
		{
			return m_f(std::forward<typename CallableArgType<Args>::type>(args)...);
		}

		void clone(void* buffer) const final
		{
			new(buffer) Wrapper(m_f);
		}

	private:
		R (*m_f)(Args...);
	};

	template <typename T, bool Dummy>
	// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
	class Wrapper<T&, Dummy> final : public Base {
	public:
		virtual ~Wrapper() noexcept override = default;

		// cppcheck-suppress uninitMemberVar
		explicit Wrapper(T& obj) noexcept
			: m_obj{&obj}
		{}

		R operator()(typename CallableArgType<Args>::type... args) const final
		{
			return (*m_obj)(
				std::forward<typename CallableArgType<Args>::type>(args)...);
		}

		void clone(void* buffer) const final
		{
			new(buffer) Wrapper(*m_obj);
		}

	private:
		T* m_obj;
	};

	template <typename F>
	class Forwarder final : public Base {
	public:
		template <
			typename F_,
			typename std::enable_if<
				!std::is_same<typename std::decay<F_>::type, Forwarder>::value,
				int>::type = 0>
		// NOLINTNEXTLINE(misc-forwarding-reference-overload,bugprone-forwarding-reference-overload)
		explicit Forwarder(F_&& f)
			: m_w{new(allocate<Wrapper<F>>()) Wrapper<F>(std::forward<F_>(f))}
		{}

		explicit Forwarder(Wrapper<F>& w) noexcept
			: m_w{&w}
		{}

		Forwarder(Forwarder&& f) noexcept
			: m_w{}
		{
			(*this) = std::move(f);
		}

		Forwarder& operator=(Forwarder&& f) noexcept
		{
			cleanup(m_w);
			m_w = f.m_w;
			f.m_w = nullptr;
			return *this;
		}

		Forwarder(Forwarder const& f)
			: m_w{}
		{
			(*this) = f;
		}

		Forwarder& operator=(Forwarder const& f)
		{
			if(&f != this) {
				auto w = allocate<Wrapper<F>>();
				try {
					f.m_w->clone(w);
					m_w = w;
				} catch(...) {
					cleanup(w);
#    ifdef STORED_cpp_exceptions
					throw;
#    endif
				}
			}
			return *this;
		}

		virtual ~Forwarder() noexcept override
		{
			cleanup(m_w);
		}

		R operator()(typename CallableArgType<Args>::type... args) const final
		{
			return (*m_w)(std::forward<typename CallableArgType<Args>::type>(args)...);
		}

		void clone(void* buffer) const final
		{
			new(buffer) Forwarder(*this);
		}

		void move(void* buffer) final
		{
			new(buffer) Forwarder(std::move(*this));
		}

	private:
		Wrapper<F>* m_w;
	};

	using Buffer = std::array<
		char, max(sizeof(Reset), sizeof(Forwarder<R (*)(Args...)>),
			  sizeof(Wrapper<R (*)(Args...)>) + sizeof(void*))>;

public:
	explicit Callable()
	{
		construct<Reset>();
	}

	template <
		typename G, typename std::enable_if<
				    !std::is_same<typename std::decay<G>::type, Callable>::value,
				    int>::type = 0>
	// NOLINTNEXTLINE(misc-forwarding-reference-overload,bugprone-forwarding-reference-overload)
	explicit Callable(G&& g)
	{
		assign(std::forward<G>(g));
	}

	Callable(Callable const& c)
	{
		c.get().clone(m_buffer.data());
	}

	Callable(Callable&& c) noexcept
	{
		c.get().move(m_buffer.data());
		c = nullptr;
	}

	~Callable() noexcept
	{
		get().~Base();
	}

	R operator()(typename CallableArgType<Args>::type... args) const
	{
		return get()(std::forward<typename CallableArgType<Args>::type>(args)...);
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	operator bool() const noexcept
	{
		return get();
	}

	template <
		typename G, typename std::enable_if<
				    !std::is_same<typename std::decay<G>::type, Callable>::value,
				    int>::type = 0>
	Callable& operator=(G&& g)
	{
		replace(std::forward<G>(g));
		return *this;
	}

	Callable& operator=(Callable const& c)
	{
		if(&c != this) {
			destroy();
			try {
				c.get().clone(m_buffer.data());
			} catch(...) {
				construct<Reset>();
#    ifdef STORED_cpp_exceptions
				throw;
#    endif
			}
		}
		return *this;
	}

	Callable& operator=(Callable&& c) noexcept
	{
		destroy();
		c.get().move(m_buffer.data());
		c = nullptr;
		return *this;
	}

protected:
	Base const& get() const noexcept
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return *reinterpret_cast<Base const*>(m_buffer.data());
	}

	Base& get() noexcept
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return *reinterpret_cast<Base*>(m_buffer.data());
	}

	template <typename G, typename F_ = typename CallableType<G>::type>
	typename std::enable_if<(sizeof(Wrapper<F_>) > std::tuple_size<Buffer>::value)>::type
	/* void */
	assign(G&& g)
	{
		construct<Forwarder<F_>>(std::forward<G>(g));
	}

	template <typename G, typename F_ = typename CallableType<G>::type>
	typename std::enable_if<(sizeof(Wrapper<F_>) <= std::tuple_size<Buffer>::value)>::type
	/* void */
	assign(G&& g)
	{
		construct<Wrapper<F_>>(std::forward<G>(g));
	}

	void assign(R (*g)(Args...))
	{
		if(g)
			construct<Wrapper<R (*)(Args...)>>(g);
		else
			construct<Reset>();
	}

	void assign(std::nullptr_t)
	{
		construct<Reset>();
	}

	template <typename T, typename... TArgs>
	void construct(TArgs&&... args)
	{
		static_assert(sizeof(T) <= std::tuple_size<Buffer>::value, "");
		new(m_buffer.data()) T{std::forward<TArgs>(args)...};
	}

	void destroy() noexcept
	{
		get().~Base();
	}

	template <typename G>
	void replace(G&& g)
	{
		destroy();
		try {
			assign(std::forward<G>(g));
		} catch(...) {
			// Make sure we always have a valid instance in the buffer.
			construct<Reset>();
#    ifdef STORED_cpp_exceptions
			throw;
#    endif
		}
	}

private:
	alignas(sizeof(void*)) Buffer m_buffer;
};

} // namespace impl

template <typename F>
struct Callable {
	template <typename R, typename... Args>
	static impl::Callable<R, Args...> callable_impl(R(Args...));
	using type = decltype(callable_impl(std::declval<F>()));
};

/*!
 * \brief A RAII-style wrapper to call a specific function (usually a lambda)
 *        upon destruction.
 */
template <typename F>
class Cleanup {
public:
	constexpr explicit Cleanup(F&& f)
		: m_f{std::move(f)}
		, m_valid{true}
	{}

	~Cleanup()
	{
		cleanup();
	}

	Cleanup(Cleanup const&) = delete;
	void operator=(Cleanup const&) = delete;

	Cleanup(Cleanup&& c) noexcept
	{
		*this = std::move(c);
	}

	Cleanup& operator=(Cleanup&& c) noexcept
	{
		cleanup();

		m_valid = c.m_valid;
		if(m_valid) {
			m_f = std::move(c.m_f);
			c.m_valid = false;
		}

		return *this;
	}

	void cleanup()
	{
		if(m_valid) {
			m_valid = false;
			m_f();
		}
	}

private:
	F m_f;
	bool m_valid = false;
};

#  endif // STORED_cplusplus >= 201103L

/*!
 * \brief libstored-allocator-aware \c std::deque.
 */
template <typename T>
struct Deque {
	typedef typename std::deque<T, typename Config::Allocator<T>::type> type;
};

/*!
 * \brief libstored-allocator-aware \c std::list.
 */
template <typename T>
struct List {
	typedef typename std::list<T, typename Config::Allocator<T>::type> type;
};

/*!
 * \brief libstored-allocator-aware \c std::map.
 */
template <typename Key, typename T, typename Compare = std::less<Key> /**/>
struct Map {
	typedef typename std::map<
		Key, T, Compare, typename Config::Allocator<std::pair<Key const, T> /**/>::type>
		type;
};

#  if STORED_cplusplus >= 201103L
/*!
 * \brief libstored-allocator-aware \c std::unordered_map.
 */
template <
	typename Key, typename T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
struct UnorderedMap {
	using type = typename std::unordered_map<
		Key, T, Hash, KeyEqual, typename Config::Allocator<std::pair<Key const, T>>::type>;
};

/*!
 * \brief libstored-allocator-aware \c std::unordered_multimap.
 */
template <
	typename Key, typename T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
struct UnorderedMultiMap {
	using type = typename std::unordered_multimap<
		Key, T, Hash, KeyEqual, typename Config::Allocator<std::pair<Key const, T>>::type>;
};
#  endif

/*!
 * \brief libstored-allocator-aware \c std::set.
 */
template <typename Key, typename Compare = std::less<Key> /**/>
struct Set {
	typedef typename std::set<Key, Compare, typename Config::Allocator<Key>::type> type;
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

} // namespace stored
#endif // __cplusplus

// STORED_CLASS_NEW_DELETE uses stored_assert, so we need util.h.
#include <libstored/util.h>

#endif // LIBSTORED_ALLOCATOR_H
