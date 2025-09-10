#ifndef LIBSTORED_SYNCHRONIZER_H
#define LIBSTORED_SYNCHRONIZER_H
// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#	include <libstored/macros.h>
#	include <libstored/protocol.h>
#	include <libstored/types.h>
#	include <libstored/util.h>

#	include <cstring>
#	include <map>
#	include <set>

namespace stored {

namespace impl {
class KeyCodec;
}

/*!
 * \brief A record of all changes within a store.
 *
 * Every variable in the store registers updates in the journal.
 * The journal keeps an administration based on the key of the variable.
 * Every change has a sequence number, which is kind of a time stamp.
 * This sequence number can be used to check which objects has changed
 * since some point in time.
 *
 * The current sequence number ('now') is bumped upon an encode or decode,
 * when there have been changes in between.
 *
 * Internally, only the last bytes of the sequence number is stored (short seq).
 * Therefore, there is a window (now-ShortSeqWindow .. now) of which
 * a short seq can be converted back to a real seq. Changes that are older than
 * the safe margin (now-SeqLowerMargin), are automatically shifted in time
 * to stay within the window. This may lead to some false positives when determining
 * which objects have changed since an old seq number. This is safe behavior,
 * but slightly less efficient for encoding updates.
 *
 * The administration is a binary tree, stored in a \c std::vector.  Every
 * node in the tree contains the maximum seq of any node below it, so a
 * search like 'find objects with a seq higher than x' can terminate early.
 * The vector must be regenerated when elements are inserted or removed.
 * This is expensive, but usually only happens during the initial phase of
 * the application.
 *
 * A store has only one journal, via #stored::Synchronizable. Multiple
 * instances of #stored::SyncConnection use the same journal.
 *
 * \see #stored::Synchronizable
 */
class StoreJournal {
	STORED_CLASS_NOCOPY(StoreJournal)
public:
	/*!
	 * \brief Timestamp of a change.
	 * \details 64-bit means that if it is bumped every ns, a wrap-around
	 *          happens after 500 years.
	 */
	typedef uint64_t Seq;
	/*!
	 * \brief A short version of Seq, used in all administration.
	 * \details This saves a lot of space, but limits handling timestamps
	 *          to ShortSeqWindow.
	 */
	typedef uint16_t ShortSeq;
	/*!
	 * \brief The key, as produced by a store.
	 * \details The key of a store is \c size_t. Limit it to 32-bit,
	 *          assuming that stores will not be bigger than 4G.
	 */
	typedef uint32_t Key;
	/*!
	 * \brief The size of an object.
	 * \details The 32-bit assumption is checked in the ctor.
	 */
	typedef Key Size;

	enum {
		/*! \brief Maximum offset of seq() that is a valid short seq. */
		ShortSeqWindow = 1U << (sizeof(ShortSeq) * 8U),
		/*! \brief Oldest margin where the short seq of changes should be moved. */
		SeqLowerMargin = ShortSeqWindow / 4U,
		/*! \brief Threshold for #clean(). */
		SeqCleanThreshold = SeqLowerMargin * 2U,
	};

	class StoreCallback {
		STORED_CLASS_NOCOPY(StoreCallback)
	public:
		StoreCallback() is_default
		virtual ~StoreCallback();

		virtual void hookEntryRO() noexcept = 0;
		virtual void hookExitRO() noexcept = 0;
		virtual void hookChanged() noexcept = 0;

		virtual void hookEntryRO(Type::type type, void* buffer, size_t len) noexcept = 0;
		virtual void hookExitRO(Type::type type, void* buffer, size_t len) noexcept = 0;
		virtual void hookChanged(Type::type, void* buffer, size_t len) noexcept = 0;

		virtual bool doHookEntryRO() noexcept = 0;
		virtual bool doHookExitRO() noexcept = 0;
		virtual bool doHookChanged() noexcept = 0;
		virtual bool doHooks() noexcept = 0;
	};

	StoreJournal(
		char const* hash, void* buffer, size_t size, StoreCallback* callback = nullptr);
	~StoreJournal() is_default

	static uint8_t keySize(size_t bufferSize);
	void* keyToBuffer(Key key, Size len = 0, bool* ok = nullptr) const;

	char const* hash() const;
	Seq seq() const;
	Seq bumpSeq();

	void clean(Seq oldest = 0);
	void changed(Key key, size_t len, bool insertIfNew = true);
	bool hasChanged(Key key, Seq since) const;
	bool hasChanged(Seq since) const;

#	if STORED_cplusplus >= 201103L
	// breathe does not like function typedefs
	using IterateChangedCallback = void(Key, void*);
#	else
	typedef void(IterateChangedCallback)(Key, void*);
#	endif
	void iterateChanged(Seq since, IterateChangedCallback* cb, void* arg = nullptr) const;

#	if STORED_cplusplus >= 201103L
	/*!
	 * \brief Iterate all changes since the given seq.
	 *
	 * The callback \p will receive the Key of the object that has changed
	 * since the given seq.
	 */
	template <typename F>
	SFINAE_IS_FUNCTION(F, void(Key), void)
	// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
	iterateChanged(Seq since, F&& cb) const
	{
		iterateChanged(
			since,
			[](Key key, void* cb_) {
				(*static_cast<typename std::decay<F>::type*>(cb_))(key);
			},
			&cb);
	}
#	endif

	void encodeHash(ProtocolLayer& p, bool last = false) const;
	static void encodeHash(ProtocolLayer& p, char const* hash, bool last = false);
	Seq encodeBuffer(ProtocolLayer& p, bool last = false);
	Seq encodeUpdates(uint8_t*& buf, Seq sinceSeq);

	static char const* decodeHash(void*& buffer, size_t& len);
	Seq decodeBuffer(void*& buffer, size_t& len);
	Seq decodeUpdates(void*& buffer, size_t& len, bool recordAll, void* scratch);

	void reserveHeap(size_t storeVariableCount);

protected:
	/*!
	 * \brief Element in the \c m_changes administration.
	 */
	struct ObjectInfo {
		ObjectInfo(
			StoreJournal::Key key_, StoreJournal::Size len_,
			StoreJournal::ShortSeq seq_)
			: key(key_)
			, len(len_)
			, seq(seq_)
			, highest(seq_)
		{}

		StoreJournal::Key key;
		StoreJournal::Size len;
		StoreJournal::ShortSeq seq;	// of this object
		StoreJournal::ShortSeq highest; // of all seqs in this part of the tree
	};

	/*!
	 * \brief Comparator to sort \c m_changes based on the key.
	 */
	struct ObjectInfoComparator {
		bool operator()(ObjectInfo const& a, ObjectInfo const& b) const
		{
			return a.key < b.key;
		}
	};

	Seq bumpSeq(bool force);
	ShortSeq toShort(Seq seq) const;
	Seq toLong(ShortSeq seq) const;

protected:
	bool update(Key key, size_t len, Seq seq, size_t lower, size_t upper);

	void regenerate();
	Seq regenerate(size_t lower, size_t upper);

	void encodeUpdates(uint8_t*& buf, Seq sinceSeq, size_t lower, size_t upper);
	void encodeUpdate(uint8_t*& buf, ObjectInfo& o);

	void encodeKey(ProtocolLayer& p, Key key);
	Key decodeKey(uint8_t*& buffer, size_t& len, bool& ok);

	void* buffer() const;
	size_t bufferSize() const;
	size_t keySize() const;

	void iterateChanged(
		Seq since, IterateChangedCallback* cb, void* arg, size_t lower, size_t upper) const;

private:
	char const* m_hash;
	void* m_buffer;
	size_t m_bufferSize;
	impl::KeyCodec const* m_keyCodec;
	Seq m_seq;
	Seq m_seqLower;
	bool m_partialSeq;
	StoreCallback* m_callback;


	// sorted based on key
	// set: binary tree lookup, update highest_seq while traversing the tree
	//      if new, full tree regeneration required (only startup effect)
	// iterate with lower bound on seq: DFS through tree, stop at highest_seq < given seq
	// no auto-remove objects (manual cleanup call required)
	typedef Vector<ObjectInfo>::type Changes;
	Changes m_changes;
};

/*!
 * \brief An extension of a store to be used by the #stored::Synchronizer.
 *
 * Assume you have \c MyStoreBase (which takes a template parameter
 * \c Implementation), and the actual store implementation \c ActualStore.
 * If it does not have to be synchronizable, you would inherit \c ActualStore from
 * \c MyStoreBase<ActualStore>.
 *
 * If it should be synchronizable, use #Synchronizable like this:
 *
 * \code
 * class ActualStore : public stored::Synchronizable<stored::MyStoreBase<ActualStore> > {
 *     STORED_CLASS_NOCOPY(ActualStore)
 * public:
 *     typedef stored::Synchronizable<stored::MyStoreBase<ActualStore> > base;
 *     using typename base::Implementation;
 *     friend class stored::MyStoreBase<ActualStore>;
 *     ActualStore() is_default
 *     ...
 * };
 * \endcode
 *
 * Or with a few macros:
 *
 * \code
 * class ActualStore: public STORE_SYNC_BASE_CLASS(MyStoreBase, ActualStore) {
 *     STORE_SYNC_CLASS_BODY(MyStoreBase, ActualStore)
 * public:
 *     ActualStore() is_default
 * };
 * \endcode
 *
 * Now, \c ActualStore inherits from \c Synchronizable, which inherits from \c MyStoreBase.
 * However, \c ActualStore is still used as the final implementation.
 */
template <typename Base>
class Synchronizable : public Base {
	STORE_WRAPPER_CLASS(Synchronizable, Base)

public:
	class TypedStoreCallback final : public StoreJournal::StoreCallback {
		STORED_CLASS_NOCOPY(TypedStoreCallback)
	public:
		explicit TypedStoreCallback(Synchronizable& store)
			: m_store(store)
		{}

		~TypedStoreCallback() override is_default

	private:
		static void hookEntryROCb(
			void* container, char const* name, Type::type type, void* buffer,
			size_t len, void* arg)
		{
			STORED_UNUSED(container)
			STORED_UNUSED(name)
			if(!Type::isFunction(type))
				static_cast<Synchronizable*>(arg)->hookEntryRO(type, buffer, len);
		}

		static void hookExitROCb(
			void* container, char const* name, Type::type type, void* buffer,
			size_t len, void* arg)
		{
			STORED_UNUSED(container)
			STORED_UNUSED(name)
			if(!Type::isFunction(type))
				static_cast<Synchronizable*>(arg)->hookExitRO(type, buffer, len);
		}

		static void hookChangedCb(
			void* container, char const* name, Type::type type, void* buffer,
			size_t len, void* arg)
		{
			STORED_UNUSED(container)
			STORED_UNUSED(name)
			if(!Type::isFunction(type))
				static_cast<Synchronizable*>(arg)->hookChanged(type, buffer, len);
		}

	public:
		void hookEntryRO() noexcept override
		{
			if(!doHookEntryRO())
				return;

			m_store.list(&hookEntryROCb, (void*)&m_store, nullptr, nullptr);
		}

		void hookExitRO() noexcept override
		{
			if(!doHookExitRO())
				return;

			m_store.list(&hookExitROCb, (void*)&m_store, nullptr, nullptr);
		}

		void hookEntryRO(Type::type type, void* buffer, size_t len) noexcept override
		{
			if(!doHookEntryRO())
				return;

			m_store.hookEntryRO(type, buffer, len);
		}

		void hookExitRO(Type::type type, void* buffer, size_t len) noexcept override
		{
			if(!doHookExitRO())
				return;

			m_store.hookExitRO(type, buffer, len);
		}

		void hookChanged() noexcept override
		{
			if(!doHookChanged())
				return;

			m_store.list(&hookChangedCb, (void*)&m_store, nullptr, nullptr);
		}

		void hookChanged(Type::type type, void* buffer, size_t len) noexcept override
		{
			if(!doHookChanged())
				return;

			m_store.hookChanged(type, buffer, len);
		}

		bool doHookEntryRO() noexcept override
		{
			return !Synchronizable::__hookEntryRO__default();
		}

		bool doHookExitRO() noexcept override
		{
			return !Synchronizable::__hookExitRO__default();
		}

		bool doHookChanged() noexcept override
		{
			return !Synchronizable::__hookChanged__default();
		}

		bool doHooks() noexcept override
		{
			return doHookEntryRO() || doHookExitRO() || doHookChanged();
		}

	private:
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		Synchronizable& m_store;
	};

	friend class TypedStoreCallback;

	typedef typename base::Objects Objects;

#	if STORED_cplusplus >= 201103L
	template <typename... Args>
	explicit Synchronizable(Args&&... args)
		: base(std::forward<Args>(args)...)
#	else
	Synchronizable()
		: base()
#	endif
		, m_callback(*this)
		, m_journal(base::hash(), base::buffer(), sizeof(base::data().buffer), &m_callback)
	{
		// Useless without hooks.
		// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
		stored_assert(Config::EnableHooks);
	}

	~Synchronizable() is_default

	StoreJournal const& journal() const
	{
		return m_journal;
	}

	StoreJournal& journal()
	{
		return m_journal;
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	operator StoreJournal const &() const
	{
		return journal();
	}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	operator StoreJournal&()
	{
		return journal();
	}

	/*!
	 * \brief Reserve worst-case heap usage.
	 *
	 * Afterwards, the store and synchronizer will not use any additional
	 * heap, which makes it possible to use it in a not-async-signal-safe context,
	 * like an interrupt handler.
	 */
	void reserveHeap()
	{
		journal().reserveHeap(Base::VariableCount);
	}

#	define MAX2(a, b)	 ((a) > (b) ? (a) : (b))
#	define MAX3(a, b, c)	 MAX2(MAX2((a), (b)), (c))
#	define MAX4(a, b, c, d) MAX3(MAX2((a), (b)), (c), (d))
	enum {
		/*! \brief Maximum size of any Synchronizer message for this store. */
		MaxMessageSize = MAX4(
			// Hello
			1 /*cmd*/ + 40 /*hash*/ + 1 /*nul*/ + 2 /*id*/,
			// Welcome
			1 /*cmd*/ + 2 * 2 /*ids*/ + Base::BufferSize,
			// Update
			1 /*cmd*/ + 2 /*id*/ + Base::BufferSize
				+ Base::VariableCount * 8 /*offset/length*/,
			// Bye
			1 /*cmd*/ + 40 /*hash*/
			),
	};
#	undef MAX4
#	undef MAX3
#	undef MAX2

protected:
	void __hookExitX(Type::type type, void* buffer, size_t len, bool changed) noexcept
	{
		if(changed) {
			StoreJournal::Key key = (StoreJournal::Key)this->bufferToKey(buffer);

			if(Config::EnableAssert) {
				bool ok = true;
				(void)ok;
				// cppcheck-suppress assertWithSideEffect
				stored_assert(
					journal().keyToBuffer(key, (StoreJournal::Size)len, &ok)
					== buffer);
				stored_assert(ok);
			}

			journal().changed(key, len);
		}

		base::__hookExitX(type, buffer, len, changed);
	}

private:
	TypedStoreCallback m_callback;
	StoreJournal m_journal;
};

/*! \deprecated Use \c stored::store or \c STORE_T instead. */
#	define STORE_SYNC_BASE_CLASS(Base, Impl) \
		STORE_T(Impl, ::stored::Synchronizable, ::stored::Base)

/*! \deprecated Use \c STORE_CLASS instead. */
#	define STORE_SYNC_CLASS_BODY(Base, Impl) \
		STORE_CLASS(Impl, ::stored::Synchronizable, ::stored::Base)

class Synchronizer;

/*!
 * \brief A one-to-one connection to synchronize one or more stores.
 *
 * A SyncConnection is related to one #stored::Synchronizer, and a
 * protocol stack to one other party. Using this connection, multiple
 * stores can be synchronized.
 *
 * The protocol is straight-forward: assume Synchronizer A wants to synchronize
 * a store with Synchronizer B via a SyncConnection:
 *
 * - A sends 'Hello' to B to indicate that it wants the full store
 *   immediately and updates afterwards.
 * - B sends 'Welcome' back to A, including the full store's buffer.
 * - When A has updates, it sends 'Update' to B.
 * - When B has updates, it sensd 'Update' to A.
 * - If A does not need updates anymore, it sends 'Bye'.
 * - B can send 'Bye' to A too, but this will probably break the application,
 *   as A usually cannot handle this.
 *
 * \see #stored::Synchronizer
 */
class SyncConnection : public ProtocolLayer {
	STORED_CLASS_NOCOPY(SyncConnection)
public:
	typedef ProtocolLayer base;
	typedef uint16_t Id;

#	ifdef DOXYGEN
	// breathe does not like complex expressions.
	static char const Hello = 'h';
	static char const Welcome = 'w';
	static char const Update = 'u';
	static char const Bye = 'b';
#	else
	static char const Hello = Config::StoreInLittleEndian ? 'h' : 'H';
	static char const Welcome = Config::StoreInLittleEndian ? 'w' : 'W';
	static char const Update = Config::StoreInLittleEndian ? 'u' : 'U';
	static char const Bye = Config::StoreInLittleEndian ? 'b' : 'B';
#	endif

	SyncConnection(Synchronizer& synchronizer, ProtocolLayer& connection);
	virtual ~SyncConnection() override;

	Synchronizer& synchronizer() const;

	bool isSynchronizing(StoreJournal& store) const;

	void source(StoreJournal& store);
	void drop(StoreJournal& store);
	StoreJournal::Seq process(StoreJournal& store, void* encodeBuffer);
	void decode(void* buffer, size_t len) override final;

	virtual void reset() override;

protected:
	Id nextId();
	void encodeCmd(char cmd, bool last = false);
	void encodeCmd(char cmd, uint8_t*& buf);
	void encodeId(Id id, bool last = false);
	void encodeId(Id id, uint8_t*& buf);
	static char decodeCmd(void*& buffer, size_t& len);
	static Id decodeId(void*& buffer, size_t& len);

	void bye();
	void bye(char const* hash);
	void bye(Id id);
	void erase(char const* hash);
	void eraseOut(Id id);
	void eraseIn(Id id);

	void dropNonSources();
	void helloAgain();
	void helloAgain(StoreJournal& store);

private:
	Synchronizer* m_synchronizer;

	struct StoreInfo {
		StoreInfo()
			: seq()
			, idOut()
			, source()
		{}

		StoreJournal::Seq seq;
		// Id determined by remote class (got via Hello message)
		Id idOut;
		// When true, this store was initially synchronized from there to here.
		bool source;
	};

	typedef Map<StoreJournal*, StoreInfo>::type StoreMap;
	StoreMap m_store;

	// Id determined by this class (set in Hello message)
	typedef Map<Id, StoreJournal*>::type IdInMap;
	IdInMap m_idIn;

	Id m_idInNext;
};

/*!
 * \brief The service that manages synchronization of stores over SyncConnections.
 *
 * A Synchronizer holds a set of stores, and a set of SyncConnections.
 * A store can be synchronized over multiple connections simultaneously.
 */
class Synchronizer {
	STORED_CLASS_NOCOPY(Synchronizer)
public:
	/*!
	 * \brief Ctor.
	 */
	Synchronizer() is_default
	~Synchronizer();

	/*!
	 * \brief Register a store in this Synchronizer.
	 */
	template <typename Store>
	void map(Synchronizable<Store>& store)
	{
		m_storeMap.insert(std::make_pair(store.hash(), &store.journal()));

		if(Synchronizable<Store>::MaxMessageSize > m_encodeBuffer.size())
			m_encodeBuffer.resize(Synchronizable<Store>::MaxMessageSize);
	}

	/*!
	 * \brief Deregister a store from this Synchronizer.
	 */
	template <typename Store>
	void unmap(Synchronizable<Store>& store)
	{
		m_storeMap.erase(store.hash());

		for(Connections::iterator it = m_connections.begin(); it != m_connections.end();
		    ++it)
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
			static_cast<SyncConnection*>(*it)->drop(store.journal());
	}

	StoreJournal* toJournal(char const* hash) const;

	SyncConnection const& connect(ProtocolLayer& connection);
	void disconnect(ProtocolLayer& connection);

	/*!
	 * \brief Mark the connection to be a source of the given store.
	 *
	 * The full store's buffer is received from the Synchronizer via the \p connection.
	 * Afterwards, updates and exchanged bidirectionally.
	 */
	template <typename Store>
	void syncFrom(Synchronizable<Store>& store, ProtocolLayer& connection)
	{
		StoreJournal* j = toJournal(store.hash());
		SyncConnection* c = toConnection(connection);
		if(!c || !j)
			return;
		c->source(*j);
	}

	/*!
	 * \brief Process updates for the given store on all connections.
	 */
	template <typename Store>
	void process(Synchronizable<Store>& store)
	{
		process(store.journal());
	}

	/*!
	 * \brief Process updates for the given store on the given connection.
	 */
	template <typename Store>
	StoreJournal::Seq process(ProtocolLayer& connection, Synchronizable<Store>& store)
	{
		return process(connection, store.journal());
	}

	void process();
	void process(StoreJournal& j);
	void process(ProtocolLayer& connection);
	StoreJournal::Seq process(ProtocolLayer& connection, StoreJournal& j);

	bool isSynchronizing(StoreJournal& j) const;
	bool isSynchronizing(StoreJournal& j, SyncConnection& notOverConnection) const;

	/*!
	 * \brief Return a buffer large enough to encode messages in.
	 */
	void* encodeBuffer()
	{
		return m_encodeBuffer.data();
	}

protected:
	SyncConnection* toConnection(ProtocolLayer& connection) const;

private:
	/*!
	 * \brief Comparator based on the hash string.
	 */
	struct HashComparator {
		bool operator()(char const* a, char const* b) const
		{
			return strcmp(a, b) < 0;
		}
	};

	typedef Map<char const*, StoreJournal*, HashComparator>::type StoreMap;
	StoreMap m_storeMap;

	typedef Set<ProtocolLayer*>::type Connections;
	Connections m_connections;

	typedef Vector<uint8_t>::type EncodeBuffer;
	EncodeBuffer m_encodeBuffer;
};

} // namespace stored
#endif // __cplusplus
#endif // LIBSTORED_SYNCHRONIZER_H
