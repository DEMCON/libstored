#ifndef __LIBSTORED_SYNCHRONIZER_H
#define __LIBSTORED_SYNCHRONIZER_H
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
 * \defgroup libstored_synchronizer synchronizer
 * \brief Distributed store synchronizer.
 *
 * In a distributed system, every process has its own instance of a store.
 * Synchronization between these instances is implemented by the #stored::Synchronizer.
 * The Synchronizer can be seen as a service, usually one per process,
 * which knows all stores in that process and all communication channels to other
 * processes. At regular intervals, it sends updates of locally modified data
 * to the other Synchronizers.
 *
 * The topology can be configured at will. In principle, a process can have any
 * number of stores, any number of synchronizers (which all handle any subset of the stores),
 * any number of connections to any other process in the system.
 *
 * There are a few rules to keep in mind:
 *
 * - Only #stored::Synchronizable stores can be handled by the Synchronizer.
 *   This has to be used correctly when the store is instantiated.
 * - To synchronize a store, one must define which store is the one that
 *   provides the initial value. Upon connection between Synchronizers, the
 *   store's content is synchronized at once from one party to the other.
 *   Afterwards, updates are sent in both directions.
 * - Writes to different objects in the same store by the same process are
 *   observed by every other process in the same order. All other write orders
 *   are undefined (like writes to objects of different stores by the same
 *   process, or writes to the same store by different processes), and can be
 *   observed to happen in a different order by different processes at the same
 *   time.
 * - Writes to one object should only be done by one process. So, every process owns
 *   a subset of a store. If multiple processes write to the same object, behavior
 *   is undefined. That would be a race-condition anyway.
 * - The communication is done in the store's endianness. If a distributed
 *   system have processors with different endianness, they should be configured
 *   to all-little or all-big endian. Accessing the store by the processor that
 *   has a store in a non-native endianness, might be a bit more expensive, but
 *   synchronization is cheaper.
 * - Stores are identified by their (SHA-1) hash. This hash is computed over the full
 *   source code of the store (the .st file). So, only stores with the exact same
 *   definition, and therefore layout, can be synchronized.
 *
 * The protocol for synchronization consists of four messages. These are sent
 * when appropriate, not in a request-response paradigm. There is no acknowledge.
 * Invalid messages are just ignored.
 *
 * ### Hello
 *
 * "I would like to have the full state and future changes
 * of the given store (by hash). All updates, send to me
 * using this reference."
 *
 * (`h` | `H`) \<hash\> \<id\>
 *
 * The hash is returned by the \c hash() function of the store, including the
 * null-terminator. The id is arbitrary chosen by the Synchronizer, and is
 * 16-bit in the store's endianness (`h` indicates little endian, `H` is big).
 *
 * ### Welcome (as a response to a Hello)
 *
 * "You are welcome. Here is the full buffer state, upon your request, of the
 * store with given reference. Any updates to the store at your side,
 * provide them to me with my reference."
 *
 * (`w` | `W`) \<hello id\> \<welcome id\> \<buffer\>
 *
 * The hello id is the id as received in the hello message (by the other party).
 * The welcome id is chosen by this Synchronizer, in the same manner.
 *
 * ### Update
 *
 * "Your store, with given reference, has changed.
 * The changes are attached."
 *
 * (`u` | `U`) \<id\> \<updates\>
 *
 * The updates are a sequence of the triplet: \<key\> \<length\> \<data\>.
 * The key and length have the most significant bytes stripped, which would
 * always be 0.  All values are in the store's endianness (`u` is little, `U` is
 * big endian).
 *
 * Proposal: The updates are a sequence defined as follows:
 * \<5 MSb key offset, 3 LSb length\> \<additional key bytes\> \<additional length bytes\> \<data\>.
 * The key offset + 1 is the offset from the previous entry in the updates sequence.
 * Updates are sent in strict ascending key offset.
 * The initial key is -1. For example, if the previous key was 10 and the 5 MSb indicate 3,
 * then the next key is 10 + 3 + 1, so 14. If the 5 MSb are all 1 (so 31), an additional
 * key byte is added after the first byte (which may be 0). This value is added
 * to the key offset.  If that value is 255, another key byte is added, etc.
 *
 * The 3 LSb bits of the first byte are decoded according to the following list:
 *
 * - 0: data length is 1
 * - 1: data length is 2
 * - 2: data length is 3
 * - 3: data length is 4
 * - 4: data length is 5
 * - 5: data length is 6
 * - 6: data length is 7, and an additional length byte follows (like the key offset)
 * - 7: data length is 8
 *
 * Using this scheme, when all variables change within the store, the overhead is always
 * one byte per variable (plus additional length bytes, but this is uncommon
 * and fixed for a given store). This is also the upper limit of the update message.
 * If less variables change, the key offset may be larger, but the total size is always less.
 *
 * The asymmetry of having 6 as indicator for additional length bytes is because
 * this is an unlikely value (7 bytes data), and at least far less common than having
 * 8 bytes of data.
 *
 * The data is sent in the store's endianness (`u` is little, `U` is big endian).
 *
 * ### Bye
 *
 * "I do not need any more updates of the given store (by hash, by id or all)."
 *
 * (`b` | `B`) \<hash\><br>
 * (`b` | `B`) \<id\><br>
 * (`b` | `B`)
 *
 * A bye using the id can be used to respond to another message that has an unknown id.
 * Previous communication sessions remnants can be cleaned up in this way.
 *
 * `b` indicates that the id is as little endian, `B` indicates big endian.
 * For the other two variants, there is no difference in endianness, but both
 * versions are defined for symmetry.
 *
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>
#include <libstored/types.h>
#include <libstored/protocol.h>

#include <map>
#include <set>
#include <cstring>

namespace stored {

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
	 * \ingroup libstored_synchronizer
	 */
	class StoreJournal {
		CLASS_NOCOPY(StoreJournal)
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
			ShortSeqWindow = 1u << (sizeof(ShortSeq) * 8u),
			/*! \brief Oldest margin where the short seq of changes should be moved. */
			SeqLowerMargin = ShortSeqWindow / 4u,
			/*! \brief Threshold for #clean(). */
			SeqCleanThreshold = SeqLowerMargin * 2u,
		};

		StoreJournal(char const* hash, void* buffer, size_t size);
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

#if STORED_cplusplus >= 201103L
		// breathe does not like function typedefs
		using IterateChangedCallback = void(Key, void*);
#else
		typedef void(IterateChangedCallback)(Key, void*);
#endif
		void iterateChanged(Seq since, IterateChangedCallback* cb, void* arg = nullptr) const;

#if STORED_cplusplus >= 201103L
		/*!
		 * \brief Iterate all changes since the given seq.
		 *
		 * The callback \p will receive the Key of the object that has changed
		 * since the given seq.
		 */
		template <typename F>
		SFINAE_IS_FUNCTION(F, void(Key), void)
		iterateChanged(Seq since, F&& cb) const {
			std::function<void(Key)> f = cb;
			iterateChanged(since,
				[](Key key, void* f_) {
					(*static_cast<std::function<void(Key)>*>(f_))(key); },
				&f);
		}
#endif

		void encodeHash(ProtocolLayer& p, bool last = false) const;
		static void encodeHash(ProtocolLayer& p, char const* hash, bool last = false);
		Seq encodeBuffer(ProtocolLayer& p, bool last = false);
		Seq encodeUpdates(ProtocolLayer& p, Seq sinceSeq, bool last = false);

		static char const* decodeHash(void*& buffer, size_t& len);
		Seq decodeBuffer(void*& buffer, size_t& len);
		Seq decodeUpdates(void*& buffer, size_t& len, bool recordAll = true);

		void reserveHeap(size_t storeVariableCount);

	protected:
		/*!
		 * \brief Element in the \c m_changes administration.
		 */
		struct ObjectInfo {
			ObjectInfo(StoreJournal::Key key, StoreJournal::Size len, StoreJournal::ShortSeq seq)
				: key(key), len(len), seq(seq), highest(seq)
			{}

			StoreJournal::Key key;
			StoreJournal::Size len;
			StoreJournal::ShortSeq seq; // of this object
			StoreJournal::ShortSeq highest; // of all seqs in this part of the tree
		};

		/*!
		 * \brief Comparator to sort \c m_changes based on the key.
		 */
		struct ObjectInfoComparator {
			bool operator()(ObjectInfo const& a, ObjectInfo const& b) const {
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

		void encodeUpdates(ProtocolLayer& p, Seq sinceSeq, size_t lower, size_t upper);
		void encodeUpdate(ProtocolLayer& p, ObjectInfo& o);

		void encodeKey(ProtocolLayer& p, Key key);
		Key decodeKey(uint8_t*& buffer, size_t& len, bool& ok);

		void* buffer() const;
		size_t bufferSize() const;
		size_t keySize() const;

		void iterateChanged(Seq since, IterateChangedCallback* cb, void* arg, size_t lower, size_t upper) const;

	private:
		char const* const m_hash;
		void* const m_buffer;
		size_t const m_bufferSize;
		uint8_t const m_keySize;
		Seq m_seq;
		Seq m_seqLower;
		bool m_partialSeq;


		// sorted based on key
		// set: binary tree lookup, update highest_seq while traversing the tree
		//      if new, full tree regeneration required (only startup effect)
		// iterate with lower bound on seq: DFS through tree, stop at highest_seq < given seq
		// no auto-remove objects (manual cleanup call required)
		typedef std::vector<ObjectInfo> Changes;
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
	 *     CLASS_NOCOPY(ActualStore)
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
	 * class ActualStore: public STORE_SYNC_BASECLASS(MyStoreBase, ActualStore) {
	 *     STORE_SYNC_CLASS_BODY(MyStoreBase, ActualStore)
	 * public:
	 *     ActualStore() is_default
	 * };
	 * \endcode
	 *
	 * Now, \c ActualStore inherits from \c Synchronizable, which inherits from \c MyStoreBase.
	 * However, \c ActualStore is still used as the final implementation.
	 *
	 * \ingroup libstored_synchronizer
	 */
	template <typename Base>
	class Synchronizable : public Base {
		CLASS_NOCOPY(Synchronizable<Base>)
	public:
		typedef Base base;
		typedef typename base::Objects Objects;
		typedef typename base::Implementation Implementation;

#if STORED_cplusplus >= 201103L
		template <typename... Args>
		explicit Synchronizable(Args&&... args)
			: base(std::forward<Args>(args)...)
#else
		Synchronizable()
			: base()
#endif
			, m_journal(base::hash(), base::buffer(), sizeof(base::data().buffer))
		{
			// Useless without hooks.
			// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert)
			stored_assert(Config::EnableHooks);
		}
		~Synchronizable() is_default

		StoreJournal const& journal() const { return m_journal; }
		StoreJournal& journal() { return m_journal; }

		/*!
		 * \brief Reserve worst-case heap usage.
		 *
		 * Afterwards, the store and synchronizer will not use any additional
		 * heap, which makes it possible to use it in a not-async-signal-safe context,
		 * like an interrupt handler.
		 */
		void reserveHeap() {
			journal().reserveHeap(Base::VariableCount);
		}

#define MAX2(a,b)		((a) > (b) ? (a) : (b))
#define MAX3(a,b,c)		MAX2(MAX2((a), (b)), (c))
#define MAX4(a,b,c,d)	MAX3(MAX2((a), (b)), (c), (d))
		enum {
			/*! \brief Maximum size of any Synchronizer message for this store. */
			MaxMessageSize =
				MAX4(
					// Hello
					1 /*cmd*/ + 40 /*hash*/ + 1 /*nul*/ + 2 /*id*/,
					// Welcome
					1 /*cmd*/ + 2 * 2 /*ids*/ + Base::BufferSize,
					// Update
					1 /*cmd*/ + 2 /*id*/ + Base::BufferSize + Base::VariableCount * 8 /*offset/length*/,
					// Bye
					1 /*cmd*/ + 40 /*hash*/
				),
		};
#undef MAX4
#undef MAX3
#undef MAX2

	protected:
		void __hookExitX(Type::type type, void* buffer, size_t len, bool changed) {
			if(changed) {
				StoreJournal::Key key = (StoreJournal::Key)this->bufferToKey(buffer);

				if(Config::EnableAssert) {
					bool ok = true;
					(void)ok;
					// cppcheck-suppress assertWithSideEffect
					stored_assert(journal().keyToBuffer(key, (StoreJournal::Size)len, &ok) == buffer);
					stored_assert(ok);
				}

				journal().changed(key, len);
			}

			base::__hookExitX(type, buffer, len, changed);
		}

	private:
		StoreJournal m_journal;
	};

#define STORE_SYNC_BASECLASS(Base, Impl) stored::Synchronizable<stored::Base<Impl > >

#define STORE_SYNC_CLASS_BODY(Base, Impl) \
	CLASS_NOCOPY(Impl) \
public: \
	typedef STORE_SYNC_BASECLASS(Base, Impl) base; \
	using typename base::Implementation; \
	friend class STORE_BASECLASS(Base, Impl); \
private:


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
	 * \ingroup libstored_synchronizer
	 */
	class SyncConnection : public ProtocolLayer {
		CLASS_NOCOPY(SyncConnection)
	public:
		typedef ProtocolLayer base;
		typedef uint16_t Id;

#ifdef DOXYGEN
		// breathe does not like complex expressions.
		static char const Hello		= 'h';
		static char const Welcome	= 'w';
		static char const Update	= 'u';
		static char const Bye		= 'b';
#else
		static char const Hello		= Config::StoreInLittleEndian ? 'h' : 'H';
		static char const Welcome	= Config::StoreInLittleEndian ? 'w' : 'W';
		static char const Update	= Config::StoreInLittleEndian ? 'u' : 'U';
		static char const Bye		= Config::StoreInLittleEndian ? 'b' : 'B';
#endif

		SyncConnection(Synchronizer& synchronizer, ProtocolLayer& connection);
		virtual ~SyncConnection() override;

		Synchronizer& synchronizer() const;

		bool isSynchronizing(StoreJournal& store) const;

		void source(StoreJournal& store);
		void drop(StoreJournal& store);
		StoreJournal::Seq process(StoreJournal& store);
		void decode(void* buffer, size_t len) override final;

		virtual void reset() override;

	protected:
		Id nextId();
		void encodeCmd(char cmd, bool last = false);
		void encodeId(Id id, bool last = false);
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
		Synchronizer& m_synchronizer;

		struct StoreInfo {
			StoreInfo() : seq(), idOut(), source() {}

			StoreJournal::Seq seq;
			// Id determined by remote class (got via Hello message)
			Id idOut;
			// When true, this store was initially synchronized from there to here.
			bool source;
		};

		typedef std::map<StoreJournal*, StoreInfo> StoreMap;
		StoreMap m_store;

		// Id determined by this class (set in Hello message)
		typedef std::map<Id, StoreJournal*> IdInMap;
		IdInMap m_idIn;

		Id m_idInNext;
	};

	/*!
	 * \brief The service that manages synchronization of stores over SyncConnections.
	 *
	 * A Synchronizer holds a set of stores, and a set of SyncConnections.
	 * A store can be synchronized over multiple connections simultaneously.
	 *
	 * \ingroup libstored_synchronizer
	 */
	class Synchronizer {
		CLASS_NOCOPY(Synchronizer)
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
		void map(Synchronizable<Store>& store) {
			m_storeMap.insert(std::make_pair(store.hash(), &store.journal()));
		}

		/*!
		 * \brief Deregister a store from this Synchronizer.
		 */
		template <typename Store>
		void unmap(Synchronizable<Store>& store) {
			m_storeMap.erase(store.hash());

			for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
				static_cast<SyncConnection*>(*it)->drop(store.journal());
		}

		StoreJournal* toJournal(char const* hash) const;

		void connect(ProtocolLayer& connection);
		void disconnect(ProtocolLayer& connection);

		/*!
		 * \brief Mark the connection to be a source of the given store.
		 *
		 * The full store's buffer is received from the Synchronizer via the \p connection.
		 * Afterwards, updates and exchanged bidirectionally.
		 */
		template <typename Store>
		void syncFrom(Synchronizable<Store>& store, ProtocolLayer& connection) {
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
		void process(Synchronizable<Store>& store) {
			process(store.journal());
		}

		/*!
		 * \brief Process updates for the given store on the given connection.
		 */
		template <typename Store>
		StoreJournal::Seq process(ProtocolLayer& connection, Synchronizable<Store>& store) {
			return process(connection, store.journal());
		}

		void process();
		void process(StoreJournal& j);
		void process(ProtocolLayer& connection);
		StoreJournal::Seq process(ProtocolLayer& connection, StoreJournal& j);

		bool isSynchronizing(StoreJournal& j) const;
		bool isSynchronizing(StoreJournal& j, SyncConnection& notOverConnection) const;

	protected:
		SyncConnection* toConnection(ProtocolLayer& connection) const;

	private:
		/*!
		 * \brief Comparator based on the hash string.
		 */
		struct HashComparator {
			bool operator()(char const* a, char const* b) const {
				return strcmp(a, b) < 0;
			}
		};

		typedef std::map<char const*, StoreJournal*, HashComparator> StoreMap;
		StoreMap m_storeMap;

		typedef std::set<ProtocolLayer*> Connections;
		Connections m_connections;
	};

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_SYNCHRONIZER_H
