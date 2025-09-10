#ifndef LIBSTORED_DEBUGGER_H
#define LIBSTORED_DEBUGGER_H
// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#	include <libstored/macros.h>
#	include <libstored/compress.h>
#	include <libstored/protocol.h>
#	include <libstored/spm.h>
#	include <libstored/types.h>
#	include <libstored/util.h>

#	include <map>
#	include <memory>
#	include <new>
#	include <utility>
#	include <vector>

#	if STORED_cplusplus >= 201103L
#		include <functional>
#	endif

namespace stored {

template <bool Compress = Config::CompressStreams>
class Stream final : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Stream)
	STORED_CLASS_NEW_DELETE(Stream)
	friend class Stream<true>;

public:
	typedef ProtocolLayer base;

	Stream()
		: m_block()
	{
		if(Config::AvoidDynamicMemory)
			m_buffer.reserve(
				Config::DebuggerStreamBuffer
				+ Config::DebuggerStreamBufferOverflow);
	}

	~Stream() final is_default

	void decode(void* buffer, size_t len) final
	{
		STORED_UNUSED(buffer)
		STORED_UNUSED(len)
	}

	void encode(void const* buffer, size_t len, bool last = true) final
	{
		STORED_UNUSED(last)

		if(blocked())
			return;

#	ifdef STORED_HAVE_ZTH
		// With Zth, the encode context may be different from
		// Debugger's CmdTrace context.  As we pass a buffer pointer to
		// encode within CmdTrace, this buffer should not be changed
		// (realloc'ed) meanwhile. Set Config::AvoidDynamicMemory,
		// increase Config::DebuggerStreamBufferOverflow, or do proper
		// reserve() when this assert goes off.
		stored_assert(
			!Config::AvoidDynamicMemory
			|| m_buffer.size() + len <= m_buffer.capacity());
#	endif

		m_buffer.append(static_cast<char const*>(buffer), len);
	}

	using base::encode;

	size_t mtu() const final
	{
		return 0;
	}

	String::type const& buffer() const noexcept
	{
		return m_buffer;
	}

	void swap(String::type& other) noexcept
	{
		m_buffer.swap(other);
	}

	bool flush() final
	{
		block();
		return true;
	}

	void clear() noexcept
	{
		m_buffer.clear();
		unblock();
	}

	void drop(size_t cnt) noexcept
	{
		if(cnt < m_buffer.size())
			m_buffer.erase(0, cnt);
		else
			m_buffer.clear();
	}

	bool empty() const noexcept
	{
		return m_buffer.empty();
	}

	void block() noexcept
	{
		m_block = true;
	}

	void unblock() noexcept
	{
		m_block = false;
	}

	bool blocked() const noexcept
	{
		return m_block;
	}

	size_t fits(size_t more) const noexcept
	{
		size_t const default_max = Config::DebuggerStreamBuffer;
		size_t const real_max = default_max + Config::DebuggerStreamBufferOverflow;

		// Allow some data to be processed, even if the input is too large.
		more = std::min(more, real_max);

		size_t size = buffer().size();

		// Allow the data to get into the overflow area, but only if it was
		// below the normal max.
		if(size >= default_max)
			return 0;

		return std::min(more, real_max - size);
	}

private:
	String::type m_buffer;
	bool m_block;
};

template <>
class Stream<true> final : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Stream)
	STORED_CLASS_NEW_DELETE(Stream)
public:
	typedef ProtocolLayer base;

	Stream()
	{
		m_compress.wrap(*this);
		m_string.wrap(m_compress);
	}

	~Stream() final is_default

	void decode(void* buffer, size_t len) final
	{
		STORED_UNUSED(buffer)
		STORED_UNUSED(len)
	}

	void encode(void const* buffer, size_t len, bool last = true) final
	{
		STORED_UNUSED(last)

		if(blocked())
			return;

		m_compress.encode(buffer, len, false);
	}

	using base::encode;

	size_t mtu() const final
	{
		return 0;
	}

	bool flush() final
	{
		m_compress.encode();
		return m_compress.flush();
	}

	void clear() noexcept
	{
		m_string.clear();
	}

	void drop(size_t cnt) noexcept
	{
		m_string.drop(cnt);
	}

	bool empty() const noexcept
	{
		return
#	ifdef STORED_HAVE_HEATSHRINK
			m_compress.idle() &&
#	endif
			m_string.empty();
	}

	String::type const& buffer() const noexcept
	{
		return m_string.buffer();
	}

	void swap(String::type& other) noexcept
	{
		m_string.swap(other);
	}

	void block() noexcept
	{
		m_string.block();
	}

	void unblock() noexcept
	{
		m_string.unblock();
	}

	bool blocked() const noexcept
	{
		return m_string.blocked();
	}

	size_t fits(size_t more) const noexcept
	{
		// Use the overflow region only for unexpected compression output.
		size_t const default_max = Config::DebuggerStreamBuffer;
		more = std::min(more, default_max);
		size_t size = buffer().size();

		if(size >= default_max)
			return 0;

		return std::min(more, default_max - size);
	}

private:
	CompressLayer m_compress;
	Stream<false> m_string;
};

#	ifdef STORED_COMPILER_ARMCC
#		pragma clang diagnostic push
#		pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#	endif
/*!
 * \brief Container-template-type-invariant base class of a wrapper for #stored::Variant.
 */
// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
class DebugVariantBase {
	CLASS_NO_WEAK_VTABLE
public:
	/*!
	 * \brief Retrieve data from the object.
	 * \param dst the destination buffer
	 * \param len the size of \p dst. If 0, it defaults to #size().
	 * \return the number of bytes written into \p dst
	 */
	virtual size_t get(void* dst, size_t len = 0) const = 0;

	/*!
	 * \brief Set data to the buffer.
	 * \param src the data to be written
	 * \param len the length of \p src. If 0, it defaults to #size().
	 * \return the number of bytes consumed
	 */
	virtual size_t set(void const* src, size_t len = 0) = 0;

	/*!
	 * \brief The type of this object.
	 */
	virtual Type::type type() const = 0;

	/*!
	 * \brief The size of this object.
	 */
	virtual size_t size() const = 0;

	/*!
	 * \brief Returns if this wrapper points to a valid object.
	 */
	virtual bool valid() const = 0;

	/*!
	 * \brief Checks if the object is a function.
	 */
	bool isFunction() const
	{
		return valid() && Type::isFunction(type());
	}

	/*!
	 * \brief Checks if the object is a variable.
	 */
	bool isVariable() const
	{
		return valid() && !Type::isFunction(type());
	}

protected:
	/*!
	 * \brief Check if this and the given variant point to the same object.
	 */
	virtual bool operator==(DebugVariantBase const& rhs) const
	{
		return this == &rhs;
	}

	/*!
	 * \brief Check if this and the given variant do not point to the same object.
	 */
	bool operator!=(DebugVariantBase const& rhs) const
	{
		return !(*this == rhs);
	}

	/*!
	 * \brief Returns the container this object belongs to.
	 */
	virtual void* container() const = 0;

	// For operator==().
	template <typename Container>
	friend class DebugVariantTyped;
	// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
	friend class DebugVariant;
};

/*!
 * \brief Container-specific subclass of #stored::DebugVariantBase.
 *
 * This object is trivially copyable and assignable.
 * You probably don't want to use this object directly, use
 * #stored::DebugVariant instead.
 *
 * \see #stored::DebugVariant
 */
template <typename Container = void>
// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
class DebugVariantTyped : public DebugVariantBase {
public:
	/*!
	 * \brief Construct a wrapper around the given variant.
	 */
	explicit DebugVariantTyped(Variant<Container> const& variant)
		: m_variant(variant)
	{
#	if STORED_cplusplus >= 201103L
		static_assert(std::is_trivially_destructible<Variant<Container>>::value, "");
#	elif defined(__GCC__)
		static_assert(__has_trivial_destructor(Variant<Container>), "");
#	endif
	}

	/*!
	 * \brief Constructs an invalid wrapper.
	 */
	DebugVariantTyped() is_default

	size_t get(void* dst, size_t len = 0) const final
	{
		return variant().get(dst, len);
	}

	size_t set(void const* src, size_t len = 0) final
	{
		return variant().set(src, len);
	}

	Type::type type() const final
	{
		return variant().type();
	}

	size_t size() const final
	{
		return variant().size();
	}

	bool valid() const final
	{
		return variant().valid();
	}

	/*! \brief Returns the variant this object is a wrapper of. */
	Variant<Container> const& variant() const
	{
		return m_variant;
	}

	/*! \copydoc variant() const */
	Variant<Container>& variant()
	{
		return m_variant;
	}

protected:
	bool operator==(DebugVariantBase const& rhs) const final
	{
		if(valid() != rhs.valid())
			return false;
		if(!valid())
			return true;
		if(container() != rhs.container())
			return false;
		return variant() == static_cast<DebugVariantTyped<Container> const&>(rhs).variant();
	}

	void* container() const final
	{
		return variant().valid() ? &variant().container() : nullptr;
	}

private:
	/*! \brief The wrapped variant. */
	Variant<Container> m_variant;
};

/*!
 * \brief A wrapper for any type of object in a store.
 *
 * This is a template-type-independent container for a #stored::DebugVariantTyped.
 *
 * Even though the \c DebugVariantTyped uses virtual functions,
 * inheritance, and templates, the allocation is done within this object.
 * This object is small, efficient, default-copyable and
 * default-assignable, and can therefore be used as a value in a standard
 * container.
 */
// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
class DebugVariant final : public DebugVariantBase {
	CLASS_NO_WEAK_VTABLE
public:
	typedef DebugVariantBase base;

	/*!
	 * \brief Constructor for an invalid #stored::Variant wrapper.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	DebugVariant()
	{
		new(m_buffer) DebugVariantTyped<>();
	}

	/*!
	 * \brief Constructor for a #stored::Variant wrapper.
	 */
	template <typename Container>
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	explicit DebugVariant(Variant<Container> const& variant)
	{
		// Check if the cast of variant() is valid.
		static_assert(
			sizeof(DebugVariantTyped<Container>) == sizeof(DebugVariantTyped<>), "");

		// Check if our default copy constructor works properly.
#	if STORED_cplusplus >= 201103L
		static_assert(std::is_trivially_copyable<Variant<Container>>::value, "");
		static_assert(std::is_trivially_destructible<Variant<Container>>::value, "");
#	elif defined(__GCC__)
		static_assert(__has_trivial_copy(Variant<Container>), "");
		static_assert(__has_trivial_destructor(Variant<Container>), "");
#	endif

		new(m_buffer) DebugVariantTyped<Container>(variant);
		// Check if the cast of variant() works properly.
		// cppcheck-suppress assertWithSideEffect
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		stored_assert(
			static_cast<DebugVariantBase*>(
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				reinterpret_cast<DebugVariantTyped<Container>*>(m_buffer))
			== &this->variant());
	}

	size_t get(void* dst, size_t len = 0) const final
	{
		return variant().get(dst, len);
	}

	size_t set(void const* src, size_t len = 0) final
	{
		return variant().set(src, len);
	}

	Type::type type() const final
	{
		return variant().type();
	}

	size_t size() const final
	{
		return variant().size();
	}

	bool valid() const final
	{
		return variant().valid();
	}

	bool operator==(DebugVariant const& rhs) const
	{
		return variant() == rhs.variant();
	}

	bool operator!=(DebugVariant const& rhs) const
	{
		return !(*this == rhs);
	}

protected:
	using base::operator==;

	/*!
	 * \brief Returns the contained #stored::DebugVariantTyped instance.
	 */
	DebugVariantBase const& variant() const
	{
		return *static_cast<DebugVariantBase const*>(
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			reinterpret_cast<DebugVariantTyped<> const*>(m_buffer));
	}

	/*!
	 * \copydoc variant() const
	 */
	DebugVariantBase& variant()
	{
		return *static_cast<DebugVariantBase*>(
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			reinterpret_cast<DebugVariantTyped<>*>(m_buffer));
	}

	void* container() const final
	{
		return variant().container();
	}

private:
	/*!
	 * \brief The buffer to create the contained #stored::DebugVariantTyped into.
	 *
	 * The destructor is never called for this object, and it must be
	 * trivially copyable and assignable. Moreover, all DebugVariantTyped
	 * instances must have equal size. These properties are checked in our
	 * the constructor.
	 */
	// flawfinder: ignore
	char m_buffer[sizeof(DebugVariantTyped<>)];
};
#	ifdef STORED_COMPILER_ARMCC
#		pragma clang diagnostic pop
#	endif

/*!
 * \brief Wrapper for a store, that hides the store's template parameters.
 *
 * This is a type-independent base class of #stored::DebugStore.
 * For #stored::Debugger, this is the interface to access a specific store.
 *
 * \see #stored::DebugStore
 */
class DebugStoreBase {
	STORED_CLASS_NOCOPY(DebugStoreBase)
	STORED_CLASS_NEW_DELETE(DebugStoreBase)
	CLASS_NO_WEAK_VTABLE
protected:
	/*! \brief Constructor. */
	DebugStoreBase() is_default

public:
	/*! \brief Destructor. */
	virtual ~DebugStoreBase() is_default

	/*!
	 * \brief Returns the name of this store.
	 * \return the name, which cannot be \c nullptr
	 */
	virtual char const* name() const noexcept = 0;

	/*!
	 * \brief Performs a lookup of the given object name.
	 * \param name the name to find, which can be abbreviated until unambiguous
	 * \param len the maximum length of \p name to check
	 * \return a #stored::DebugVariant, which is invalid when the object was not found.
	 */
	virtual DebugVariant
	find(char const* name, size_t len = std::numeric_limits<size_t>::max()) noexcept = 0;

	/*!
	 * \brief Callback function prototype as supplied to \c list().
	 *
	 * It receives the name of the object, the corresponding
	 * #stored::DebugVariant of the object, and the \c arg value, as passed
	 * to \c list().
	 *
	 * \see #stored::DebugStoreBase::list(ListCallbackArg*, void*, char const*) const
	 */
#	ifdef DOXYGEN
	// breathe breaks on the function typedef.
	using ListCallbackArg = void(char const*, DebugVariant&, void*);
#	else
	typedef void(ListCallbackArg)(char const*, DebugVariant&, void*);
#	endif

	/*!
	 * \brief Iterates over the directory and invoke a callback for every object.
	 * \param f the callback to invoke
	 * \param arg an arbitrary argument to be passed to \p f
	 * \param prefix a prefix applied to the name passed to \p f
	 */
	virtual void
	list(ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr) const = 0;
};

/*!
 * \brief A type-specific wrapper for a store to be used by #stored::Debugger.
 */
template <typename Store>
class DebugStore : public DebugStoreBase {
	STORED_CLASS_NOCOPY(DebugStore)
	STORED_CLASS_NEW_DELETE(DebugStore)
public:
	/*!
	 * \brief Constuctor.
	 * \param store the store to be wrapped
	 */
	explicit DebugStore(Store& store) noexcept
		: m_store(store)
	{}

	/*!
	 * \brief Destructor.
	 */
	virtual ~DebugStore() override is_default

	char const* name() const noexcept final
	{
		return store().name();
	}

	DebugVariant
	find(char const* name, size_t len = std::numeric_limits<size_t>::max()) noexcept final
	{
		return DebugVariant(store().find(name, len));
	}

private:
	/*!
	 * \brief Helper class to pass arguments to #listCallback().
	 */
	struct ListCallbackArgs {
		ListCallbackArg* f;
		void* arg;
	};

	/*!
	 * \brief Callback wrapper for #list().
	 *
	 * This function is used by \c list(), which wraps the given object
	 * properties in a #stored::DebugVariant and forward it to the callback
	 * that was supplied to \c list().
	 */
	static void listCallback(
		void* container, char const* name, Type::type type, void* buffer, size_t len,
		void* arg)
	{
		DebugVariant variant(
			Type::isFunction(type)
				? Variant<typename Store::Implementation>(
					*static_cast<typename Store::Implementation*>(container),
					type, (unsigned int)(uintptr_t)buffer, len) // NOLINT
				: Variant<typename Store::Implementation>(
					*static_cast<typename Store::Implementation*>(container),
					type, buffer, len));
		ListCallbackArgs& args = *static_cast<ListCallbackArgs*>(arg);
		(*args.f)(name, variant, args.arg);
	}

public:
	virtual void
	list(DebugStoreBase::ListCallbackArg* f, void* arg = nullptr,
	     char const* prefix = nullptr) const override
	{
		ListCallbackArgs args = {f, arg};
		store().list(&listCallback, &args, prefix);
	}

	/*!
	 * \brief Returns the store that is wrapped.
	 */
	typename Store::Implementation& store() const noexcept
	{
		return m_store.implementation();
	}

private:
	/*! \brief The wrapped store. */
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
	Store& m_store;
};

/*!
 * \brief The application-layer implementation of the Embedded %Debugger protocol.
 *
 * To use the Debugger in your application:
 *
 * - Instantiate Debugger.
 * - #map() your store into the Debugger instance.
 * - Wrap this instance in any #stored::ProtocolLayer that is required for your device.
 *
 * By default, the Debugger provides the standard set of commands.  To
 * extend it, create a subclass and override #capabilities() and
 * #process().
 */
class Debugger : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Debugger)
public:
	explicit Debugger(char const* identification = nullptr, char const* versions = nullptr);
	virtual ~Debugger() noexcept override;

public:
	////////////////////////////
	// Store mapping

	/*!
	 * Helper class to sort #StoreMap based on name.
	 */
	struct StorePrefixComparator {
		bool operator()(char const* lhs, char const* rhs) const noexcept
		{
			stored_assert(lhs && rhs);
			return strcmp(lhs, rhs) < 0;
		}
	};

	/*!
	 * \brief The map to used by #map(), which maps names to stores.
	 */
	typedef Map<char const*, DebugStoreBase*, StorePrefixComparator>::type StoreMap;

	/*!
	 * \brief Register a store to this Debugger.
	 *
	 * If there is only one store registered, all objects of that store
	 * are accessible using the names as defined by the store.
	 * If multiple stores are mapped, all objects are prefixed using either
	 * the prefix supplied to #map(), or using the store's name when \p name is \c nullptr.
	 */
	template <typename Store>
	void map(Store& store, char const* name = nullptr)
	{
		map(new(allocate<DebugStore<Store> /**/>()) DebugStore<Store>(store), name);
	}

	void unmap(char const* name);
	StoreMap const& stores() const;

protected:
	void map(DebugStoreBase* store, char const* name);

private:
	/*! \brief The store map. */
	StoreMap m_map;



public:
	////////////////////////////
	// Variable access

	DebugVariant find(char const* name, size_t len = std::numeric_limits<size_t>::max()) const;

	/*!
	 * \typedef ListCallbackArg
	 * \copydoc stored::DebugStoreBase::ListCallbackArg
	 */
	typedef DebugStoreBase::ListCallbackArg ListCallbackArg;
	void list(ListCallbackArg* f, void* arg = nullptr) const;

#	if STORED_cplusplus >= 201103L
	/*!
	 * \brief Callback function prototype as supplied to \c list().
	 *
	 * It receives the name of the object, and the corresponding
	 * #stored::DebugVariant of the object.
	 *
	 * \see #list<F>(F&&) const
	 */
	using ListCallback = void(char const*, DebugVariant&);

	/*!
	 * \brief Iterates over the directory and invoke a callback for every object.
	 * \param f the callback to invoke, which can be any type, but must look like #ListCallback
	 */
	template <typename F>
	SFINAE_IS_FUNCTION(F, ListCallback, void)
	// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
	list(F&& f) const
	{
		// cppcheck-suppress constParameter
		auto cb = [](char const* name, DebugVariant& variant, void* f_) {
			(*static_cast<typename std::decay<F>::type*>(f_))(name, variant);
		};
		list(static_cast<ListCallbackArg*>(cb), &f);
	}
#	endif
private:
	static void listCmdCallback(char const* name, DebugVariant& variant, void* arg);



public:
	////////////////////////////
	// Protocol

	static char const CmdCapabilities = '?';
	static char const CmdRead = 'r';
	static char const CmdWrite = 'w';
	static char const CmdEcho = 'e';
	static char const CmdList = 'l';
	static char const CmdAlias = 'a';
	static char const CmdMacro = 'm';
	static char const CmdIdentification = 'i';
	static char const CmdVersion = 'v';
	static char const CmdReadMem = 'R';
	static char const CmdWriteMem = 'W';
	static char const CmdStream = 's';
	static char const CmdTrace = 't';
	static char const CmdFlush = 'f';
	static char const Ack = '!';
	static char const Nack = '?';

public:
	virtual void capabilities(char*& caps, size_t& len, size_t reserve = 0);
	virtual char const* identification();
	void setIdentification(char const* identification = nullptr);
	virtual bool version(ProtocolLayer& response);
	void setVersions(char const* versions = nullptr);

	size_t stream(char s, char const* data);
	size_t stream(char s, char const* data, size_t len);
	Stream<>* stream(char s, bool alloc = false);
	Stream<> const* stream(char s) const;
	char const* streams(void const*& buffer, size_t& len);

	void trace();
	bool tracing() const;

	virtual void process(void const* frame, size_t len, ProtocolLayer& response);
	virtual void decode(void* buffer, size_t len) override;

protected:
	ScratchPad<>& spm() const;

	/*! \brief Type of alias map. */
	typedef Map<char, DebugVariant>::type AliasMap;
	AliasMap& aliases();
	AliasMap const& aliases() const;

	/*! \brief Type of macro map. */
	typedef Map<char, String::type>::type MacroMap;
	MacroMap& macros();
	MacroMap const& macros() const;
	virtual bool runMacro(char m, ProtocolLayer& response);

	/*!
	 * \brief Encode a given value to ASCII hex.
	 * \see #encodeHex(stored::Type::type, void*&, size_t&, bool)
	 */
	template <typename T, typename B>
	size_t encodeHex(T value, B*& buf, bool shortest = true)
	{
		void* data = (void*)&value;
		size_t len = sizeof(T);
		encodeHex(toType<T>::type, data, len, shortest);
		buf = (B*)data;
		return len;
	}
	void encodeHex(Type::type type, void*& data, size_t& len, bool shortest = true);
	bool decodeHex(Type::type type, void const*& data, size_t& len);

private:
	/*! \brief A scratch pad memory for any Debugger operation. */
	mutable ScratchPad<> m_scratchpad;

	/*! \brief The identification. */
	char const* m_identification;
	/*! \brief The versions. */
	char const* m_versions;

	/*! \brief The aliases. */
	AliasMap m_aliases;

	/*! \brief The macros. */
	MacroMap m_macros;
	/*! \brief Total size of macro definitions. */
	size_t m_macroSize;

	/*! \brief The streams map type. */
	typedef Map<char, Stream<>*>::type StreamMap;
	/*! \brief The streams. */
	StreamMap m_streams;

	/*! \brief Macro of #trace(). */
	char m_traceMacro;
	/*! \brief Stream of #trace(). */
	char m_traceStream;
	/*! \brief Decimate setting of #trace(). 0 disables tracing. */
	unsigned int m_traceDecimate;
	/*! \brief Decimate counter of #trace(). */
	unsigned int m_traceCount;
};

} // namespace stored
#endif // __cplusplus
#endif // LIBSTORED_DEBUGGER_H
