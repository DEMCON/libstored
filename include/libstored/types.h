#ifndef LIBSTORED_TYPES_H
#define LIBSTORED_TYPES_H
// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#  include <libstored/macros.h>
#  include <libstored/config.h>
#  include <libstored/util.h>

#  include <algorithm>
#  include <cstdlib>
#  include <cstring>
#  include <exception>
#  include <limits>

#  if STORED_cplusplus >= 201103L
#    include <cinttypes>
#  else
#    include <inttypes.h>
#  endif

#  ifdef STORED_HAVE_QT
#    include <QVariant>
#  endif

namespace stored {

/*!
 * \brief %Type traits of objects in a store.
 *
 * The type is a set of flags masked into a byte.
 * It always fits in a signed char.
 */
struct Type {
	enum type
#  if STORED_cplusplus >= 201103L
		: uint8_t
#  endif
	{
		MaskSize = 0x07U,
		MaskFlags = 0x78U,
		FlagSigned = 0x08U,
		FlagInt = 0x10U,
		FlagFixed = 0x20U,
		FlagFunction = 0x40U,

		// int
		// cppcheck-suppress badBitmaskCheck
		Int8 = FlagFixed | FlagInt | FlagSigned | 0U,
		// cppcheck-suppress badBitmaskCheck
		Uint8 = FlagFixed | FlagInt | 0U,
		Int16 = FlagFixed | FlagInt | FlagSigned | 1U,
		Uint16 = FlagFixed | FlagInt | 1U,
		Int32 = FlagFixed | FlagInt | FlagSigned | 3U,
		Uint32 = FlagFixed | FlagInt | 3U,
		Int64 = FlagFixed | FlagInt | FlagSigned | 7U,
		Uint64 = FlagFixed | FlagInt | 7U,
		Int = FlagFixed | FlagInt | (sizeof(int) - 1),
		Uint = FlagFixed | (sizeof(int) - 1),

		// things with fixed length
		Float = FlagFixed | FlagSigned | 3U,
		Double = FlagFixed | FlagSigned | 7U,
		// cppcheck-suppress badBitmaskCheck
		Bool = FlagFixed | 0U,
		Pointer32 = FlagFixed | 3U,
		Pointer64 = FlagFixed | 7U,
		Pointer = (sizeof(void*) <= 4 ? Pointer32 : Pointer64),

		// (special) things with undefined length
		Void = 0U,
		Blob = 1U,
		String = 2U,

		Invalid = 0xffU
	};

	/*! \brief Checks if the given type is a function. */
	static constexpr bool isFunction(type t) noexcept
	{
		return t & FlagFunction;
	}

	/*! \brief Checks if the given type has a fixed length, or is a function with such an
	 * argument. */
	static constexpr bool isFixed(type t) noexcept
	{
		return t & FlagFixed;
	}

	/*!
	 * \brief Checks if the given type is an integer, or is a function with
	 *	such an argument.
	 */
	static constexpr bool isInt(type t) noexcept
	{
		return isFixed(t) && (t & FlagInt);
	}

	/*!
	 * \brief Checks if the given type is signed number, or is a function
	 *	with such an argument.
	 */
	static constexpr bool isSigned(type t) noexcept
	{
		return isFixed(t) && (t & FlagSigned);
	}

	/*!
	 * \brief Checks if the given type is special (non-fixed size) type, or
	 *	is a function with such an argument.
	 */
	static constexpr bool isSpecial(type t) noexcept
	{
		return (t & MaskFlags) == 0;
	}

	/*! \brief Returns the size of the (function argument) type, or 0 when it is not fixed. */
	static constexpr size_t size(type t) noexcept
	{
		return !isFixed(t) ? 0U : (size_t)(t & MaskSize) + 1U;
	}

	/*! \brief Checks if endianness of given type is swapped in the store's buffer. */
	static constexpr bool isStoreSwapped(type t) noexcept
	{
		return
#  ifdef STORED_LITTLE_ENDIAN
			!
#  endif
			Config::StoreInLittleEndian
			&& Type::isFixed(t);
	}
};

static constexpr Type::type operator|(Type::type a, Type::type b) noexcept
{
	return (Type::type)((uint8_t)a | (uint8_t)b);
}

namespace impl {
/*! \brief Returns the #stored::Type::type of the given \c int type. */
template <bool signd, size_t size>
struct toIntType {
	static stored::Type::type const type = Type::Void;
};

template <>
struct toIntType<true, 1> {
	static stored::Type::type const type = Type::Int8;
};

template <>
struct toIntType<false, 1> {
	static stored::Type::type const type = Type::Uint8;
};

template <>
struct toIntType<true, 2> {
	static stored::Type::type const type = Type::Int16;
};

template <>
struct toIntType<false, 2> {
	static stored::Type::type const type = Type::Uint16;
};

template <>
struct toIntType<true, 4> {
	static stored::Type::type const type = Type::Int32;
};

template <>
struct toIntType<false, 4> {
	static stored::Type::type const type = Type::Uint32;
};

template <>
struct toIntType<true, 8> {
	static stored::Type::type const type = Type::Int64;
};

template <>
struct toIntType<false, 8> {
	static stored::Type::type const type = Type::Uint64;
};

template <Type::type T>
struct fromType {
	typedef char* type; // Blob
};

template <>
struct fromType<stored::Type::Int8> {
	typedef int8_t type;
};

template <>
struct fromType<stored::Type::Uint8> {
	typedef uint8_t type;
};

template <>
struct fromType<stored::Type::Int16> {
	typedef int16_t type;
};

template <>
struct fromType<stored::Type::Uint16> {
	typedef uint16_t type;
};

template <>
struct fromType<stored::Type::Int32> {
	typedef int32_t type;
};

template <>
struct fromType<stored::Type::Uint32> {
	typedef uint32_t type;
};

#  ifdef ULLONG_MAX
template <>
struct fromType<stored::Type::Int64> {
	typedef int64_t type;
};

template <>
struct fromType<stored::Type::Uint64> {
	typedef uint64_t type;
};
#  endif

template <>
struct fromType<stored::Type::Float> {
	typedef float type;
};

template <>
struct fromType<stored::Type::Double> {
	typedef double type;
};

template <>
struct fromType<stored::Type::Bool> {
	typedef bool type;
};

template <>
struct fromType<stored::Type::Pointer> {
	typedef void* type;
};

template <>
struct fromType<stored::Type::Void> {
	typedef void type;
};

template <>
struct fromType<stored::Type::String> {
	typedef char* type;
};
} // namespace impl

/*!
 * \brief Returns the #stored::Type::type that corresponds to the given type \p T.
 */
template <typename T>
struct toType {
	static Type::type const type = Type::Blob;
};
template <>
struct toType<void> {
	static Type::type const type = Type::Void;
};
template <>
struct toType<bool> {
	static Type::type const type = Type::Bool;
};
template <>
struct toType<char> : public impl::toIntType<false, sizeof(char)> {};

template <>
struct toType<signed char> : public impl::toIntType<true, sizeof(char)> {};

template <>
struct toType<unsigned char> : public impl::toIntType<false, sizeof(char)> {};

template <>
struct toType<short> : public impl::toIntType<true, sizeof(short)> {};

template <>
struct toType<unsigned short> : public impl::toIntType<false, sizeof(short)> {};

template <>
struct toType<int> : public impl::toIntType<true, sizeof(int)> {};

template <>
struct toType<unsigned int> : public impl::toIntType<false, sizeof(int)> {};

template <>
struct toType<long> : public impl::toIntType<true, sizeof(long)> {};

template <>
struct toType<unsigned long> : public impl::toIntType<false, sizeof(long)> {};

#  ifdef ULLONG_MAX
// long long only exist since C99 and C++11, but many compilers do support them anyway.
template <>
struct toType<long long> : public impl::toIntType<true, sizeof(long long)> {};

template <>
struct toType<unsigned long long> : public impl::toIntType<false, sizeof(long long)> {};
#  endif

template <>
struct toType<float> {
	static Type::type const type = Type::Float;
};

template <>
struct toType<double> {
	static Type::type const type = Type::Double;
};

template <>
struct toType<char*> {
	static Type::type const type = Type::String;
};

template <typename T>
struct toType<T*> {
	static Type::type const type = Type::Pointer;
};

template <Type::type T>
struct fromType {
	typedef typename impl::fromType<(Type::type)(
		(unsigned int)T&(unsigned int)~Type::FlagFunction)>::type type;
};

template <typename Container = void>
class Variant;

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
	// cppcheck-suppress uninitMemberVar
	// cppcheck-suppress constParameter
	// cppcheck-suppress constParameterReference
	Variable(Container& container, type& buffer) noexcept
		: m_buffer(&buffer)
	{
		STORED_UNUSED(container)
		stored_assert(((uintptr_t)m_buffer & (sizeof(type) - 1U)) == 0U);
	}

	/*!
	 * \brief Constructor for an invalid Variable.
	 */
	// cppcheck-suppress uninitMemberVar
	constexpr Variable() noexcept
		: m_buffer()
	{}

	/*!
	 * \brief Copy construct, such that this Variable points to the same buffer the given
	 * Variable does.
	 */
	Variable(Variable const& v) noexcept
	{
		(*this) = v;
	}

	/*!
	 * \brief Let this Variable point to the same buffer as the given Variable.
	 */
	Variable&
	// cppcheck-suppress operatorEqVarError
	operator=( // NOLINT(bugprone-unhandled-self-assignment,cert-oop54-cpp)
		Variable const& v) noexcept
	{
		m_buffer = v.m_buffer;
		return *this;
	}

#  if STORED_cplusplus >= 201103L
	/*!
	 * \brief Move-construct.
	 */
	Variable(Variable&& v) noexcept
	{
		(*this) = std::move(v);
	}

	/*!
	 * \brief Move-assign.
	 */
	// cppcheck-suppress operatorEqVarError
	Variable& operator=(Variable&& v) noexcept
	{
		this->operator=((Variable const&)v);
		return *this;
	}

	/*!
	 * \brief Dtor.
	 */
	~Variable() noexcept = default;
#  else
	/*!
	 * \brief Dtor.
	 */
	~Variable() noexcept {}
#  endif

	/*!
	 * \brief Returns the value.
	 * \details Only call this function when it is #valid().
	 */
	type get() const noexcept
	{
		stored_assert(valid());
		return endian_s2h(buffer());
	}

	/*!
	 * \brief Returns the value, like #get(), cast to the given type.
	 * \details Only call this function when it is #valid().
	 */
	template <typename U>
	U as() const noexcept
	{
		return saturated_cast<U>(get());
	}

	/*!
	 * \brief Returns the value, which is identical to #get().
	 */
#  if STORED_cplusplus >= 201103L
	explicit
#  endif
	operator type() const noexcept
	{
		return get();
	} // NOLINT(hicpp-explicit-conversions)

	/*!
	 * \brief Sets the value.
	 * \details Only call this function when it is #valid().
	 */
	void set(type v) noexcept
	{
		stored_assert(valid());
		buffer() = endian_h2s(v);
	}

	/*!
	 * \brief Sets the value, which is identical to #set().
	 */
	Variable& operator=(type v) noexcept
	{
		set(v);
		return *this;
	}

	/*!
	 * \brief Checks if this Variable points to a valid buffer.
	 */
	constexpr bool valid() const noexcept
	{
		return m_buffer != nullptr;
	}

	/*!
	 * \brief Returns the container this Variable belongs to.
	 */
	Container& container() const; // { std::terminate(); }

	/*!
	 * \brief Checks if two Variables point to the same buffer, or are both invalid.
	 */
	constexpr bool operator==(Variable const& rhs) const noexcept
	{
		return m_buffer == rhs.m_buffer;
	}

	/*!
	 * \brief Checks if two Variables do not point to the same buffer.
	 */
	constexpr bool operator!=(Variable const& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	/*!
	 * \brief Returns the size of the data.
	 */
	static constexpr size_t size() noexcept
	{
		return sizeof(type);
	}

protected:
	/*!
	 * \brief Returns the buffer this Variable points to.
	 * \details Only call this function when it is #valid().
	 */
	type& buffer() const
	{
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
class Variable<T, Container, true> : public Variable<T, Container, false> {
public:
	typedef Variable<T, Container, false> base;
	/*! \copydoc stored::Variable::type */
	typedef typename base::type type;

	/*! \copydoc stored::Variable::Variable(Container&, type&) */
	// cppcheck-suppress uninitMemberVar
	constexpr Variable(Container& container, type& buffer) noexcept
		: base(container, buffer)
		, m_container(&container)
#  ifdef _DEBUG
		, m_entry()
#  endif
	{}

	/*! \copydoc stored::Variable::Variable() */
	// cppcheck-suppress uninitMemberVar
	constexpr Variable() noexcept
		: m_container()
#  ifdef _DEBUG
		, m_entry()
#  endif
	{}

	/*! \copydoc stored::Variable::Variable(Variable const&) */
	Variable(Variable const& v) noexcept
		: base()
		, m_container()
#  ifdef _DEBUG
		, m_entry()
#  endif
	{
		(*this) = v;
	}

	/*! \copydoc stored::Variable::operator=(Variable const&) */
	// cppcheck-suppress operatorEqVarError
	Variable& operator=( // NOLINT(bugprone-unhandled-self-assignment,cert-oop54-cpp)
		Variable const& v) noexcept
	{
#  ifdef _DEBUG
		stored_assert(m_entry == EntryNone);
#  endif
		base::operator=(v);
		m_container = v.m_container;
		return *this;
	}

#  if STORED_cplusplus >= 201103L
	/*! \copydoc stored::Variable::Variable(Variable&&) */
	Variable(Variable&& v) noexcept
#    ifdef _DEBUG
		: m_entry()
#    endif
	{
		(*this) = std::move(v);
	}

	/*! \copydoc stored::Variable::operator=(Variable&&) */
	// cppcheck-suppress operatorEqVarError
	Variable& operator=(Variable&& v) noexcept
	{
		this->operator=((Variable const&)v);
		return *this;
	}

	/*! \copydoc stored::Variable::~Variable() */
	~Variable() noexcept
#    ifdef _DEBUG
	{
		stored_assert(m_entry == EntryNone);
	}
#    else
		= default;
#    endif
#  else
	/*! \copydoc stored::Variable::~Variable() */
	~Variable() noexcept
	{
		stored_assert(m_entry == EntryNone);
	}
#  endif

	/*!
	 * \copydoc stored::Variable::get()
	 * \details #entryRO()/#exitRO() are called around the actual data retrieval.
	 */
	type get() const noexcept
	{
		entryRO();
		type res = base::get();
		exitRO();
		return res;
	}

	/*! \copydoc stored::Variable::as() */
	template <typename U>
	U as() const noexcept
	{
		return saturated_cast<U>(get());
	}

	/*!
	 * \brief Returns the value, which is identical to #get().
	 */
#  if STORED_cplusplus >= 201103L
	explicit
#  endif
	operator type() const noexcept
	{
		return get();
	} // NOLINT(hicpp-explicit-conversions)

	/*!
	 * \copydoc stored::Variable::set()
	 * \details #entryX()/#exitX() are called around the actual data retrieval.
	 */
	void set(type v) noexcept
	{
		entryX();

		bool changed = false;
		if_constexpr(Type::isStoreSwapped(toType<type>::type))
			changed = memcmp_swap(&v, &this->buffer(), sizeof(v)) != 0;
		else if_constexpr(std::numeric_limits<type>::is_integer)
			// NOLINTNEXTLINE(clang-diagnostic-float-equal)
			changed = v != this->buffer();
		else if_constexpr(sizeof(v) == sizeof(float)) {
			void const* v_ = (void const*)&v;
			void const* b_ = (void const*)&this->buffer();
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			changed = *reinterpret_cast<uint32_t const*>(v_)
				  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				  != *reinterpret_cast<uint32_t const*>(b_);
		} else {
			// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)
			changed = memcmp(&v, &this->buffer(), sizeof(v)) != 0;
		}

		if(changed)
			base::set(v);
		exitX(changed);
	}

	/*! \copydoc stored::Variable::operator=(type) */
	Variable& operator=(type v) noexcept
	{
		set(v);
		return *this;
	}

	/*! \copydoc stored::Variable::size() */
	static constexpr size_t size() noexcept
	{
		return sizeof(type);
	}

	/*! \copydoc stored::Variable::container() */
	Container& container() const noexcept
	{
		stored_assert(this->valid());
		return *m_container; // NOLINT(clang-analyzer-core.uninitialized.UndefReturn)
	}

	/*!
	 * \brief Returns the key that belongs to this Variable.
	 * \see your store's bufferToKey()
	 */
	typename Container::Key key() const noexcept
	{
		return container().bufferToKey(&this->buffer());
	}

	/*!
	 * \brief Calls the \c entryX() hook of the container.
	 * \see your store's \c hookEntryX()
	 */
	void entryX() const noexcept
	{
		container().hookEntryX(toType<T>::type, &this->buffer(), sizeof(type));
#  ifdef _DEBUG
		stored_assert(m_entry == EntryNone);
		m_entry = EntryX;
#  endif
	}

	/*!
	 * \brief Calls the \c exitX() hook of the container.
	 * \see your store's \c hookExitX()
	 */
	void exitX(bool changed) const noexcept
	{
#  ifdef _DEBUG
		stored_assert(m_entry == EntryX);
		m_entry = EntryNone;
#  endif
		container().hookExitX(toType<T>::type, &this->buffer(), sizeof(type), changed);
	}

	/*!
	 * \brief Calls the \c entryRO() hook of the container.
	 * \see your store's \c hookEntryRO()
	 */
	void entryRO() const noexcept
	{
		container().hookEntryRO(toType<T>::type, &this->buffer(), sizeof(type));
#  ifdef _DEBUG
		stored_assert(m_entry == EntryNone);
		m_entry = EntryRO;
#  endif
	}

	/*!
	 * \brief Calls the \c exitRO() hook of the container.
	 * \see your store's \c hookExitRO()
	 */
	void exitRO() const noexcept
	{
#  ifdef _DEBUG
		stored_assert(m_entry == EntryRO);
		m_entry = EntryNone;
#  endif
		container().hookExitRO(toType<T>::type, &this->buffer(), sizeof(type));
	}

private:
	/*! \brief The container of this Variable. */
	Container* m_container;
#  ifdef _DEBUG
	enum {EntryNone, EntryRO, EntryX};
	/*! \brief Tracking entry/exit calls. */
	mutable uint_fast8_t m_entry;
#  endif
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
		: m_container(&container)
		, m_f(f)
	{}

	/*!
	 * \brief Constructor for an invalid Function.
	 */
	constexpr Function() noexcept
		: m_container()
		, m_f()
	{}

	/*!
	 * \brief Calls the function and return its value.
	 * \details Only call this function when it is #valid().
	 */
	type get() const
	{
		stored_assert(valid());
		type value = type();
		callback(false, value);
		return value;
	}

	/*! \copydoc Variable::as() */
	template <typename U>
	U as() const
	{
		return saturated_cast<U>(get());
	}

	/*!
	 * \brief Returns the value, which is identical to #get().
	 */
#  if STORED_cplusplus >= 201103L
	explicit
#  endif
	operator type() const
	{
		return get();
	} // NOLINT(hicpp-explicit-conversions)

	/*!
	 * \brief Calls the function and write its value in the given buffer.
	 * \details Only call this function when it is #valid().
	 * \param dst the buffer to write to
	 * \param len the length of \p dst, normally equal to #size()
	 * \return the number of bytes written to \p dst
	 */
	size_t get(void* dst, size_t len) const
	{
		stored_assert(valid());
		return callback(false, dst, len);
	}

	/*!
	 * \brief Call the function to write the value.
	 * \details Only call this function when it is #valid().
	 */
	void set(type value) const
	{
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
	size_t set(void* src, size_t len)
	{
		stored_assert(valid());
		return callback(true, src, len);
	}

	/*!
	 * \brief Call the function, like #get().
	 */
	type operator()() const
	{
		return get();
	}

	/*!
	 * \brief Call the function, like #set().
	 */
	void operator()(type value)
	{
		set(value);
	}

	/*! \copydoc Variable::operator=(type) */
	Function& operator=(type v)
	{
		set(v);
		return *this;
	}

	/*!
	 * \brief Checks if this Function is valid.
	 */
	constexpr bool valid() const noexcept
	{
		return m_f > 0;
	}

	/*!
	 * \brief Returns the container this Function belongs to.
	 * \details Only call this function when it is #valid().
	 */
	Container& container() const noexcept
	{
		stored_assert(valid());
		return *m_container;
	}

	/*!
	 * \brief Invoke the callback at the #container().
	 */
	size_t callback(bool set, type& value) const
	{
		stored_assert(valid());
		return container().callback(set, &value, sizeof(type), id());
	}

	/*!
	 * \brief Invoke the callback at the #container().
	 */
	size_t callback(bool set, void* buffer, size_t len) const
	{
		stored_assert(valid());

		if(!Config::UnalignedAccess
		   // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		   && ((uintptr_t)buffer & (std::min(sizeof(void*), sizeof(type)) - 1U))) {
			// Unaligned access, do the callback on a local buffer.
			stored_assert(len >= sizeof(type));
			type v;

			if(set) {
				container().callback(true, &v, sizeof(v), id());
				memcpy(buffer, &v, sizeof(v));
			} else {
				memcpy(&v, buffer, sizeof(v));
				container().callback(false, &v, sizeof(v), id());
			}

			return sizeof(v);
		} else
			return container().callback(set, buffer, len, id());
	}

	/*!
	 * \brief Returns the function ID.
	 * \details Only call this function when it is #valid().
	 */
	unsigned int id() const noexcept
	{
		stored_assert(valid());
		return m_f;
	}

	/*!
	 * \brief Checks if this Function points to the same Function as the given one.
	 */
	bool operator==(Function const& rhs) const noexcept
	{
		if(valid() != rhs.valid())
			return false;
		if(!valid())
			return true;
		return m_container == rhs.m_container && m_f == rhs.m_f;
	}

	/*!
	 * \brief Checks if this Function points to the same Function as the given one.
	 */
	bool operator!=(Function const& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	/*!
	 * \brief Returns the size of the function's argument.
	 */
	static constexpr size_t size() noexcept
	{
		return sizeof(type);
	}

private:
	/*! \brief The container this Function belongs to. */
	Container* m_container;
	/*! \brief The function ID. */
	unsigned int m_f;
};

/*!
 * \brief A typed variable, which is not yet bound to a store.
 *
 * For C++14, you can construct this object as constexpr via
 * #stored::find(), resulting in a #stored::Variant<void>, which is applied
 * to a Container type. From this object, the conversion to a Variable is
 * very cheap.
 */
template <typename T, typename Container_>
class FreeVariable {
public:
	/*! \brief The type of the variable. */
	typedef T type;
	/*! \brief The container type that holds this variable. */
	typedef Container_ Container;
	/*! \brief The full Variable type. */
	typedef Variable<type, Container> Variable_type;
	/*! \brief The full (bound) Variable type. */
	typedef Variable_type Bound_type;
	/*! \brief A type that is able to store the store's buffer offset. */
	typedef typename value_type<static_cast<uintmax_t>(Container::BufferSize)>::fast_type
		offset_type;

	/*! \brief Constructor for an invalid variable. */
	constexpr FreeVariable() noexcept
		: m_offset((offset_type)Container::BufferSize)
	{}

protected:
	/*!
	 * \brief Constructor for a valid variable.
	 * \details This can only be called by #stored::Variant<void>::variable().
	 */
	constexpr explicit FreeVariable(size_t offset) noexcept
		: m_offset(static_cast<offset_type>(offset))
	{
#  if STORED_cplusplus < 201103L || STORED_cplusplus >= 201402L
		// For C++11, the body must be empty.
		stored_assert(offset < std::numeric_limits<offset_type>::max());
#  endif
	}

	friend class Variant<void>;

public:
	/*! \brief Returns if this variable is valid. */
	constexpr bool valid() const noexcept
	{
		return m_offset != (offset_type)Container::BufferSize;
	}

	/*! \brief Convert this free variable into a bound one. */
	Variable_type apply(Container& container) const noexcept
	{
		if(valid())
			return apply_(container);
		else
			return Variable_type();
	}

	/*! \brief Convert this free variable into a bound one, without validity checking. */
	Variable_type apply_(Container& container) const noexcept
	{
		stored_assert(valid());
		// cppcheck-suppress invalidPointerCast
		return Variable_type(
			container,
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			*reinterpret_cast<type*>(
				static_cast<char*>(container.buffer()) + m_offset));
	}

	/*! \brief Check if two free variables are identical. */
	constexpr bool operator==(FreeVariable const& other) const
	{
		return m_offset == other.m_offset;
	}

	/*! \brief Check if two free variables are different. */
	constexpr bool operator!=(FreeVariable const& other) const
	{
		return !((*this) == other);
	}

private:
	/*! \brief The offset within the buffer of a store. */
	offset_type m_offset;
};

/*!
 * \brief A typed function, which is not yet bound to a store.
 *
 * For C++14, you can construct this object as constexpr via
 * #stored::find(), resulting in a #stored::Variant<void>, which is applied
 * to a Container type. From this object, the conversion to a Function is
 * very cheap.
 */
template <typename T, typename Container_>
class FreeFunction {
public:
	/*! \brief The type of the function argument. */
	typedef T type;
	/*! \brief The container type that holds this variable. */
	typedef Container_ Container;
	/*! \brief The full Function type. */
	typedef Function<type, Container> Function_type;
	/*! \brief The full (bound) Function type. */
	typedef Function_type Bound_type;
	/*! \brief A type that is able to store the store's buffer offset. */
#  ifdef DOXYGEN
	typedef unsigned int f_type;
#  else
	typedef typename value_type<static_cast<uintmax_t>(
		Container::FunctionCount > 0 ? Container::FunctionCount - 1 : 0)>::type f_type;
#  endif

	/*! \brief Constructor for an invalid variable. */
	constexpr FreeFunction() noexcept
		: m_f()
	{}

protected:
	/*!
	 * \brief Constructor for a valid variable.
	 * \details This can only be called by #stored::Variant<void>::variable().
	 */
	constexpr explicit FreeFunction(unsigned int f) noexcept
		: m_f(static_cast<f_type>(f))
	{
#  if STORED_cplusplus < 201103L || STORED_cplusplus >= 201402L
		// For C++11, the body must be empty.
		stored_assert(f < std::numeric_limits<f_type>::max());
#  endif
	}

	friend class Variant<void>;

public:
	/*! \brief Returns if this function is valid. */
	constexpr bool valid() const noexcept
	{
		return m_f != 0U;
	}

	/*! \brief Convert this free function into a bound one. */
	Function_type apply(Container& container) const noexcept
	{
		if(valid())
			return apply_(container);
		else
			return Function_type();
	}

	/*! \brief Convert this free function into a bound one, without validity checking. */
	Function_type apply_(Container& container) const noexcept
	{
		stored_assert(valid());
		return Function_type(container, m_f);
	}

	/*! \brief Check if two free functions are identical. */
	constexpr bool operator==(FreeFunction const& other) const
	{
		return m_f == other.m_f;
	}

	/*! \brief Check if two free functions are different. */
	constexpr bool operator!=(FreeFunction const& other) const
	{
		return !((*this) == other);
	}

private:
	/*! \brief The function ID. */
	f_type m_f;
};

/*!
 * \brief A untyped interface to an object in a store.
 *
 * This class works for all variables and functions of all types.
 * However, using #stored::Variable or #stored::Function is more efficient both in performance and
 * memory. Use those when you can.
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
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	Variant(Container& container, Type::type type, void* buffer, size_t len) noexcept
		: m_container(&container)
		, m_buffer(buffer)
		, m_len(len)
		, m_type((uint8_t)type)
#  ifdef _DEBUG
		, m_entry()
#  endif
	{
		stored_assert(!Type::isFunction(type));
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		stored_assert(
			!Type::isFixed(this->type())
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			|| (reinterpret_cast<uintptr_t>(buffer) & (Type::size(this->type()) - 1))
				   == 0);
	}

	/*!
	 * \brief Constructor for a function.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	Variant(Container& container, Type::type type, unsigned int f, size_t len) noexcept
		: m_container(&container)
		, m_f((uintptr_t)f)
		, m_len(len)
		, m_type((uint8_t)type)
#  ifdef _DEBUG
		, m_entry()
#  endif
	{
		stored_assert(Type::isFunction(type));
		static_assert(sizeof(uintptr_t) >= sizeof(unsigned int), "");
	}

	/*!
	 * \brief Constructor for an invalid Variant.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	constexpr Variant() noexcept
		: m_container()
		, m_buffer()
		, m_len()
		, m_type()
#  ifdef _DEBUG
		, m_entry()
#  endif
	{}

	/*!
	 * \brief Constructor for a Variable.
	 */
	template <typename T>
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	constexpr explicit Variant(Variable<T, Container> const& v) noexcept
		: m_container(v.valid() ? &v.container() : nullptr)
		, m_buffer(v.valid() ? &v.buffer() : nullptr)
		, m_len(sizeof(T))
		, m_type(toType<T>::type)
#  ifdef _DEBUG
		, m_entry()
#  endif
	{}

	/*!
	 * \brief Constructor for a Function.
	 */
	template <typename T>
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	constexpr explicit Variant(Function<T, Container> const& f) noexcept
		: m_container(f.valid() ? &f.container() : nullptr)
		, m_f(f.valid() ? f.id() : 0)
		, m_len(sizeof(T))
		, m_type(f.type())
#  ifdef _DEBUG
		, m_entry()
#  endif
	{}

	/*!
	 * \brief Get the value.
	 * \details For variables, #entryRO()/#exitRO() is called.
	 * \details In case #type() is Type::String, only up to the first null byte are copied.
	 *          If \p dst is sufficiently large (\p len &gt; #size()), a null terminator is
	 *          always written after the string.
	 * \details Only call this function when #valid().
	 * \param dst the buffer to copy to
	 * \param len the length of \p dst, when this is a fixed type, 0 implies the normal size
	 * \return the number of bytes written into \p dst
	 */
	size_t get(void* dst, size_t len = 0) const
	{
		if(Type::isFixed(type())) {
			stored_assert(len == size() || len == 0);
			len = size();
		} else {
			len = std::min(len, size());
		}

		if(Type::isFunction(type())) {
			len = callback(false, dst, len);
		} else {
			entryRO(len);
			if(unlikely(type() == Type::String)) {
				char* dst_ = static_cast<char*>(dst);
				char const* buffer_ = static_cast<char const*>(m_buffer);
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
	template <typename T>
	T get() const
	{
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
	Vector<char>::type get() const
	{
		Vector<char>::type buf(size());
		get(buf.data(), buf.size());
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
	size_t set(void const* src, size_t len = 0)
	{
		if(Type::isFixed(type())) {
			stored_assert(len == size() || len == 0);
			len = size();
		} else {
			len = std::min(len, size());
		}

		if(isFunction()) {
			len = callback(
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				true, const_cast<void*>(src), len);
		} else {
			bool changed = true;
			if(unlikely(type() == Type::String)) {
				// The byte after the buffer of the string is reserved for the \0.
				size_t changed_len = len + 1U;
				entryX(changed_len);

				// src may not be null-terminated.
				char const* src_ = static_cast<char const*>(src);
				char* buffer_ = static_cast<char*>(m_buffer);

				if(Config::EnableHooks)
					changed = strncmp(src_, len, buffer_, len + 1U) != 0;

				if(changed) {
					len = strncpy(buffer_, src_, len);
					buffer_[len] = '\0';
				}

				exitX(changed, changed_len);
			} else {
				entryX(len);

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

				exitX(changed, len);
			}
		}

		return len;
	}

	/*!
	 * \brief Wrapper for #set(void const*,size_t) that converts the type.
	 * \details This only works for fixed types. Make sure that #type() matches \p T.
	 */
	template <typename T>
	void set(T value)
	{
		stored_assert(Type::isFixed(type()));
		stored_assert(toType<T>::type == type());
		stored_assert(sizeof(T) == size());
		set(&value, sizeof(T));
	}

	/*!
	 * \brief Sets the value.
	 * \see #set(void const*, size_t)
	 */
	void set(Vector<char>::type const& data)
	{
		set(data.data(), data.size());
	}

	/*!
	 * \brief Sets a string.
	 * \details Only works if this variant is a string.
	 */
	void set(char const* data)
	{
		stored_assert((type() & ~Type::FlagFunction) == Type::String);
		set(data, strlen(data));
	}

	/*!
	 * \brief Invoke the function callback.
	 * \details Only works if this variant is a function.
	 */
	size_t callback(bool set, void* buffer, size_t len) const
	{
		stored_assert(valid() && isFunction());
		size_t size_ = Type::size(type());

		if(!Config::UnalignedAccess
		   && Type::isFixed(type())
		   // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		   && ((uintptr_t)buffer & (std::min(sizeof(void*), size_) - 1U))) {
			// Unaligned access, do the callback on a local buffer.
			stored_assert(size_ <= sizeof(uint64_t) && len >= size_);
			uint64_t v = 0;

			if(set) {
				container().callback(true, &v, size_, (unsigned int)m_f);
				memcpy(buffer, &v, size_);
			} else {
				memcpy(&v, buffer, size_);
				container().callback(false, &v, size_, (unsigned int)m_f);
			}

			return size_;
		} else
			return container().callback(set, buffer, len, (unsigned int)m_f);
	}

#  ifdef STORED_HAVE_QT
	/*!
	 * \brief Convert the value to a QVariant.
	 */
	QVariant toQVariant() const
	{
		switch(type()) {
		case Type::Int8:
			return (int)get<int8_t>();
		case Type::Int16:
			return (int)get<int16_t>();
		case Type::Int32:
			return get<int32_t>();
		case Type::Int64:
			return (qlonglong)get<int64_t>();
		case Type::Uint8:
			return (unsigned int)get<uint8_t>();
		case Type::Uint16:
			return (unsigned int)get<uint16_t>();
		case Type::Uint32:
			return get<uint32_t>();
		case Type::Uint64:
			return (qulonglong)get<uint64_t>();
		case Type::Float:
			return get<float>();
		case Type::Double:
			return get<double>();
		case Type::Bool:
			return get<bool>();
		case Type::String: {
			QByteArray buf{(int)size(), 0};
			get(buf.data(), (size_t)buf.size());
			return QString{buf};
		}
		default:
			return QVariant{};
		}
	}

	/*!
	 * \brief Set the value, given a QVariant.
	 * \return \c true upon success
	 */
	bool set(QVariant const& v)
	{
		if(!v.isValid())
			return false;

#    define CASE_TYPE(T)          \
    case stored::toType<T>::type: \
      if(!v.canConvert<T>())      \
	return false;             \
      set<T>(v.value<T>());       \
      return true;

		switch(type() & ~Type::FlagFunction) {
			CASE_TYPE(int8_t)
			CASE_TYPE(uint8_t)
			CASE_TYPE(int16_t)
			CASE_TYPE(uint16_t)
			CASE_TYPE(int32_t)
			CASE_TYPE(uint32_t)
			CASE_TYPE(qlonglong)
			CASE_TYPE(qulonglong)
			CASE_TYPE(float)
			CASE_TYPE(double)
			CASE_TYPE(bool)
		case stored::Type::String: {
			if(!v.canConvert<QString>())
				return false;

			auto s = v.value<QString>().toUtf8();
			set(s.data(), (size_t)s.size());
			return true;
		}
		default:
			return false;
		}
#    undef CASE_TYPE
	}

	/*!
	 * \brief Convert the value to a QString.
	 *
	 * Only works if the #type() is String.
	 */
	QString toQString() const
	{
		stored_assert((type() & ~Type::FlagFunction) == Type::String);
		QByteArray buf{(int)size(), 0};
		get(buf.data(), (size_t)buf.size());
		return QString{buf};
	}

	/*!
	 * \brief Sets a string.
	 * \details Only works if this variant is a string.
	 */
	void set(QString const& value)
	{
		auto buf = value.toUtf8();
		set(buf.constData());
	}
#  endif // STORED_HAVE_QT

	/*!
	 * \brief Invokes \c hookEntryX() on the #container().
	 */
	void entryX() const noexcept
	{
		entryX(size());
	}

	/*! \copydoc entryX() */
	void entryX(size_t len) const noexcept
	{
		if(Config::EnableHooks) {
			container().hookEntryX(type(), m_buffer, len);
#  ifdef _DEBUG
			stored_assert(m_entry == EntryNone);
			m_entry = EntryX;
#  endif
		}
	}

	/*!
	 * \brief Invokes \c hookExitX() on the #container().
	 */
	void exitX(bool changed) const noexcept
	{
		exitX(changed, size());
	}

	/*! \copydoc exitX() */
	void exitX(bool changed, size_t len) const noexcept
	{
		if(Config::EnableHooks) {
#  ifdef _DEBUG
			stored_assert(m_entry == EntryX);
			m_entry = EntryNone;
#  endif
			container().hookExitX(type(), m_buffer, len, changed);
		}
	}

	/*!
	 * \brief Invokes \c hookEntryRO() on the #container().
	 */
	void entryRO() const noexcept
	{
		entryRO(size());
	}

	/*! \copydoc entryRO() */
	void entryRO(size_t len) const noexcept
	{
		if(Config::EnableHooks) {
			container().hookEntryRO(type(), m_buffer, len);
#  ifdef _DEBUG
			stored_assert(m_entry == EntryNone);
			m_entry = EntryRO;
#  endif
		}
	}

	/*!
	 * \brief Invokes \c hookExitRO() on the #container().
	 */
	void exitRO() const noexcept
	{
		exitRO(size());
	}

	/*! \copydoc exitRO() */
	void exitRO(size_t len) const noexcept
	{
		if(Config::EnableHooks) {
#  ifdef _DEBUG
			stored_assert(m_entry == EntryRO);
			m_entry = EntryNone;
#  endif
			container().hookExitRO(type(), m_buffer, len);
		}
	}

	/*!
	 * \brief Returns the type.
	 * \details Only call this function when it is #valid().
	 */
	Type::type type() const noexcept
	{
		stored_assert(valid());
		return (Type::type)m_type;
	}

	/*!
	 * \brief Returns the size.
	 *
	 * In case #type() is Type::String, this returns the maximum size of
	 * the string, excluding null terminator.
	 *
	 * Only call this function when it is #valid().
	 */
	size_t size() const noexcept
	{
		stored_assert(valid());
		return Type::isFixed(type()) ? Type::size(type()) : m_len;
	}

	/*!
	 * \brief Returns the buffer.
	 * \details Only call this function when it is #valid().
	 */
	void* buffer() const noexcept
	{
		stored_assert(isVariable());
		return m_buffer;
	}

	/*!
	 * \brief Checks if this Variant is valid.
	 */
	constexpr bool valid() const noexcept
	{
		return m_buffer != nullptr;
	}

	/*!
	 * \brief Checks if the #type() is a function.
	 * \details Only call this function when it is #valid().
	 */
	bool isFunction() const noexcept
	{
		stored_assert(valid());
		return Type::isFunction(type());
	}

	/*!
	 * \brief Checks if the #type() is a variable.
	 * \details Only call this function when it is #valid().
	 */
	bool isVariable() const noexcept
	{
		stored_assert(valid());
		return !isFunction();
	}

	/*!
	 * \brief Returns the container.
	 * \details Only call this function when it is #valid().
	 */
	Container& container() const noexcept
	{
		stored_assert(valid());
		return *m_container;
	}

	/*!
	 * \brief Returns a #stored::Variable that corresponds to this Variant.
	 * \details Only call this function when it #isVariable() and the #type() matches \p T.
	 */
	template <typename T>
	Variable<T, Container> variable() const noexcept
	{
		if(unlikely(!valid()))
			return Variable<T, Container>();

		stored_assert(isVariable());
		stored_assert(Type::isFixed(type()));
		stored_assert(toType<T>::type == type());
		stored_assert(sizeof(T) == size());
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return Variable<T, Container>(container(), *reinterpret_cast<T*>(m_buffer));
	}

	/*!
	 * \brief Returns a #stored::Function that corresponds to this Variant.
	 * \details Only call this function when it #isFunction() and the #type() matches \p T.
	 */
	template <typename T>
	Function<T, Container> function() const noexcept
	{
		if(unlikely(!valid()))
			return Function<T, Container>();

		stored_assert(isFunction());
		stored_assert(Type::isFixed(type()));
		stored_assert(toType<T>::type == (type() & ~Type::FlagFunction));
		stored_assert(sizeof(T) == size());
		return Function<T, Container>(container(), (unsigned int)m_f);
	}

	/*!
	 * \brief Returns the key of this variable.
	 * \details Only call this function when it #isVariable().
	 * \see your store's \c bufferToKey()
	 */
	typename Container::Key key() const noexcept
	{
		stored_assert(isVariable());
		return container()->bufferToKey(m_buffer);
	}

	/*!
	 * \brief Checks if this Variant points to the same object as the given one.
	 */
	bool operator==(Variant const& rhs) const noexcept
	{
		if(valid() != rhs.valid())
			return false;
		if(!valid())
			return true;
		return m_type == rhs.m_type && m_container == rhs.m_container
		       && (isFunction() ? m_f == rhs.m_f
					: m_buffer == rhs.m_buffer && m_len == rhs.m_len);
	}

	/*!
	 * \brief Checks if this Variant points to the same object as the given one.
	 */
	bool operator!=(Variant const& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	/*!
	 * \brief Copies data from a Variant from another Container.
	 *
	 * This copies data directly, without type conversion.  This may come
	 * in handy when data of the same variable of different stores (or
	 * between stores with different wrappers) should be copied.
	 *
	 * Only use this when:
	 *
	 * - this and other are valid()
	 * - this and other are not equal
	 * - this and other have the same type()
	 * - this and other have the same size()
	 * - this and other are variables
	 */
	template <typename C>
	void copy(Variant<C> const& other) noexcept
	{
		stored_assert(valid() && other.valid());
		stored_assert(buffer() != other.buffer());
		stored_assert(type() == other.type());
		stored_assert(size() == other.size());
		stored_assert(isVariable() && other.isVariable());

		size_t len = size();
		bool changed = true;

		other.entryRO(len);
		entryX(len);

		if(type() == Type::String) {
			if(Config::EnableHooks)
				changed = ::strncmp(
						  static_cast<char*>(buffer()),
						  static_cast<char const*>(other.buffer()), len)
					  != 0;
			if(changed)
				strncpy(static_cast<char*>(buffer()),
					static_cast<char const*>(other.buffer()), len);
		} else {
			if(Config::EnableHooks)
				changed = memcmp(buffer(), other.buffer(), len) != 0;
			if(changed)
				memcpy(buffer(), other.buffer(), len);
		}

		exitX(changed, len);
		other.exitRO(len);
	}

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
#  ifdef _DEBUG
	enum { EntryNone, EntryRO, EntryX };
	/*! \brief Tracking entry/exit calls. */
	mutable uint8_t m_entry;
#  endif
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
	 * \brief Constructor for a variable or function.
	 */
	constexpr Variant(Type::type type, uintptr_t buffer_offset_or_f, size_t len) noexcept
		: m_dummy()
		, m_offset(buffer_offset_or_f)
		, m_len(len)
		, m_type((uint8_t)type)
#  ifdef _DEBUG
		, m_entry()
#  endif
	{
		static_assert(sizeof(uintptr_t) >= sizeof(unsigned int), "");
	}

	/*!
	 * \brief Constructor for an invalid Variant.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	constexpr Variant() noexcept
		: m_dummy()
		, m_offset()
		, m_len()
		, m_type((uint8_t)Type::Invalid)
#  ifdef _DEBUG
		, m_entry()
#  endif
	{}

	/*!
	 * \brief Apply the stored object properties to a container.
	 */
	template <typename Container>
	Variant<Container> apply(Container& container) const noexcept
	{
		static_assert(sizeof(Variant<Container>) == sizeof(Variant<>), "");

		if(!valid())
			return Variant<Container>();
		else if(isFunction())
			return Variant<Container>(
				container, (Type::type)m_type, (unsigned int)m_offset, m_len);
		else {
			stored_assert(m_offset + m_len <= sizeof(typename Container::Data));
			char* buf = container.buffer();
			return Variant<Container>(
				container, (Type::type)m_type, buf + m_offset, m_len);
		}
	}

	/*!
	 * \brief Get the typed variable corresponding to this variant.
	 */
	template <typename T, typename Container>
	Variable<T, Container> variable(Container& container) const noexcept
	{
		return apply<Container>(container).template variable<T>();
	}

	/*!
	 * \brief Get the typed variable corresponding to this variant, which is not bound to a
	 * specific store yet.
	 */
	template <typename T, typename Container>
	constexpr14 FreeVariable<T, Container> variable() const noexcept
	{
		if(!valid())
			return FreeVariable<T, Container>();

		stored_assert(isVariable());
		stored_assert(Type::isFixed(type()));
		stored_assert(toType<T>::type == type());
		stored_assert(sizeof(T) == size());
		stored_assert(m_offset + m_len <= sizeof(typename Container::Data));
		return FreeVariable<T, Container>(m_offset);
	}

	/*!
	 * \brief Get the typed function corresponding to this variant.
	 */
	template <typename T, typename Container>
	Variable<T, Container> function(Container& container) const noexcept
	{
		return apply<Container>(container).template function<T>();
	}

	/*!
	 * \brief Get the typed function corresponding to this variant, which is not bound to a
	 * specific store yet.
	 */
	template <typename T, typename Container>
	constexpr14 FreeFunction<T, Container> function() const noexcept
	{
		if(!valid())
			return FreeFunction<T, Container>();

		stored_assert(isFunction());
		stored_assert(Type::isFixed(type()));
		stored_assert(
			toType<T>::type
			== (Type::type)((unsigned int)type() & (unsigned int)~Type::FlagFunction));
		stored_assert(sizeof(T) == size());
		return FreeFunction<T, Container>((unsigned int)m_offset);
	}

	/*! \brief Don't use. */
	size_t get(void* dst, size_t len = 0) const noexcept
	{
		STORED_UNUSED(dst)
		STORED_UNUSED(len)
		stored_assert(valid());
		return 0;
	}

	/*! \brief Don't use. */
	template <typename T>
	T get() const noexcept
	{
		stored_assert(valid());
		return T();
	}

	/*! \brief Don't use. */
	size_t set(void const* src, size_t len = 0) noexcept
	{
		STORED_UNUSED(src)
		STORED_UNUSED(len)
		stored_assert(valid());
		return 0;
	}

	/*! \brief Don't use. */
	template <typename T>
	void set(T value) noexcept
	{
		STORED_UNUSED(value)
		stored_assert(valid());
	}

	/*! \brief Don't use. */
	void entryX(size_t len = 0) const noexcept
	{
		STORED_UNUSED(len)
	}
	/*! \brief Don't use. */
	void exitX(bool changed, size_t len = 0) const noexcept
	{
		STORED_UNUSED(changed)
		STORED_UNUSED(len)
	}
	/*! \brief Don't use. */
	void entryRO(size_t len = 0) const noexcept
	{
		STORED_UNUSED(len)
	}
	/*! \brief Don't use. */
	void exitRO(size_t len = 0) const noexcept
	{
		STORED_UNUSED(len)
	}
	/*! \copybrief Variant::type() */
	constexpr Type::type type() const noexcept
	{
		return (Type::type)m_type;
	}

	/*! \copybrief Variant::size() */
	constexpr14 size_t size() const noexcept
	{
		stored_assert(valid());
		return Type::isFixed(type()) ? Type::size(type()) : m_len;
	}

	/*! \copybrief Variant::valid() */
	constexpr bool valid() const noexcept
	{
		return type() != Type::Invalid;
	}

	/*! \copybrief Variant::isFunction() */
	constexpr14 bool isFunction() const noexcept
	{
		stored_assert(valid());
		return Type::isFunction(type());
	}

	/*! \copybrief Variant::isVariable() */
	constexpr14 bool isVariable() const noexcept
	{
		stored_assert(valid());
		return !isFunction();
	}

	/*! \brief Don't use. */
	int& container() const noexcept
	{
		stored_assert(valid());
		std::terminate();
	}

	/*! \copybrief Variant::operator==() */
	bool operator==(Variant const& rhs) const noexcept
	{
		if(valid() != rhs.valid())
			return false;
		if(!valid())
			return true;
		return m_type == rhs.m_type && m_offset == rhs.m_offset && m_len == rhs.m_len;
	}

	/*! \copybrief Variant::operator!=() */
	bool operator!=(Variant const& rhs) const noexcept
	{
		return !(*this == rhs);
	}

private:
	// Make this class the same size as a non-void container.
#  ifdef __clang__
	void* m_dummy __attribute__((unused));
#  else
	void* m_dummy;
#  endif

	/*!
	 * \brief Encodes either the store's buffer offset or function.
	 */
	uintptr_t m_offset;
	/*! \copydoc Variant::m_len */
	size_t m_len;
	/*! \copydoc Variant::m_type */
	uint8_t m_type;
#  ifdef _DEBUG
	/*! \copydoc Variant::m_entry */
#    ifdef __clang__
	uint8_t m_entry __attribute__((unused));
#    else
	uint8_t m_entry;
#    endif
#  endif
};


namespace impl {
template <typename StoreBase, typename T>
static constexpr void* objectToVoidPtr(T& o) noexcept
{
	static_assert(sizeof(T) == sizeof(typename StoreBase::Objects), "");
	return (void*)&o;
}

template <typename StoreBase, typename T>
static constexpr StoreBase& objectToStore(T& o) noexcept
{
	static_assert(sizeof(T) == sizeof(typename StoreBase::Objects), "");
	return *static_cast<StoreBase*>(
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		reinterpret_cast<typename StoreBase::Objects*>(objectToVoidPtr<StoreBase, T>(o)));
}

/*!
 * \brief Variable class as used in the *Objects base class of a store.
 *
 * Do not use. Only the *Objects class is allowed to use it.
 */
template <typename Store, typename Implementation, typename T, size_t offset, size_t size_>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class StoreVariable {
public:
#  if STORED_cplusplus >= 201103L
	// Prevent accidental copying this object.  This only
	// works for C++11, as these special functions must be
	// trivial otherwise being used as union member.
	StoreVariable(StoreVariable const&) = delete;
	StoreVariable(StoreVariable&&) = delete;
	void operator=(StoreVariable const&) = delete;
	void operator=(StoreVariable&&) = delete;
#  endif

	typedef T type;
	typedef Variable<type, Implementation> Variable_type;
	typedef Variant<Implementation> Variant_type;

	static constexpr uintptr_t key() noexcept
	{
		return static_cast<uintptr_t>(offset);
	}

	constexpr Variable_type variable() const noexcept
	{
		static_assert(size_ == sizeof(type), "");
		return objectToStore<Store>(*this).template _variable<type>(offset);
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Variable_type() const noexcept
	{
		return variable();
	}

	constexpr Variant_type variant() const noexcept
	{
		return Variant_type(variable());
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Variant_type() const noexcept
	{
		return variant();
	}

	type get() const noexcept
	{
		return variable().get();
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	operator type() const noexcept
	{
		return get();
	}

	template <typename U>
	U as() const noexcept
	{
		return saturated_cast<U>(get());
	}

	void set(type value) noexcept
	{
		variable().set(value);
	}

	// NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
	Variable_type operator=(type value) noexcept
	{
		Variable_type v = variable();
		v.set(value);
		return v;
	}

	static constexpr size_t size()
	{
		return sizeof(type);
	}
};

/*!
 * \brief Function class as used in the *Objects base class of a store.
 *
 * Do not use. Only the *Objects class is allowed to use it.
 */
template <
	typename Store, typename Implementation,
	template <typename, unsigned int> class FunctionMap, unsigned int F>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class StoreFunction {
public:
#  if STORED_cplusplus >= 201103L
	StoreFunction(StoreFunction const&) = delete;
	StoreFunction(StoreFunction&&) = delete;
	void operator=(StoreFunction const&) = delete;
	void operator=(StoreFunction&&) = delete;
#  endif

	typedef typename FunctionMap<Implementation, F>::type type;
	typedef Function<type, Implementation> Function_type;
	typedef Variant<Implementation> Variant_type;

	static constexpr unsigned int id() noexcept
	{
		return F;
	}

	constexpr Function_type function() const noexcept
	{
		return objectToStore<Store>(*this).template _function<type>(F);
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Function_type() const noexcept
	{
		return function();
	}

	constexpr Variant_type variant() const noexcept
	{
		return Variant_type(function());
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Variant_type() const noexcept
	{
		return variant();
	}

	type get() const
	{
		type v;
		call(false, v);
		return v;
	}

#  if STORED_cplusplus >= 201103L
	explicit
#  endif
	operator type() const
	{
		return get();
	} // NOLINT(hicpp-explicit-conversions)

	template <typename U>
	U as() const
	{
		return saturated_cast<U>(get());
	}

	size_t get(void* dst, size_t len) const
	{
		STORED_UNUSED(len)
		stored_assert(len == sizeof(type));
		stored_assert(dst);
		call(false, *static_cast<type*>(dst));
		return sizeof(type);
	}

	type operator()() const
	{
		return get();
	}

	void set(type value)
	{
		call(true, value);
	}

	size_t set(void* src, size_t len)
	{
		STORED_UNUSED(len)
		stored_assert(len == sizeof(type));
		stored_assert(src);
		call(true, *static_cast<type*>(src));
		return sizeof(type);
	}

	void operator()(type value)
	{
		function()(value);
	}

	StoreFunction& operator=(type value)
	{
		set(value);
		return *this;
	}

	static constexpr size_t size() noexcept
	{
		return sizeof(type);
	}

protected:
	constexpr Implementation& implementation() const noexcept
	{
		return static_cast<Implementation&>(objectToStore<Store>(*this));
	}

	void call(bool set, type& value) const
	{
		FunctionMap<Implementation, F>::call(implementation(), set, value);
	}
};

/*!
 * \brief Variant class for a variable as used in the *Objects base class of a store.
 *
 * Do not use. Only the *Objects class is allowed to use it.
 */
template <typename Store, typename Implementation, Type::type type_, size_t offset, size_t size_>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class StoreVariantV {
public:
#  if STORED_cplusplus >= 201103L
	StoreVariantV(StoreVariantV const&) = delete;
	StoreVariantV(StoreVariantV&&) = delete;
	void operator=(StoreVariantV const&) = delete;
	void operator=(StoreVariantV&&) = delete;
#  endif

	typedef Variant<Implementation> Variant_type;

	static constexpr uintptr_t key() noexcept
	{
		return static_cast<uintptr_t>(offset);
	}

	constexpr Variant_type variant() const noexcept
	{
		return objectToStore<Store>(*this)._variantv(type_, offset, size_);
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Variant_type() const noexcept
	{
		return variant();
	}

	size_t get(void* dst, size_t len = 0) const noexcept
	{
		return variant().get(dst, len);
	}

	template <typename T>
	T get() const noexcept
	{
		return variant().template get<T>();
	}

	size_t set(void const* src, size_t len = 0) noexcept
	{
		return variant().set(src, len);
	}

	template <typename T>
	void set(T value) noexcept
	{
		variant().template set<T>(value);
	}

	void set(char const* s) noexcept
	{
		variant().set(s);
	}

	static constexpr Type::type type() noexcept
	{
		return type_;
	}

	static constexpr size_t size() noexcept
	{
		return size_;
	}

	void* buffer() const noexcept
	{
		return variant().buffer();
	}

#  ifdef STORED_HAVE_QT
	QVariant toQVariant() const
	{
		return variant().toQVariant();
	}

	bool set(QVariant const& v)
	{
		return variant().set(v);
	}

	QString toQString() const
	{
		return variant().toQString();
	}

	void set(QString const& value)
	{
		variant().set(value);
	}
#  endif // STORED_HAVE_QT
};

/*!
 * \brief Variant class for a variable as used in the *Objects base class of a store.
 *
 * Do not use. Only the *Objects class is allowed to use it.
 */
template <typename Store, typename Implementation, Type::type type_, unsigned int F, size_t size_>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class StoreVariantF {
public:
#  if STORED_cplusplus >= 201103L
	StoreVariantF(StoreVariantF const&) = delete;
	StoreVariantF(StoreVariantF&&) = delete;
	void operator=(StoreVariantF const&) = delete;
	void operator=(StoreVariantF&&) = delete;
#  endif

	typedef Variant<Implementation> Variant_type;

	static constexpr unsigned int id() noexcept
	{
		return F;
	}

	constexpr Variant_type variant() const noexcept
	{
		return objectToStore<Store>(*this)._variantf(type_, F, size_);
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	constexpr operator Variant_type() const noexcept
	{
		return variant();
	}

	size_t get(void* dst, size_t len = 0) const
	{
		return variant().get(dst, len);
	}

	template <typename T>
	T get() const
	{
		return variant().template get<T>();
	}

	size_t set(void const* src, size_t len = 0)
	{
		return variant().set(src, len);
	}

	template <typename T>
	void set(T value)
	{
		variant().template set<T>(value);
	}

	void set(char const* s) noexcept
	{
		variant().set(s);
	}

	static constexpr Type::type type() noexcept
	{
		return type_;
	}

	static constexpr size_t size() noexcept
	{
		return size_;
	}

#  ifdef STORED_HAVE_QT
	QVariant toQVariant() const
	{
		return variant().toQVariant();
	}

	bool set(QVariant const& v)
	{
		return variant().set(v);
	}

	QString toQString() const
	{
		return variant().toQString();
	}

	void set(QString const& value)
	{
		variant().set(value);
	}
#  endif // STORED_HAVE_QT
};
} // namespace impl

} // namespace stored
#endif // __cplusplus
#endif // LIBSTORED_TYPES_H
