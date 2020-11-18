#ifndef __LIBSTORED_DEBUGGER_H
#define __LIBSTORED_DEBUGGER_H
// vim:fileencoding=utf-8
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

/*!
 * \defgroup libstored_debugger debugger
 * \brief Embedded Debugger message handling.
 *
 * The default set of commands that is processed by #stored::Debugger is listed below.
 * A subclass of %Debugger may extend the set of capabilities, for application-specific
 * purposes.
 *
 * The protocol is a request-response mechanism; for every request, there must be a response.
 * Requests are processed in order.
 * This is the OSI application layer of the protocol stack.
 * For other layers, see \ref libstored_protocol.
 *
 * Requests always start with an ASCII character, which is the command.  The
 * response can be actual data, or with ack `!` or nack `?`.  All
 * requests and responses are usually plain ASCII (or UTF-8 for strings), to
 * simplify processing it over a terminal or by humans. However, it does not
 * have to be the case.
 *
 * ### Capabilities
 *
 * Request: `?`
 *
 * 	?
 *
 * Response: a list of command characters.
 *
 * 	?rwe
 *
 * This command is mandatory for every debugging target.
 *
 * ### Echo
 *
 * Request: `e` \<any data\>
 *
 * 	eHello World
 *
 * Response: \<the same data\>
 *
 * 	Hello World
 *
 * ### Read
 *
 * Request: `r` \<name of object\>
 *
 * (Every scope within) the name of the object may be abbreviated, as long as it is unambiguous.
 * In case there is a alias created for the object (see Alias below), the alias
 * character can be used instead of the name.
 *
 * 	r/bla/asdf
 *
 * Response: `?` | \<ASCII hex value of object\>
 *
 * For values with fixed length (int, float), the byte order is big/network
 * endian.  For ints, the initial zeros can be omitted. For other data, all bytes
 * are encoded.
 *
 * 	123abc
 *
 * ### Write
 *
 * Request: `w` \<value in ASCII hex\> \<name of object\>
 *
 * See Read for details about the hex value and object name.
 *
 * 	w10/b/a
 *
 * Response: `!` | `?`
 *
 * 	!
 *
 * ### List
 *
 * Requests a full list of all objects of all registered stores to the current Embedded Debugger.
 *
 * Request: `l`
 *
 * 	l
 *
 * Response: ( \<type byte in hex\> \<length in hex\> \<name of object\> `\n` ) * | `?`
 *
 * 	3b4/b/i8
 * 	201/b/b
 *
 * See #stored::Type for the type byte. The type byte is always two hex
 * characters.  The number of characters of the length of the object depends on
 * the value; leading zeros may be removed.
 *
 * ### Alias
 *
 * Assigns a character to a object path.  An alias can be everywhere where an
 * object path is expected.  Creating aliases skips parsing the object path
 * repeatedly, so makes debugging more efficient.  If no object is specified, the
 * alias is removed.  The number of aliases may be limited. If the limit is hit,
 * the response will be `?`.
 * The alias name can be any char in the range 0x20 (`␣`) - 0x7e (`~`), except for 0x2f (`/`).
 *
 * Request: `a` \<char\> ( \<name of object\> ) ?
 *
 * 	a0/bla/a
 *
 * Response: `!` | `?`
 *
 * 	!
 *
 * ### Macro
 *
 * Saves a sequence of commands and assigns a name to it.  The macro name can be
 * any char in the range 0x20 (`␣`) - 0x7e (`~`).  In case of a name clash with an
 * existing non-macro command, the command is executed; the macro cannot hide or
 * replace the command.
 *
 * The separator can be any char, as long as it is not used
 * within a command of the macro definition. Using `\r`, `\n`, or `\t` is
 * usually safe, as it cannot occur inside a name.
 *
 * Without the definition after the macro name, the macro is removed. The
 * system may be limited in total definition length. The macro string is
 * reinterpreted every time it is invoked.
 *
 * The responses of the commands are merged into one response frame, without
 * separators. The Echo command can be used to inject separators in the output.
 *
 * Request: `m` \<char\> ( \<separator\> \<command\> ) *
 *
 * 	mZ r/bla/a e; r/bla/z
 *
 * Response: `!` | `?`
 *
 * 	!
 *
 * If the `Z` command is now executed, the result could be something like:
 *
 * 	123;456
 *
 * ### Identification
 *
 * Returns a fixed string that identifies the application.
 *
 * Request: `i`
 *
 * 	i
 *
 * Response: `?` | \<UTF-8 encoded application name\>
 *
 * 	libstored
 *
 * ### Version
 *
 * Returns a list of versions.
 *
 * Request: `v`
 *
 * 	v
 *
 * Response: `?` | \<protocol version\> ( `␣` \<application-specific version\> ) *
 *
 * 	2 r243+trunk beta
 *
 * ### Read memory
 *
 * Read a memory via a pointer instead of the store.  Returns the number of
 * requested bytes. If no length is specified, a word is returned.
 *
 * Request: `R` \<pointer in hex\> ( `␣` \<length\> ) ?
 *
 * 	R1ffefff7cc 4
 *
 * Response: `?` | \<bytes in hex\>
 *
 * 	efbe0000
 *
 * Bytes are just concatenated as they occur in memory, having the byte at the
 * lowest address first.
 *
 * ### Write memory
 *
 * Write a memory via a pointer instead of the store.
 *
 * Request: `W` \<pointer in hex\> `␣` \<bytes in hex\>
 *
 * 	W1ffefff7cc 0123
 *
 * Response: `?` | `!`
 *
 * 	!
 *
 * ### Streams
 *
 * Read all available data from a stream. Streams are application-defined
 * sequences of bytes, like stdout and stderr. They may contain binary data.
 * There are an arbitrary number of streams, with an arbitrary single-char name,
 * except for `?`, as it makes the response ambiguous.
 *
 * To list all streams with data:
 *
 * Request: `s`
 *
 * To request all data from a stream, where the optional suffix is appended to the response:
 *
 * Request: `s` \<char\> \<suffix\> ?
 *
 * 	sA/
 *
 * Response: `?` | \<data\> \<suffix\>
 *
 * 	Hello World!!1/
 *
 * Once data has been read from the stream, it is removed. The next call will
 * return new data.  If a stream was never used, `?` is returned. If it was used,
 * but it is empty now, the stream char does not show up in the `s` call, but does
 * respond with the suffix. If no suffix was provided, and there is no data, the
 * response is empty.
 *
 * The number of streams and the maximum buffer size of a stream may be limited.
 *
 * Depending on stored::Config::CompressStreams, the data returned by `s`
 * is compressed using heatshrink (window=8, lookahead=4). Every chunk of data
 * returned by `s` is part of a single stream, and must be decompressed as
 * such. As (de)compression is stateful, all data from the start of the stream is
 * required for decompression. Moreover, stream data may remain in the compressor
 * before retrievable via `s`.
 *
 * To detect if the stream is compressed, and to forcibly flush out and reset
 * the compressed stream, use the Flush (`f`) command. A flush will terminate
 * the current stream, push out the last bit of data from the compressor's buffers
 * and restart the compressor's state. So, a normal startup sequence of a
 * debug client would be:
 *
 * - Check if `f` capability exists. If not, done; no compression is used on streams.
 * - Flush out all streams: execute `f`.
 * - Drop all streams, as the start of the stream is possibly missing: execute `sx`
 *   for every stream returned by `s`.
 *
 * Afterwards, pass all data received from `s` through the heatshrink decoder.
 *
 * ### Flush
 *
 * Flush out and reset a stream (see also Streams). When this capability does not
 * exist, streams are not compressed. If it does exist, all streams are compressed.
 * Use this function to initialize the stream if the (de)compressing state is unknown,
 * or to force out the last data (for example, the last trace data).
 *
 * Request: `f` \<char\> ?
 *
 * Response: `!`
 *
 * The optional char is the stream name. If omitted, all streams are flushed and reset.
 * The response is always `!`, regardless of whether the stream existed or had data.
 *
 * The stream is blocked until it is read out by `s`. This way, the last data
 * is not lost, but new data could be dropped if this takes too long. If you want
 * an atomic flush-retrieve, use a macro.
 *
 * ### Tracing
 *
 * Executes a macro every time the application invokes stored::Debugger::trace().
 * A stream is filled with the macro output.
 *
 * Request: `t` ( \<macro\> \<stream\> ( \<decimate in hex\> ) ? ) ?
 *
 * 	tms64
 *
 * This executes macro output of `m` to the stream `s`, but only one in every
 * 100 calls to trace().  If the output does not fit in the stream buffer, it
 * is silently dropped.
 *
 * `t` without arguments disables tracing. If the decimate argument is omitted,
 * 1 is assumed (no decimate).  Only one tracing configuration is supported;
 * another `t` command with arguments overwrites the previous configuration.
 *
 * Response: `?` | `!`
 *
 * 	!
 *
 * The buffer collects samples over time, which is read out by the client possibly
 * at irregular intervals.  Therefore, you probably want to know the time stamp of
 * the sample. For this, include reading the time in the macro definition. By
 * convention, the time is a top-level variable `t` with the unit between braces.
 * It is implementation-defined what the offset is of `t`, which can be since the
 * epoch or since the last boot, for example.  For example, your store can have
 * one of the following time variables:
 *
 * 	// Nice resolution, wraps around after 500 millennia.
 * 	(uint64) t (us)
 * 	// Typical ARM systick counter, wraps around after 49 days.
 * 	(uint32) t (ms)
 * 	// Pythonic time. Watch out with significant bits.
 * 	(double) t (s)
 *
 * The time is usually a function type, as it is read-only and reading it should
 * invoke some time-keeping functions.
 *
 * Macro executions are just concatenated in the stream buffer.
 * Make sure to use the Echo command to inject proper separators to allow parsing
 * the stream content afterwards.
 *
 * For example, the following requests are typical to setup tracing:
 *
 * 	# Define aliases to speed up macro processing.
 * 	at/t (us)
 * 	a1/some variable
 * 	a2/some other variable
 * 	# Save the initial start time offset to relate it to our wall clock.
 * 	rt
 * 	# Define a macro for tracing
 * 	mM rt e, r1 e, r2 e;
 * 	# Setup tracing
 * 	tMT
 * 	# Repeatedly read trace buffer
 * 	sT
 * 	sT
 * 	sT
 * 	...
 *
 * Now, the returned stream buffer (after decompressing) contains triplets of
 * time, /some variable, /some other variable, like this:
 *
 * 	101,1,2;102,1,2;103,1,2;
 *
 * Depending on the buffer size, reading the buffer may be orders of magnitude slower
 * than the actual tracing speed.
 *
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/types.h>
#include <libstored/util.h>
#include <libstored/spm.h>
#include <libstored/protocol.h>
#include <libstored/compress.h>

#include <new>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <memory>

#if STORED_cplusplus >= 201103L
#  include <functional>
#endif

namespace stored {

	template <bool Compress = Config::CompressStreams>
	class Stream : public ProtocolLayer {
		CLASS_NOCOPY(Stream)
		friend class Stream<true>;
	public:
		typedef ProtocolLayer base;

		Stream() : m_block() {}
		~Stream() final is_default

		void decode(void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len)) final {}

		void encode(void const* buffer, size_t len, bool UNUSED_PAR(last) = true) final {
			if(blocked())
				return;

			m_buffer.append(static_cast<char const*>(buffer), len);
		}

		using base::encode;

		size_t mtu() const final {
			return 0;
		}

		std::string const& buffer() const {
			return m_buffer;
		}

		bool flush() final {
			block();
			return true;
		}

		void clear() {
			m_buffer.clear();
			unblock();
		}

		bool empty() const {
			return m_buffer.empty();
		}

		void block() {
			m_block = true;
		}

		void unblock() {
			m_block = false;
		}

		bool blocked() const {
			return m_block;
		}

	private:
		std::string m_buffer;
		bool m_block;
	};

	template<>
	class Stream<true> : public ProtocolLayer {
		CLASS_NOCOPY(Stream)
	public:
		typedef ProtocolLayer base;

		Stream()
		{
			m_compress.wrap(*this);
			m_string.wrap(m_compress);
		}

		~Stream() final is_default

		void decode(void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len)) final {}

		void encode(void const* buffer, size_t len, bool UNUSED_PAR(last) = true) final {
			if(blocked())
				return;

			m_compress.encode(buffer, len, false);
		}

		using base::encode;

		size_t mtu() const final {
			return 0;
		}

		bool flush() final {
			m_compress.encode();
			return m_compress.flush();
		}

		void clear() {
			m_string.clear();
		}

		bool empty() const {
			return
#ifdef STORED_HAVE_HEATSHRINK
				m_compress.idle() &&
#endif
				m_string.empty();
		}

		std::string const& buffer() const {
			return m_string.buffer();
		}

		void block() {
			m_string.block();
		}

		void unblock() {
			m_string.unblock();
		}

		bool blocked() const {
			return m_string.blocked();
		}

	private:
		CompressLayer m_compress;
		Stream<false> m_string;
	};

#ifdef STORED_COMPILER_ARMCC
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif
	/*!
	 * \brief Container-template-type-invariant base class of a wrapper for #stored::Variant.
	 */
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
		bool isFunction() const { return valid() && Type::isFunction(type()); }

		/*!
		 * \brief Checks if the object is a variable.
		 */
		bool isVariable() const { return valid() && !Type::isFunction(type()); }

	protected:
		/*!
		 * \brief Check if this and the given variant point to the same object.
		 */
		virtual bool operator==(DebugVariantBase const& rhs) const { return this == &rhs; }

		/*!
		 * \brief Check if this and the given variant do not point to the same object.
		 */
		bool operator!=(DebugVariantBase const& rhs) const { return !(*this == rhs); }

		/*!
		 * \brief Returns the container this object belongs to.
		 */
		virtual void* container() const = 0;

		// For operator==().
		template <typename Container> friend class DebugVariantTyped;
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
	class DebugVariantTyped : public DebugVariantBase {
	public:
		/*!
		 * \brief Construct a wrapper around the given variant.
		 */
		explicit DebugVariantTyped(Variant<Container> const& variant)
			: m_variant(variant)
		{
#if STORED_cplusplus >= 201103L
			static_assert(std::is_trivially_destructible<Variant<Container>>::value, "");
#elif defined(__GCC__)
			static_assert(__has_trivial_destructor(Variant<Container>), "");
#endif
		}

		/*!
		 * \brief Constructs an invalid wrapper.
		 */
		DebugVariantTyped() is_default

		size_t get(void* dst, size_t len = 0) const final {
			return variant().get(dst, len); }
		size_t set(void const* src, size_t len = 0) final {
			return variant().set(src, len); }
		Type::type type() const final {
			return variant().type(); }
		size_t size() const final {
			return variant().size(); }
		bool valid() const final {
			return variant().valid(); }

		/*! \brief Returns the variant this object is a wrapper of. */
		Variant<Container> const& variant() const { return m_variant; }
		/*! \copydoc variant() const */
		Variant<Container>& variant() { return m_variant; }

	protected:
		bool operator==(DebugVariantBase const& rhs) const final {
			if(valid() != rhs.valid())
				return false;
			if(!valid())
				return true;
			if(container() != rhs.container())
				return false;
			return variant() == static_cast<DebugVariantTyped<Container> const&>(rhs).variant();
		}

		void* container() const final {
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
	 *
	 * \ingroup libstored_debugger
	 */
	class DebugVariant final : public DebugVariantBase {
		CLASS_NO_WEAK_VTABLE
	public:
		typedef DebugVariantBase base;

		/*!
		 * \brief Constructor for an invalid #stored::Variant wrapper.
		 */
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		DebugVariant() {
			new(m_buffer) DebugVariantTyped<>();
		}

		/*!
		 * \brief Constructor for a #stored::Variant wrapper.
		 */
		template <typename Container>
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		explicit DebugVariant(Variant<Container> const& variant) {
			// Check if the cast of variant() is valid.
			static_assert(sizeof(DebugVariantTyped<Container>) == sizeof(DebugVariantTyped<>), "");

			// Check if our default copy constructor works properly.
#if STORED_cplusplus >= 201103L
			static_assert(std::is_trivially_copyable<Variant<Container>>::value, "");
			static_assert(std::is_trivially_destructible<Variant<Container>>::value, "");
#elif defined(__GCC__)
			static_assert(__has_trivial_copy(Variant<Container>), "");
			static_assert(__has_trivial_destructor(Variant<Container>), "");
#endif

			new(m_buffer) DebugVariantTyped<Container>(variant);
			// Check if the cast of variant() works properly.
			// cppcheck-suppress assertWithSideEffect
			stored_assert(static_cast<DebugVariantBase*>(reinterpret_cast<DebugVariantTyped<Container>*>(m_buffer)) == &this->variant()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
		}

		size_t get(void* dst, size_t len = 0) const final {
			return variant().get(dst, len); }
		size_t set(void const* src, size_t len = 0) final {
			return variant().set(src, len); }
		Type::type type() const final {
			return variant().type(); }
		size_t size() const final {
			return variant().size(); }
		bool valid() const final {
			return variant().valid(); }
		bool operator==(DebugVariant const& rhs) const {
			return variant() == rhs.variant(); }
		bool operator!=(DebugVariant const& rhs) const {
			return !(*this == rhs); }

	protected:
		using base::operator==;

		/*!
		 * \brief Returns the contained #stored::DebugVariantTyped instance.
		 */
		DebugVariantBase const& variant() const {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			return *static_cast<DebugVariantBase const*>(reinterpret_cast<DebugVariantTyped<> const*>(m_buffer));
		}

		/*!
		 * \copydoc variant() const
		 */
		DebugVariantBase& variant() {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			return *static_cast<DebugVariantBase*>(reinterpret_cast<DebugVariantTyped<>*>(m_buffer));
		}

		void* container() const final {
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
		char m_buffer[sizeof(DebugVariantTyped<>)];
	};
#ifdef STORED_COMPILER_ARMCC
#  pragma clang diagnostic pop
#endif

	/*!
	 * \brief Wrapper for a store, that hides the store's template parameters.
	 *
	 * This is a type-independent base class of #stored::DebugStore.
	 * For #stored::Debugger, this is the interface to access a specific store.
	 *
	 * \see #stored::DebugStore
	 * \ingroup libstored_debugger
	 */
	class DebugStoreBase {
		CLASS_NOCOPY(DebugStoreBase)
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
		virtual char const* name() const = 0;

		/*!
		 * \brief Performs a lookup of the given object name.
		 * \param name the name to find, which can be abbreviated until unambiguous
		 * \param len the maximum length of \p name to check
		 * \return a #stored::DebugVariant, which is invalid when the object was not found.
		 */
		virtual DebugVariant find(char const* name, size_t len = std::numeric_limits<size_t>::max()) = 0;

		/*!
		 * \brief Callback function prototype as supplied to \c list().
		 *
		 * It receives the name of the object, the corresponding
		 * #stored::DebugVariant of the object, and the \c arg value, as passed
		 * to \c list().
		 *
		 * \see #stored::DebugStoreBase::list(ListCallbackArg*, void*, char const*) const
		 */
		typedef void(ListCallbackArg)(char const*, DebugVariant&, void*);

		/*!
		 * \brief Iterates over the directory and invoke a callback for every object.
		 * \param f the callback to invoke
		 * \param arg an arbitrary argument to be passed to \p f
		 * \param prefix a prefix applied to the name passed to \p f
		 */
		virtual void list(ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr) const = 0;
	};

	/*!
	 * \brief A type-specific wrapper for a store to be used by #stored::Debugger.
	 */
	template <typename Store>
	class DebugStore : public DebugStoreBase {
		CLASS_NOCOPY(DebugStore)
	public:
		/*!
		 * \brief Constuctor.
		 * \param store the store to be wrapped
		 */
		explicit DebugStore(Store& store)
			: m_store(store)
		{}

		/*!
		 * \brief Destructor.
		 */
		virtual ~DebugStore() override is_default

		char const* name() const final { return store().name(); }

		DebugVariant find(char const* name, size_t len = std::numeric_limits<size_t>::max()) final {
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
		static void listCallback(void* container, char const* name, Type::type type, void* buffer, size_t len, void* arg) {
			DebugVariant variant(
				Type::isFunction(type) ?
					// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
					Variant<typename Store::Implementation>(*(typename Store::Implementation*)container, type, (unsigned int)(uintptr_t)buffer, len) :
					Variant<typename Store::Implementation>(*(typename Store::Implementation*)container, type, buffer, len));
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			ListCallbackArgs& args = *reinterpret_cast<ListCallbackArgs*>(arg);
			(*args.f)(name, variant, args.arg);
		}

	public:
		virtual void list(DebugStoreBase::ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr) const override {
			ListCallbackArgs args = {f, arg};
			store().list(&listCallback, &args, prefix);
		}

		/*!
		 * \brief Returns the store that is wrapped.
		 */
		typename Store::Implementation& store() const { return m_store.implementation(); }

	private:
		/*! \brief The wrapped store. */
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
	 *
	 * \ingroup libstored_debugger
	 */
	class Debugger : public ProtocolLayer {
		CLASS_NOCOPY(Debugger)
	public:
		explicit Debugger(char const* identification = nullptr, char const* versions = nullptr);
		virtual ~Debugger() override;

	public:
		////////////////////////////
		// Store mapping

		/*!
		 * Helper class to sort #StoreMap based on name.
		 */
		struct StorePrefixComparator {
			bool operator()(char const* lhs, char const* rhs) const {
				stored_assert(lhs && rhs);
				return strcmp(lhs, rhs) < 0;
			}
		};

		/*!
		 * \brief The map to used by #map(), which maps names to stores.
		 */
		typedef std::map<char const*,
#if STORED_cplusplus >= 201103L
			std::unique_ptr<DebugStoreBase>
#else
			DebugStoreBase*
#endif
			, StorePrefixComparator> StoreMap;

		/*!
		 * \brief Register a store to this Debugger.
		 *
		 * If there is only one store registered, all objects of that store
		 * are accessible using the names as defined by the store.
		 * If multiple stores are mapped, all objects are prefixed using either
		 * the prefix supplied to #map(), or using the store's name when \p name is \c nullptr.
		 */
		template <typename Store>
		void map(Store& store, char const* name = nullptr) {
			map(new DebugStore<Store>(store), name);
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

		/*! \copydoc stored::DebugStoreBase::ListCallbackArg */
		typedef DebugStoreBase::ListCallbackArg ListCallbackArg;
		void list(ListCallbackArg* f, void* arg = nullptr) const;

#if STORED_cplusplus >= 201103L
		/*!
		 * \brief Callback function prototype as supplied to \c list().
		 *
		 * It receives the name of the object, and the corresponding
		 * #stored::DebugVariant of the object.
		 *
		 * \see #list<F>(F&&) const
		 */
		typedef void(ListCallback)(char const*, DebugVariant&);

		/*!
		 * \brief Iterates over the directory and invoke a callback for every object.
		 * \param f the callback to invoke, which can be any type, but must look like #ListCallback
		 */
		template <typename F>
		SFINAE_IS_FUNCTION(F, ListCallback, void)
		list(F&& f) const {
			std::function<ListCallback> f_ = f;
			auto cb = [](char const* name, DebugVariant& variant, void* f__) {
				(*static_cast<std::function<ListCallback>*>(f__))(name, variant);
			};
			list(static_cast<ListCallbackArg*>(cb), &f_);
		}
#endif
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
		virtual void capabilities(char*& list, size_t& len, size_t reserve = 0);
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
		ScratchPad<>& spm();

		/*! \brief Type of alias map. */
		typedef std::map<char, DebugVariant> AliasMap;
		AliasMap& aliases();
		AliasMap const& aliases() const;

		/*! \brief Type of macro map. */
		typedef std::map<char, std::string> MacroMap;
		MacroMap& macros();
		MacroMap const& macros() const;
		virtual bool runMacro(char m, ProtocolLayer& response);

		/*!
		 * \brief Encode a given value to ASCII hex.
		 * \see #encodeHex(stored::Type::type, void*&, size_t&, bool)
		 */
		template <typename T, typename B>
		size_t encodeHex(T value, B*& buf, bool shortest = true) {
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
		ScratchPad<> m_scratchpad;

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
		typedef std::map<char,
#if STORED_cplusplus >= 201103L
			std::unique_ptr<Stream<>>
#else
			Stream<>*
#endif
			> StreamMap;
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

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DEBUGGER_H
