#ifndef LIBSTORED_TYPES_H
#define LIBSTORED_TYPES_H
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

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/config.h>
#include <libstored/util.h>

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

#if STORED_cplusplus >= 201103L
#  include <cinttypes>
#else
#  include <inttypes.h>
#endif

namespace stored {

	/*!
	 * \brief %Type traits of objects in a store.
	 *
	 * The type is a set of flags masked into a byte.
	 * It always fits in a signed char.
	 */
	struct Type {
		enum type {
			MaskSize = 0x07u,
			MaskFlags = 0x78u,
			FlagSigned = 0x08u,
			FlagInt = 0x10u,
			FlagFixed = 0x20u,
			FlagFunction = 0x40u,

			// int
			Int8 = FlagFixed | FlagInt | FlagSigned | 0u,
			Uint8 = FlagFixed | FlagInt | 0u,
			Int16 = FlagFixed | FlagInt | FlagSigned | 1u,
			Uint16 = FlagFixed | FlagInt | 1u,
			Int32 = FlagFixed | FlagInt | FlagSigned | 3u,
			Uint32 = FlagFixed | FlagInt | 3u,
			Int64 = FlagFixed | FlagInt | FlagSigned | 7u,
			Uint64 = FlagFixed | FlagInt | 7u,
			Int = FlagFixed | FlagInt | (sizeof(int) - 1),
			Uint = FlagFixed | (sizeof(int) - 1),

			// things with fixed length
			Float = FlagFixed | FlagSigned | 3u,
			Double = FlagFixed | FlagSigned | 7u,
			Bool = FlagFixed | 0u,
			Pointer32 = FlagFixed | 3u,
			Pointer64 = FlagFixed | 7u,
			Pointer = (sizeof(void*) <= 4 ? Pointer32 : Pointer64),

			// (special) things with undefined length
			Void = 0u,
			Blob = 1u,
			String = 2u,

			Invalid = 0xffu
		};

		/*! \brief Checks if the given type is a function. */
		constexpr static bool isFunction(type t) noexcept { return t & FlagFunction; }
		/*! \brief Checks if the given type has a fixed length, or is a function with such an argument. */
		constexpr static bool isFixed(type t) noexcept { return t & FlagFixed; }
		/*! \brief Checks if the given type is an integer, or is a function with such an argument. */
		constexpr static bool isInt(type t) noexcept { return isFixed(t) && (t & FlagInt); }
		/*! \brief Checks if the given type is signed number, or is a function with such an argument. */
		constexpr static bool isSigned(type t) noexcept { return isFixed(t) && (t & FlagSigned); }
		/*! \brief Checks if the given type is special (non-fixed size) type, or is a function with such an argument. */
		constexpr static bool isSpecial(type t) noexcept { return (t & MaskFlags) == 0; }
		/*! \brief Returns the size of the (function argument) type, or 0 when it is not fixed. */
		constexpr static size_t size(type t) noexcept { return !isFixed(t) ? 0u : (size_t)(t & MaskSize) + 1u; }
		/*! \brief Checks if endianness of given type is swapped in the store's buffer. */
		constexpr static bool isStoreSwapped(type t) noexcept {
			return
#ifdef STORED_LITTLE_ENDIAN
				!
#endif
				Config::StoreInLittleEndian
				&& Type::isFixed(t);
		}
	};

	constexpr static inline Type::type operator|(Type::type a, Type::type b) noexcept { return (Type::type)((uint8_t)a | (uint8_t)b); }

	namespace impl {
		/*! \brief Returns the #stored::Type::type of the given \c int type. */
		template <bool signd, size_t size> struct toIntType { static stored::Type::type const type = Type::Void; };
		template <> struct toIntType<true,1> { static stored::Type::type const type = Type::Int8; };
		template <> struct toIntType<false,1> { static stored::Type::type const type = Type::Uint8; };
		template <> struct toIntType<true,2> { static stored::Type::type const type = Type::Int16; };
		template <> struct toIntType<false,2> { static stored::Type::type const type = Type::Uint16; };
		template <> struct toIntType<true,4> { static stored::Type::type const type = Type::Int32; };
		template <> struct toIntType<false,4> { static stored::Type::type const type = Type::Uint32; };
		template <> struct toIntType<true,8> { static stored::Type::type const type = Type::Int64; };
		template <> struct toIntType<false,8> { static stored::Type::type const type = Type::Uint64; };
	}

	/*!
	 * \brief Returns the #stored::Type::type that corresponds to the given type \p T.
	 */
	template <typename T> struct toType { static Type::type const type = Type::Blob; };
	template <> struct toType<void> { static Type::type const type = Type::Void; };
	template <> struct toType<bool> { static Type::type const type = Type::Bool; };
	template <> struct toType<char> : public impl::toIntType<false,sizeof(char)> {};
	template <> struct toType<signed char> : public impl::toIntType<true,sizeof(char)> {};
	template <> struct toType<unsigned char> : public impl::toIntType<false,sizeof(char)> {};
	template <> struct toType<short> : public impl::toIntType<true,sizeof(short)> {};
	template <> struct toType<unsigned short> : public impl::toIntType<false,sizeof(short)> {};
	template <> struct toType<int> : public impl::toIntType<true,sizeof(int)> {};
	template <> struct toType<unsigned int> : public impl::toIntType<false,sizeof(int)> {};
	template <> struct toType<long> : public impl::toIntType<true,sizeof(long)> {};
	template <> struct toType<unsigned long> : public impl::toIntType<false,sizeof(long)> {};
#ifdef ULLONG_MAX
	// long long only exist since C99 and C++11, but many compilers do support them anyway.
	template <> struct toType<long long> : public impl::toIntType<true,sizeof(long long)> {};
	template <> struct toType<unsigned long long> : public impl::toIntType<false,sizeof(long long)> {};
#endif
	template <> struct toType<float> { static Type::type const type = Type::Float; };
	template <> struct toType<double> { static Type::type const type = Type::Double; };
	template <> struct toType<char*> { static Type::type const type = Type::String; };
	template <typename T> struct toType<T*> { static Type::type const type = Type::Pointer; };

	template <typename Container = void> class Variant;

	/*!
	 * \brief A typed variable in a store.
	 *
	 * This class only works for fixed-length variables (see stored::Type::isFixed()).
	 * Otherwise, use #stored::Variant.
	 *
	 * A Variable is very small (it contains only a pointer).
	 * It is copyable and assignable, so it is fine to pass it by value.
	 */
	template <typename T, typename Container, bool Hooks = Config::EnableHooks>
	class Variable {
	public:
		/*! \brief The (fixed-length) type of this variable. */
		typedef T type;

		/*!
		 * \brief Constructor for a valid Variable.
		 * \param container the Container this Variable belongs to
		 * \param buffer the reference to this Variable's buffer inside container's buffer
		 */
#ifndef _DEBUG
		constexpr
#endif
		// cppcheck-suppress uninitMemberVar
		Variable(Container& UNUSED_PAR(container), type& buffer) noexcept
			: m_buffer(&buffer)
		{
			stored_assert(((uintptr_t)&buffer & (sizeof(type) - 1)) == 0);
		}

		/*!
		 * \brief Constructor for an invalid Variable.
		 */
		// cppcheck-suppress uninitMemberVar
		constexpr Variable() noexcept : m_buffer() {}

		/*!
		 * \brief Copy construct, such that this Variable points to the same buffer the given Variable does.
		 */
		Variable(Variable const& v) noexcept { (*this) = v; }

		/*!
		 * \brief Let this Variable point to the same buffer as the given Variable.
		 */
		// cppcheck-suppress operatorEqVarError
		Variable& operator=(Variable const& v) noexcept { // NOLINT(bugprone-unhandled-self-assignment)
			m_buffer = v.m_buffer;
			return *this;
		}

#if STORED_cplusplus >= 201103L
		/*!
		 * \brief Move-construct.
		 */
		Variable(Variable&& v) noexcept { (*this) = std::move(v); }
		/*!
		 * \brief Move-assign.
		 */
		Variable& operator=(Variable&& v) noexcept { this->operator=((Variable const&)v); }
		/*!
		 * \brief Dtor.
		 */
		~Variable() noexcept = default;
#else
		/*!
		 * \brief Dtor.
		 */
		~Variable() noexcept {}
#endif

		/*!
		 * \brief Returns the value.
		 * \details Only call this function when it is #valid().
		 */
		type get() const noexcept {
			stored_assert(valid());
			return endian_s2h(buffer());
		}

		/*!
		 * \brief Returns the value, like #get(), cast to the given type.
		 * \details Only call this function when it is #valid().
		 */
		template <typename U>
		U as() const noexcept { return saturated_cast<U>(get()); }

		/*!
		 * \brief Returns the value, which is identical to #get().
		 */
#if STORED_cplusplus >= 201103L
		explicit
#endif
		operator type() const noexcept { return get(); } // NOLINT(hicpp-explicit-conversions)

		/*!
		 * \brief Sets the value.
		 * \details Only call this function when it is #valid().
		 */
		void set(type v) noexcept {
			stored_assert(valid());
			buffer() = endian_h2s(v);
		}

		/*!
		 * \brief Sets the value, which is identical to #set().
		 */
		Variable& operator=(type v) noexcept { set(v); return *this; }

		/*!
		 * \brief Checks if this Variable points to a valid buffer.
		 */
		constexpr bool valid() const noexcept { return m_buffer != nullptr; }

		/*!
		 * \brief Returns the container this Variable belongs to.
		 */
		Container& container() const;// { std::terminate(); }

		/*!
		 * \brief Checks if two Variables point to the same buffer, or are both invalid.
		 */
		constexpr bool operator==(Variable const& rhs) const noexcept { return m_buffer == rhs.m_buffer; }

		/*!
		 * \brief Checks if two Variables do not point to the same buffer.
		 */
		constexpr bool operator!=(Variable const& rhs) const noexcept { return !(*this == rhs); }

		/*!
		 * \brief Returns the size of the data.
		 */
		constexpr static size_t size() noexcept { return sizeof(type); }

	protected:
		/*!
		 * \brief Returns the buffer this Variable points to.
		 * \details Only call this function when it is #valid().
		 */
		type& buffer() const {
			stored_assert(valid());
			return *m_buffer;
		}

		// Make Variant a friend, such that a Variable can be converted to a Variant.
		friend class Variant<Container>;

	private:
		/*! \brief The buffer of this Variable. */
		type* m_buffer;
	};

	/*!
	 * \brief A typed variable in a store, with hook support.
	 *
	 * This class only works for fixed-length variables (see stored::Type::isFixed()).
	 * Otherwise, use #stored::Variant.
	 *
	 * This Variable is very small (it contains two pointers).
	 * It is copyable and assignable, so it is fine to pass it by value.
	 */
	template <typename T, typename Container>
	class Variable<T,Container,true> : public Variable<T,Container,false> {
	public:
		typedef Variable<T,Container,false> base;
		/*! \copydoc stored::Variable::type */
		typedef typename base::type type;

		/*! \copydoc stored::Variable::Variable(Container&, type&) */
		constexpr Variable(Container& container, type& buffer) noexcept
			: base(container, buffer)
			, m_container(&container)
#ifdef _DEBUG
			, m_entry()
#endif
		{}

		/*! \copydoc stored::Variable::Variable() */
		constexpr Variable() noexcept
			: m_container()
#ifdef _DEBUG
			, m_entry()
#endif
		{}

		/*! \copydoc stored::Variable::Variable(Variable const&) */
		Variable(Variable const& v) noexcept
			: base()
			, m_container()
#ifdef _DEBUG
			, m_entry()
#endif
		{ (*this) = v; }

		/*! \copydoc stored::Variable::operator=(Variable const&) */
		// cppcheck-suppress operatorEqVarError
		Variable& operator=(Variable const& v) noexcept { // NOLINT(bugprone-unhandled-self-assignment)
#ifdef _DEBUG
			stored_assert(m_entry == EntryNone);
#endif
			base::operator=(v);
			m_container = v.m_container;
			return *this;
		}

#if STORED_cplusplus >= 201103L
		/*! \copydoc stored::Variable::Variable(Variable&&) */
		Variable(Variable&& v) noexcept
#  ifdef _DEBUG
			: m_entry()
#  endif
		{ (*this) = std::move(v); }
		/*! \copydoc stored::Variable::operator=(Variable&&) */
		Variable& operator=(Variable&& v) noexcept { this->operator=((Variable const&)v); return *this; }
		/*! \copydoc stored::Variable::~Variable() */
		~Variable() noexcept
#  ifdef _DEBUG
		{ stored_assert(m_entry == EntryNone); }
#  else
			= default;
#  endif
#else
		/*! \copydoc stored::Variable::~Variable() */
		~Variable() noexcept { stored_assert(m_entry == EntryNone); }
#endif
		/*!
		 * \copydoc stored::Variable::get()
		 * \details #entryRO()/#exitRO() are called around the actual data retrieval.
		 */
		type get() const noexcept {
			entryRO();
			type res = base::get();
			exitRO();
			return res;
		}

		/*! \copydoc stored::Variable::as() */
		template <typename U>
		U as() const noexcept { return saturated_cast<U>(get()); }

		/*!
		 * \brief Returns the value, which is identical to #get().
		 */
#if STORED_cplusplus >= 201103L
		explicit
#endif
		operator type() const noexcept { return get(); } // NOLINT(hicpp-explicit-conversions)

		/*!
		 * \copydoc stored::Variable::set()
		 * \details #entryX()/#exitX() are called around the actual data retrieval.
		 */
		void set(type v) noexcept {
			entryX();

			bool changed = false;
			if(Type::isStoreSwapped(toType<type>::type))
				changed = memcmp_swap(&v, &this->buffer(), sizeof(v)) != 0;
			else
				changed = memcmp(&v, &this->buffer(), sizeof(v)) != 0;

			if(changed)
				base::set(v);
			exitX(changed);
		}

		/*! \copydoc stored::Variable::operator=(type) */
		Variable& operator=(type v) noexcept {
			set(v);
			return *this;
		}

		/*! \copydoc stored::Variable::size() */
		constexpr static size_t size() noexcept { return sizeof(type); }

		/*! \copydoc stored::Variable::container() */
		Container& container() const noexcept {
			stored_assert(this->valid());
			return *m_container; // NOLINT(clang-analyzer-core.uninitialized.UndefReturn)
		}

		/*!
		 * \brief Returns the key that belongs to this Variable.
		 * \see your store's bufferToKey()
		 */
		typename Container::Key key() const noexcept { return container().bufferToKey(&this->buffer()); }

		/*!
		 * \brief Calls the \c entryX() hook of the container.
		 * \see your store's \c hookEntryX()
		 */
		void entryX() const noexcept {
#ifdef _DEBUG
			stored_assert(m_entry == EntryNone);
			m_entry = EntryX;
#endif
			container().hookEntryX(toType<T>::type, &this->buffer(), sizeof(type));
		}

		/*!
		 * \brief Calls the \c exitX() hook of the container.
		 * \see your store's \c hookExitX()
		 */
		void exitX(bool changed) const noexcept {
			container().hookExitX(toType<T>::type, &this->buffer(), sizeof(type), changed);
#ifdef _DEBUG
			stored_assert(m_entry == EntryX);
			m_entry = EntryNone;
#endif
		}

		/*!
		 * \brief Calls the \c entryRO() hook of the container.
		 * \see your store's \c hookEntryRO()
		 */
		void entryRO() const noexcept {
#ifdef _DEBUG
			stored_assert(m_entry == EntryNone);
			m_entry = EntryRO;
#endif
			container().hookEntryRO(toType<T>::type, &this->buffer(), sizeof(type));
		}

		/*!
		 * \brief Calls the \c exitRO() hook of the container.
		 * \see your store's \c hookExitRO()
		 */
		void exitRO() const noexcept {
			container().hookExitRO(toType<T>::type, &this->buffer(), sizeof(type));
#ifdef _DEBUG
			stored_assert(m_entry == EntryRO);
			m_entry = EntryNone;
#endif
		}

	private:
		/*! \brief The container of this Variable. */
		Container* m_container;
#ifdef _DEBUG
		enum { EntryNone, EntryRO, EntryX };
		/*! \brief Tracking entry/exit calls. */
		mutable uint_fast8_t m_entry;
#endif
	};

	/*!
	 * \brief A typed function in a store.
	 *
	 * This class only works for functions with fixed-length arguments (see stored::Type::isFixed()).
	 * Otherwise, use #stored::Variant.
	 *
	 * A Function is very small (it contains two words).
	 * It is default copyable and assignable, so it is fine to pass it by value.
	 */
	template <typename T, typename Container>
	class Function {
	public:
		/*! \brief The type of the function's argument. */
		typedef T type;

		/*!
		 * \brief Constructor for a valid Function.
		 */
		constexpr Function(Container& container, unsigned int f) noexcept
			: m_container(&container), m_f(f)
		{}

		/*!
		 * \brief Constructor for an invalid Function.
		 */
		constexpr Function() noexcept
			: m_container(), m_f()
		{}

		/*!
		 * \brief Calls the function and return its value.
		 * \details Only call this function when it is #valid().
		 */
		type get() const {
			stored_assert(valid());
			type value;
			callback(false, value);
			return value;
		}

		/*! \copydoc Variable::as() */
		template <typename U>
		U as() const { return saturated_cast<U>(get()); }

		/*!
		 * \brief Returns the value, which is identical to #get().
		 */
#if STORED_cplusplus >= 201103L
		explicit
#endif
		operator type() const { return get(); } // NOLINT(hicpp-explicit-conversions)

		/*!
		 * \brief Calls the function and write its value in the given buffer.
		 * \details Only call this function when it is #valid().
		 * \param dst the buffer to write to
		 * \param len the length of \p dst, normally equal to #size()
		 * \return the number of bytes written to \p dst
		 */
		size_t get(void* dst, size_t len) const {
			stored_assert(valid());
			return callback(false, dst, len);
		}

		/*!
		 * \brief Call the function to write the value.
		 * \details Only call this function when it is #valid().
		 */
		void set(type value) const {
			stored_assert(valid());
			callback(true, value);
		}

		/*!
		 * \brief Call the function to write the value.
		 * \details Only call this function when it is #valid().
		 * \param src the buffer to read from
		 * \param len the length of \p src, normally equal to #size()
		 * \return the number of bytes read from \p src
		 */
		size_t set(void* src, size_t len) {
			stored_assert(valid());
			return callback(true, src, len);
		}

		/*!
		 * \brief Call the function, like #get().
		 */
		type operator()() const { return get(); }
		/*!
		 * \brief Call the function, like #set().
		 */
		void operator()(type value) { set(value); }
		/*! \copydoc Variable::operator=(type) */
		Function& operator=(type v) { set(v); return *this; }

		/*!
		 * \brief Checks if this Function is valid.
		 */
		constexpr bool valid() const noexcept { return m_f > 0; }

		/*!
		 * \brief Returns the container this Function belongs to.
		 * \details Only call this function when it is #valid().
		 */
		Container& container() const noexcept {
			stored_assert(valid());
			return *m_container;
		}

		/*!
		 * \brief Invoke the callback at the #container().
		 */
		size_t callback(bool set, type& value) const {
			stored_assert(valid());
			return container().callback(set, &value, sizeof(type), id());
		}

		/*!
		 * \brief Invoke the callback at the #container().
		 */
		size_t callback(bool set, void* buffer, size_t len) const {
			stored_assert(valid());
			return container().callback(set, buffer, len, id());
		}

		/*!
		 * \brief Returns the function ID.
		 * \details Only call this function when it is #valid().
		 */
		unsigned int id() const noexcept {
			stored_assert(valid());
			return m_f;
		}

		/*!
		 * \brief Checks if this Function points to the same Function as the given one.
		 */
		bool operator==(Function const& rhs) const noexcept {
			if(valid() != rhs.valid())
				return false;
			if(!valid())
				return true;
			return m_container == rhs.m_container && m_f == rhs.m_f;
		}

		/*!
		 * \brief Checks if this Function points to the same Function as the given one.
		 */
		bool operator!=(Function const& rhs) const noexcept { return !(*this == rhs); }

		/*!
		 * \brief Returns the size of the function's argument.
		 */
		constexpr static size_t size() noexcept { return sizeof(type); }

	private:
		/*! \brief The container this Function belongs to. */
		Container* m_container;
		/*! \brief The function ID. */
		unsigned int m_f;
	};

	/*!
	 * \brief A untyped interface to an object in a store.
	 *
	 * This class works for all variables and functions of all types.
	 * However, using #stored::Variable or #stored::Function is more efficient both in performance and memory.
	 * Use those when you can.
	 *
	 * A Variant is quite small (only about four words).
	 * It is default copyable and assignable, so it is fine to pass it by value.
	 */
	template <typename Container>
	class Variant {
	public:
		/*!
		 * \brief Constructor for a variable.
		 */
#ifndef _DEBUG
		constexpr
#endif
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		Variant(Container& container, Type::type type, void* buffer, size_t len) noexcept
			: m_container(&container), m_buffer(buffer), m_len(len), m_type((uint8_t)type)
#ifdef _DEBUG
			, m_entry()
#endif
		{
			stored_assert(!Type::isFunction(type));
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			stored_assert(!Type::isFixed(this->type()) ||
				(reinterpret_cast<uintptr_t>(buffer) & (Type::size(this->type()) - 1)) == 0);
		}

		/*!
		 * \brief Constructor for a function.
		 */
#ifndef _DEBUG
		constexpr
#endif
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		Variant(Container& container, Type::type type, unsigned int f, size_t len) noexcept
			: m_container(&container), m_f((uintptr_t)f), m_len(len), m_type((uint8_t)type)
#ifdef _DEBUG
			, m_entry()
#endif
		{
			stored_assert(Type::isFunction(type));
			static_assert(sizeof(uintptr_t) >= sizeof(unsigned int), "");
		}

		/*!
		 * \brief Constructor for an invalid Variant.
		 */
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		constexpr Variant() noexcept
			: m_container(), m_buffer(), m_len(), m_type()
#ifdef _DEBUG
			, m_entry()
#endif
		{
		}

		/*!
		 * \brief Constructor for a Variable.
		 */
		template <typename T>
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		constexpr explicit Variant(Variable<T,Container> const& v) noexcept
			: m_container(v.valid() ? &v.container() : nullptr)
			, m_buffer(v.valid() ? &v.buffer() : nullptr)
			, m_len(sizeof(T))
			, m_type(toType<T>::type)
#ifdef _DEBUG
			, m_entry()
#endif
		{}

		/*!
		 * \brief Constructor for a Function.
		 */
		template <typename T>
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		constexpr explicit Variant(Function<T,Container> const& f) noexcept
			: m_container(f.valid() ? &f.container() : nullptr)
			, m_f(f.valid() ? f.id() : 0)
			, m_len(sizeof(T))
			, m_type(f.type())
#ifdef _DEBUG
			, m_entry()
#endif
		{}

		/*!
		 * \brief Get the value.
		 * \details For variables, #entryRO()/#exitRO() is called.
		 * \details In case #type() is Type::String, only up to the first null byte are copied.
		 *          If \p dst is sufficiently large (\p len &gt; #size()), a null terminator is always written after the string.
		 * \details Only call this function when #valid().
		 * \param dst the buffer to copy to
		 * \param len the length of \p dst, when this is a fixed type, 0 implies the normal size
		 * \return the number of bytes written into \p dst
		 */
		size_t get(void* dst, size_t len = 0) const {
			if(Type::isFixed(type())) {
				stored_assert(len == size() || len == 0);
				len = size();
			} else {
				len = std::min(len, size());
			}

			if(Type::isFunction(type())) {
				len = container().callback(false, dst, len, (unsigned int)m_f);
			} else {
				entryRO(len);
				if(unlikely(type() == Type::String)) {
					char* dst_ = static_cast<char*>(dst);
					char* buffer_ = static_cast<char*>(m_buffer);
					size_t len_ = strncpy(dst_, buffer_, len);
					if(len > len_)
						dst_[len_] = '\0';
					len = len_;
				} else {
					if(Type::isStoreSwapped(type()))
						memcpy_swap(dst, m_buffer, len);
					else
						memcpy(dst, m_buffer, len);
				}
				exitRO(len);
			}
			return len;
		}

		/*!
		 * \brief Wrapper for #get(void*,size_t) const that converts the type.
		 * \details This only works for fixed types. Make sure that #type() matches \p T.
		 */
		template <typename T> T get() const {
			stored_assert(Type::isFixed(type()));
			stored_assert(toType<T>::type == type());
			stored_assert(sizeof(T) == size());
			T data;
			size_t len = get(&data, sizeof(T));
			// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.UndefReturn)
			return len == sizeof(T) ? data : T();
		}

		/*!
		 * \brief Gets the value.
		 * \see #get(void*, size_t) const
		 */
		Vector<char>::type get() const {
			Vector<char>::type buf(size());
			get(&buf[0], buf.size());
			return buf;
		}

		/*!
		 * \brief Set the value.
		 * \details For variables, #entryX()/#exitX() is called.
		 * \details In case #type() is Type::String, only up to the first null byte are copied.
		 *          If there is no null byte in \p src, it is implicitly appended at the end.
		 * \details Only call this function when #valid().
		 * \param src the buffer to copy from
		 * \param len the length of \p src, when this is a fixed type, 0 implies the normal size
		 * \return the number of bytes read from \p src
		 */
		size_t set(void const* src, size_t len = 0) {
			if(Type::isFixed(type())) {
				stored_assert(len == size() || len == 0);
				len = size();
			} else {
				len = std::min(len, size());
			}

			if(isFunction()) {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				len = container().callback(true, const_cast<void*>(src), len, (unsigned int)m_f);
			} else {
				bool changed = true;

				entryX(len);

				if(unlikely(type() == Type::String)) {
					// src may not be null-terminated.
					char const* src_ = static_cast<char const*>(src);
					// The byte after the buffer of the string is reserved for the \0.
					char* buffer_ = static_cast<char*>(m_buffer);

					if(Config::EnableHooks)
						changed = strncmp(src_, len, buffer_, len + 1) != 0;

					if(changed)
						buffer_[len = strncpy(buffer_, src_, len)] = '\0';
				} else {
					if(Config::EnableHooks) {
						if(Type::isStoreSwapped(type()))
							changed = memcmp_swap(src, m_buffer, len) != 0;
						else
							changed = memcmp(src, m_buffer, len) != 0;
					}

					if(changed) {
						if(Type::isStoreSwapped(type()))
							memcpy_swap(m_buffer, src, len);
						else
							memcpy(m_buffer, src, len);
					}
				}

				exitX(changed, len);
			}
			return len;
		}

		/*!
		 * \brief Wrapper for #set(void const*,size_t) that converts the type.
		 * \details This only works for fixed types. Make sure that #type() matches \p T.
		 */
		template <typename T> void set(T value) {
			stored_assert(Type::isFixed(type()));
			stored_assert(toType<T>::type == type());
			stored_assert(sizeof(T) == size());
			set(&value, sizeof(T));
		}

		/*!
		 * \brief Invokes \c hookEntryX() on the #container().
		 */
		void entryX() const noexcept { entryX(size()); }
		/*! \copydoc entryX() */
		void entryX(size_t len) const noexcept {
			if(Config::EnableHooks) {
#ifdef _DEBUG
				stored_assert(m_entry == EntryNone);
				m_entry = EntryX;
#endif
				container().hookEntryX(type(), m_buffer, len);
			}
		}

		/*!
		 * \brief Invokes \c hookExitX() on the #container().
		 */
		void exitX(bool changed) const noexcept { exitX(changed, size()); }
		/*! \copydoc exitX() */
		void exitX(bool changed, size_t len) const noexcept {
			if(Config::EnableHooks) {
				container().hookExitX(type(), m_buffer, len, changed);
#ifdef _DEBUG
				stored_assert(m_entry == EntryX);
				m_entry = EntryNone;
#endif
			}
		}

		/*!
		 * \brief Invokes \c hookEntryRO() on the #container().
		 */
		void entryRO() const noexcept { entryRO(size()); }
		/*! \copydoc entryRO() */
		void entryRO(size_t len) const noexcept {
			if(Config::EnableHooks) {
#ifdef _DEBUG
				stored_assert(m_entry == EntryNone);
				m_entry = EntryRO;
#endif
				container().hookExitRO(type(), m_buffer, len);
			}
		}

		/*!
		 * \brief Invokes \c hookExitRO() on the #container().
		 */
		void exitRO() const noexcept { exitRO(size()); }
		/*! \copydoc exitRO() */
		void exitRO(size_t len) const noexcept {
			if(Config::EnableHooks) {
				container().hookExitRO(type(), m_buffer, len);
#ifdef _DEBUG
				stored_assert(m_entry == EntryRO);
				m_entry = EntryNone;
#endif
			}
		}

		/*!
		 * \brief Returns the type.
		 * \details Only call this function when it is #valid().
		 */
		Type::type type() const noexcept { stored_assert(valid()); return (Type::type)m_type; }
		/*!
		 * \brief Returns the size.
		 * \details In case #type() is Type::String, this returns the maximum size of the string, excluding null terminator.
		 * \details Only call this function when it is #valid().
		 */
		size_t size() const noexcept { stored_assert(valid()); return Type::isFixed(type()) ? Type::size(type()) : m_len; }
		/*!
		 * \brief Returns the buffer.
		 * \details Only call this function when it is #valid().
		 */
		void* buffer() const noexcept { stored_assert(isVariable()); return m_buffer; }
		/*!
		 * \brief Checks if this Variant is valid.
		 */
		constexpr bool valid() const noexcept { return m_buffer != nullptr; }
		/*!
		 * \brief Checks if the #type() is a function.
		 * \details Only call this function when it is #valid().
		 */
		bool isFunction() const noexcept { stored_assert(valid()); return Type::isFunction(type()); }
		/*!
		 * \brief Checks if the #type() is a variable.
		 * \details Only call this function when it is #valid().
		 */
		bool isVariable() const noexcept { stored_assert(valid()); return !isFunction(); }
		/*!
		 * \brief Returns the container.
		 * \details Only call this function when it is #valid().
		 */
		Container& container() const noexcept { stored_assert(valid()); return *m_container; }

		/*!
		 * \brief Returns a #stored::Variable that corresponds to this Variant.
		 * \details Only call this function when it #isVariable() and the #type() matches \p T.
		 */
		template <typename T> Variable<T,Container> variable() const noexcept {
			if(unlikely(!valid()))
				return Variable<T,Container>();

			stored_assert(isVariable());
			stored_assert(Type::isFixed(type()));
			stored_assert(toType<T>::type == type());
			stored_assert(sizeof(T) == size());
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			return Variable<T,Container>(container(), *reinterpret_cast<T*>(m_buffer));
		}

		/*!
		 * \brief Returns a #stored::Function that corresponds to this Variant.
		 * \details Only call this function when it #isFunction() and the #type() matches \p T.
		 */
		template <typename T> Function<T,Container> function() const noexcept {
			if(unlikely(!valid()))
				return Function<T,Container>();

			stored_assert(isFunction());
			stored_assert(Type::isFixed(type()));
			stored_assert(toType<T>::type == (type() & ~Type::FlagFunction));
			stored_assert(sizeof(T) == size());
			return Function<T,Container>(container(), (unsigned int)m_f);
		}

		/*!
		 * \brief Returns the key of this variable.
		 * \details Only call this function when it #isVariable().
		 * \see your store's \c bufferToKey()
		 */
		typename Container::Key key() const noexcept {
			stored_assert(isVariable());
			return container()->bufferToKey(m_buffer);
		}

		/*!
		 * \brief Checks if this Variant points to the same object as the given one.
		 */
		bool operator==(Variant const& rhs) const noexcept {
			if(valid() != rhs.valid())
				return false;
			if(!valid())
				return true;
			return m_type == rhs.m_type && m_container == rhs.m_container &&
				(isFunction() ? m_f == rhs.m_f : m_buffer == rhs.m_buffer && m_len == rhs.m_len);
		}

		/*!
		 * \brief Checks if this Variant points to the same object as the given one.
		 */
		bool operator!=(Variant const& rhs) const noexcept { return !(*this == rhs); }

	private:
		/*! \brief The container. */
		Container* m_container;
		union {
			/*! \brief The buffer for a variable. */
			void* m_buffer;
			/*! \brief The function ID for a function. */
			uintptr_t m_f;
		};
		/*! \brief Size of the data. */
		size_t m_len;
		/*! \brief Type of the object. */
		uint8_t m_type;
#ifdef _DEBUG
		enum { EntryNone, EntryRO, EntryX };
		/*! \brief Tracking entry/exit calls. */
		mutable uint8_t m_entry;
#endif
	};

	/*!
	 * \brief A store-independent untyped wrapper for an object.
	 *
	 * It is not usable, until it is applied to a store.
	 * All member functions, except for #apply() are there to match the Variant's interface,
	 * but are non-functional, as there is no container.
	 *
	 * \see #apply()
	 */
	template <>
	// cppcheck-suppress noConstructor
	class Variant<void> {
	public:
		/*!
		 * \brief Constructor for a variable.
		 */
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		constexpr Variant(Type::type type, void* buffer, size_t len) noexcept
			: m_dummy(), m_buffer(buffer), m_len(len), m_type((uint8_t)type)
#ifdef _DEBUG
			, m_entry()
#endif
		{
		}

		/*!
		 * \brief Constructor for a function.
		 */
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		constexpr Variant(Type::type type, unsigned int f, size_t len) noexcept
			: m_dummy(), m_f((uintptr_t)f), m_len(len), m_type((uint8_t)type)
#ifdef _DEBUG
			, m_entry()
#endif
		{
			static_assert(sizeof(uintptr_t) >= sizeof(unsigned int), "");
		}

		/*!
		 * \brief Constructor for an invalid Variant.
		 */
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		constexpr Variant() noexcept
			: m_dummy(), m_buffer(), m_len(), m_type((uint8_t)Type::Invalid)
#ifdef _DEBUG
			, m_entry()
#endif
		{}

		/*!
		 * \brief Apply the stored object properties to a container.
		 */
		template <typename Container>
		Variant<Container> apply(Container& container) const noexcept {
			static_assert(sizeof(Variant<Container>) == sizeof(Variant<>), "");

			if(!valid())
				return Variant<Container>();
			else if(isFunction())
				return Variant<Container>(container, (Type::type)m_type, (unsigned int)m_f, m_len);
			else {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
				stored_assert((uintptr_t)m_buffer >= (uintptr_t)&container && (uintptr_t)m_buffer + m_len <= (uintptr_t)&container + sizeof(Container));
				return Variant<Container>(container, (Type::type)m_type, m_buffer, m_len);
			}
		}

		/*! \brief Don't use. */
		size_t get(void* UNUSED_PAR(dst), size_t UNUSED_PAR(len) = 0) const noexcept { stored_assert(valid()); return 0; }
		/*! \brief Don't use. */
		template <typename T> T get() const noexcept { stored_assert(valid()); return T(); }
		/*! \brief Don't use. */
		size_t set(void const* UNUSED_PAR(src), size_t UNUSED_PAR(len) = 0) noexcept { stored_assert(valid()); return 0; }
		/*! \brief Don't use. */
		template <typename T> void set(T UNUSED_PAR(value)) noexcept { stored_assert(valid()); }
		/*! \brief Don't use. */
		void entryX(size_t UNUSED_PAR(len) = 0) const noexcept {}
		/*! \brief Don't use. */
		void exitX(bool UNUSED_PAR(changed), size_t UNUSED_PAR(len) = 0) const noexcept {}
		/*! \brief Don't use. */
		void entryRO(size_t UNUSED_PAR(len) = 0) const noexcept {}
		/*! \brief Don't use. */
		void exitRO(size_t UNUSED_PAR(len) = 0) const noexcept {}
		/*! \copybrief Variant::type() */
		constexpr Type::type type() const noexcept { return (Type::type)m_type; }
		/*! \copybrief Variant::size() */
		size_t size() const noexcept { stored_assert(valid()); return Type::isFixed(type()) ? Type::size(type()) : m_len; }
		/*! \copybrief Variant::valid() */
		constexpr bool valid() const noexcept { return type() != Type::Invalid; }
		/*! \copybrief Variant::isFunction() */
		bool isFunction() const noexcept { stored_assert(valid()); return Type::isFunction(type()); }
		/*! \copybrief Variant::isVariable() */
		bool isVariable() const noexcept { stored_assert(valid()); return !isFunction(); }
		/*! \brief Don't use. */
		int& container() const noexcept { stored_assert(valid()); std::terminate(); }

		/*! \copybrief Variant::operator==() */
		bool operator==(Variant const& rhs) const noexcept {
			if(valid() != rhs.valid())
				return false;
			if(!valid())
				return true;
			return m_type == rhs.m_type &&
				(isFunction() ? m_f == rhs.m_f : m_buffer == rhs.m_buffer && m_len == rhs.m_len);
		}

		/*! \copybrief Variant::operator!=() */
		bool operator!=(Variant const& rhs) const noexcept { return !(*this == rhs); }

	private:
		// Make this class the same size as a non-void container.
#ifdef __clang__
		void* m_dummy __attribute__((unused));
#else
		void* m_dummy;
#endif

		union {
			/*! \copydoc Variant::m_buffer */
			void* m_buffer;
			/*! \copydoc Variant::m_f */
			uintptr_t m_f;
		};
		/*! \copydoc Variant::m_len */
		size_t m_len;
		/*! \copydoc Variant::m_type */
		uint8_t m_type;
#ifdef _DEBUG
		/*! \copydoc Variant::m_entry */
#  ifdef __clang__
		uint8_t m_entry __attribute__((unused));
#  else
		uint8_t m_entry;
#  endif
#endif
	};


	namespace impl {
		template <typename StoreBase, typename T>
		constexpr static inline void* objectToVoidPtr(T& o) noexcept {
			static_assert(sizeof(T) == sizeof(typename StoreBase::Objects), "");
			return (void*)&o;
		}

		template <typename StoreBase, typename T>
		constexpr static inline StoreBase& objectToStore(T& o) noexcept {
			static_assert(sizeof(T) == sizeof(typename StoreBase::Objects), "");
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			return *static_cast<StoreBase*>(reinterpret_cast<typename StoreBase::Objects*>(objectToVoidPtr<StoreBase,T>(o)));
		}

		/*!
		 * \brief Variable class as used in the *Objects base class of a store.
		 *
		 * Do not use. Only the *Objects class is allowed to use it.
		 */
		template <typename Store, typename Implementation, typename T, size_t offset, size_t size_>
		class StoreVariable {
		public:
			typedef T type;
			typedef Variable<type,Implementation> Variable_type;
			typedef Variant<Implementation> Variant_type;

			constexpr Variable_type variable() const noexcept {
				static_assert(size_ == sizeof(type), "");
				return objectToStore<Store>(*this).template _variable<type>(offset);
			}
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			constexpr operator Variable_type() const noexcept { return variable(); }

			constexpr Variant_type variant() const noexcept { return Variant_type(variable()); }
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			constexpr operator Variant_type() const noexcept { return variant(); }

			type get() const noexcept { return variable().get(); }
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			operator type() const noexcept { return get(); }

			template <typename U>
			U as() const noexcept { return saturated_cast<U>(get()); }

			void set(type value) noexcept { variable().set(value); }
			// NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
			Variable_type operator=(type value) noexcept {
				Variable_type v = variable();
				v.set(value);
				return v;
			}

			constexpr static size_t size() { return sizeof(type); }
		};

		/*!
		 * \brief Function class as used in the *Objects base class of a store.
		 *
		 * Do not use. Only the *Objects class is allowed to use it.
		 */
		template <
			typename Store, typename Implementation,
			template <typename, unsigned int> class FunctionMap,
			unsigned int F>
		class StoreFunction {
		public:
			typedef typename FunctionMap<Implementation,F>::type type;
			typedef Function<type,Implementation> Function_type;
			typedef Variant<Implementation> Variant_type;

			constexpr Function_type function() const noexcept {
				return objectToStore<Store>(*this).template _function<type>(F);
			}
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			constexpr operator Function_type() const noexcept { return function(); }

			constexpr Variant_type variant() const noexcept { return Variant_type(function()); }
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			constexpr operator Variant_type() const noexcept { return variant(); }

			type get() const {
				type v;
				call(false, v);
				return v;
			}

#if STORED_cplusplus >= 201103L
			explicit
#endif
			operator type() const { return get(); } // NOLINT(hicpp-explicit-conversions)

			template <typename U>
			U as() const { return saturated_cast<U>(get()); }

			size_t get(void* dst, size_t UNUSED_PAR(len)) const {
				stored_assert(len == sizeof(type));
				stored_assert(dst);
				call(false, *static_cast<type*>(dst));
				return sizeof(type);
			}
			type operator()() const { return get(); }

			void set(type value) {
				call(true, value);
			}

			size_t set(void* src, size_t UNUSED_PAR(len)) {
				stored_assert(len == sizeof(type));
				stored_assert(src);
				call(true, *static_cast<type*>(src));
				return sizeof(type);
			}

			void operator()(type value) { function()(value); }

			StoreFunction& operator=(type value) {
				set(value);
				return *this;
			}

			constexpr static size_t size() noexcept { return sizeof(type); }

		protected:
			constexpr Implementation& implementation() const noexcept {
				return static_cast<Implementation&>(objectToStore<Store>(*this));
			}

			void call(bool set, type& value) const {
				FunctionMap<Implementation,F>::call(implementation(), set, value);
			}
		};

		/*!
		 * \brief Variant class for a variable as used in the *Objects base class of a store.
		 *
		 * Do not use. Only the *Objects class is allowed to use it.
		 */
		template <typename Store, typename Implementation, Type::type type_, size_t offset, size_t size_>
		class StoreVariantV {
		public:
			typedef Variant<Implementation> Variant_type;

			constexpr Variant_type variant() const noexcept {
				return objectToStore<Store>(*this)._variantv(type_, offset, size_);
			}
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			constexpr operator Variant_type() const noexcept { return variant(); }

			size_t get(void* dst, size_t len = 0) const noexcept { return variant().get(dst, len); }
			template <typename T> T get() const noexcept { return variant().template get<T>(); }

			size_t set(void const* src, size_t len = 0) noexcept { return variant().set(src, len); }
			template <typename T> void set(T value) noexcept { variant().template set<T>(value); }

			constexpr static Type::type type() noexcept { return type_; }
			constexpr static size_t size() noexcept { return size_; }
			void* buffer() const noexcept { return variant().buffer(); }
		};

		/*!
		 * \brief Variant class for a variable as used in the *Objects base class of a store.
		 *
		 * Do not use. Only the *Objects class is allowed to use it.
		 */
		template <typename Store, typename Implementation, Type::type type_, unsigned int F, size_t size_>
		class StoreVariantF {
		public:
			typedef Variant<Implementation> Variant_type;

			constexpr Variant_type variant() const noexcept {
				return objectToStore<Store>(*this)._variantf(type_, F, size_);
			}
			// NOLINTNEXTLINE(hicpp-explicit-conversions)
			constexpr operator Variant_type() const noexcept { return variant(); }

			size_t get(void* dst, size_t len = 0) const { return variant().get(dst, len); }
			template <typename T> T get() const { return variant().template get<T>(); }

			size_t set(void const* src, size_t len = 0) { return variant().set(src, len); }
			template <typename T> void set(T value) { variant().template set<T>(value); }

			constexpr static Type::type type() noexcept { return type_; }
			constexpr static size_t size() noexcept { return size_; }
		};
	}

} // namespace
#endif // __cplusplus
#endif // LIBSTORED_TYPES_H
