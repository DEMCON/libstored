#include <libstored/util.h>

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
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#if STORED_cplusplus >= 201103L
#  include <utility>
#endif

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
	 * \brief libstored-allocator-aware \c std::function.
	 */
#if STORED_cplusplus < 201103L
	template <typename F>
	struct Callable {
		typedef F* type;
	};
#else

	namespace impl {
		template <typename R, typename... Args>
		class Callable {
		protected:
			class Base {
			public:
				virtual ~Base() = default;
				virtual R operator()(Args... args) const = 0;
				virtual operator bool() const { return true; }
			};

			class Reset : public Base {
			public:
				~Reset() final = default;

				R operator()(Args... args) const final {
					return R();
				}

				operator bool() const final { return false; }
			};

			template <typename F, bool Dummy = false>
			class Wrapper : public Base {
			public:
				~Wrapper() final = default;

				template <typename F_>
				Wrapper(F_&& f) : m_f{std::forward<F_>(f)} {}

				R operator()(Args... args) const final {
					return m_f(args...);
				}
			private:
				F m_f;
			};

			template <bool Dummy>
			class Wrapper<R(Args...), Dummy> : public Base {
			public:
				~Wrapper() final = default;

				template <typename F_>
				Wrapper(R(*f)(Args...)) : m_f{f} {}

				R operator()(Args... args) const final {
					return m_f(args...);
				}
			private:
				R(*m_f)(Args...);
			};

			template <typename F>
			class Forwarder : public Base {
			public:
				template <typename F_>
				__attribute__((nonnull)) explicit Forwarder(F_&& f)
					: m_w{new(allocate<Wrapper<F>>()) Wrapper<F>(std::forward<F_>(f))}
				{}

				~Forwarder() noexcept final {
					cleanup<Wrapper<F>>(m_w);
				}

				R operator()(Args... args) const final {
					return (*m_w)(args...);
				}
			private:
				Wrapper<F>* m_w;
			};

		public:
			explicit Callable(std::nullptr_t = nullptr) {
				construct<Reset>();
			}

			template <typename G>
			explicit Callable(G&& g) {
				assign(std::forward<G>(g));
			}

			~Callable() noexcept {
				get().~Base();
			}

			R operator()(Args... args) const {
				return (get())(args...);
			}

			operator bool() const {
				return get();
			}

			Callable& operator=(std::nullptr_t) {
				if(*this) {
					destroy();
					construct<Reset>();
				}

				return *this;
			}

			template <typename G>
			Callable& operator=(G&& g) {
				replace(std::forward<G>(g));
				return *this;
			}

		protected:
			Base const& get() const {
				return *reinterpret_cast<Base const*>(m_buffer);
			}

			Base& get() {
				return *reinterpret_cast<Base*>(m_buffer);
			}

			template <typename G>
			void assign(G&& g) {
				using F_ = typename std::decay<G>::type;
				if(sizeof(Wrapper<F_>) > sizeof(m_buffer))
					construct<Forwarder<F_>>(std::forward<G>(g));
				else
					construct<Wrapper<F_>>(std::forward<G>(g));
			}

			template <typename T, typename... TArgs>
			void construct(TArgs&&... args) {
				static_assert(sizeof(T) <= sizeof(m_buffer), "");
				new(m_buffer) T{std::forward<TArgs>(args)...};
			}

			void destroy() {
				get().~Base();
			}

			template <typename G>
			void replace(G&& g) {
				destroy();
				assign(std::forward<G>(g));
			}

		private:
			char m_buffer[std::max(sizeof(Reset), sizeof(Forwarder<R(*)(Args...)>) + sizeof(void*))];
		};
	}

	template <typename F>
	struct Callable {
		template <typename R, typename... Args>
		static impl::Callable<R,typename std::remove_reference<Args>::type...> callable_impl(R(Args...));
		using type = decltype(callable_impl(std::declval<F>()));
	};
#endif

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
	template <typename Key, typename T, typename Compare = std::less<Key> >
	struct Map {
		typedef typename std::map<Key, T, Compare, typename Config::Allocator<std::pair<Key const, T> >::type> type;
	};

	/*!
	 * \brief libstored-allocator-aware \c std::set.
	 */
	template <typename Key, typename Compare = std::less<Key> >
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

} // namespace
#endif // __cplusplus
#endif // LIBSTORED_ALLOCATOR_H
