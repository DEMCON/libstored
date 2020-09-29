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
 * `h` \<hash\> \<id\>
 *
 * The hash is returned by the \c hash() function of the store, including the
 * null-terminator. The id is arbitrary chosen by the Synchronizer, and is
 * 16-bit in the store's endianness.
 *
 * ### Welcome (as a response to a Hello)
 *
 * "You are welcome. Here is the full buffer state, upon your request, of the
 * store with given reference. Any updates to the store at your side,
 * provide them to me with my reference."
 *
 * `w` \<hello id\> \<welcome id\> \<buffer\>
 *
 * The hello id is the id as received in the hello message (by the other party).
 * The welcome id is chosen by this Synchronizer, in the same manner.
 *
 * ### Update
 *
 * "Your store, with given reference, has changed.
 * The changes are attached."
 *
 * `u` \<id\> \<updates\>
 *
 * The updates are a sequence of the triple: \<key\> \<length\> \<data\>.
 * The key and length have the most significant bytes stripped, which would
 * always be 0.  The data is in the store's endianness.
 *
 * ### Bye
 *
 * "I do not need any more updates of the given store (by hash, by id or all)."
 *
 * 'b' hash
 * 'b' id
 * 'b'
 *
 * A bye using the id can be used to respond to another message that has an unknown id.
 * Previous communication sessions remnents can be cleaned up in this way.
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
	 * \see #stored::Synchronizable
	 * \ingroup libstored_synchronizer
	 */
	class StoreJournal {
		CLASS_NOCOPY(StoreJournal)
	public:
		typedef uint64_t Seq;
		typedef uint16_t ShortSeq;
		typedef uint32_t Key;
		typedef Key Size;

		enum {
			SeqLowerMargin = 1u << (sizeof(ShortSeq) * 8u - 2u),
			SeqCleanThreshold = SeqLowerMargin * 2u,
		};

		StoreJournal(char const* hash, void* buffer, size_t size);

		static uint8_t keySize(size_t bufferSize);
		void* keyToBuffer(Key key, Size len = 0, bool* ok = nullptr) const;

		char const* hash() const;
		Seq seq() const;
		Seq bumpSeq();

		void clean(Seq oldest = 0);
		void changed(Key key, size_t len);
		bool hasChanged(Key key, Seq since) const;
		bool hasChanged(Seq since) const;

		typedef void(IterateChangedCallback)(Key, void*);
		void iterateChanged(Seq since, IterateChangedCallback* cb, void* arg = nullptr);

#if STORED_cplusplus >= 201103L
		template <typename F>
		SFINAE_IS_FUNCTION(F, void(Key), void)
		iterateChanged(Seq since, F&& cb) {
			std::function<void(Key)> f = cb;
			iterateChanged(since,
				[](Key key, void* f_) {
					(*static_cast<std::function<void(Key)>*>(f_))(key); },
				&f);
		}
#endif

		void encodeHash(ProtocolLayer& p, bool last = false);
		static void encodeHash(ProtocolLayer& p, char const* hash, bool last = false);
		Seq encodeBuffer(ProtocolLayer& p, bool last = false);
		Seq encodeUpdates(ProtocolLayer& p, Seq sinceSeq, bool last = false);

		static char const* decodeHash(void*& buffer, size_t& len);
		Seq decodeBuffer(void*& buffer, size_t& len);
		Seq decodeUpdates(void*& buffer, size_t& len);

	protected:
		struct ObjectInfo {
			ObjectInfo(StoreJournal::Key key, StoreJournal::Size len, StoreJournal::ShortSeq seq)
				: key(key), len(len), seq(seq), highest(seq)
			{}

			StoreJournal::Key key;
			StoreJournal::Size len;
			StoreJournal::ShortSeq seq; // of this object
			StoreJournal::ShortSeq highest; // of all seqs in this part of the tree
		};

		struct ObjectInfoComparator {
			bool operator()(ObjectInfo const& a, ObjectInfo const& b) const {
				return a.key < b.key;
			}
		};

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

		void iterateChanged(Seq since, IterateChangedCallback* cb, void* arg, size_t lower, size_t upper);

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
	 * class ActualStore : public Synchronizable<MyStoreBase<ActualStore> > {
	 * ...
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
		using typename base::Objects;
		using typename base::Implementation;

#if STORED_cplusplus >= 201103L
		template <typename... Args>
		Synchronizable(Args&&... args)
			: base(std::forward<Args>(args)...)
#else
		Synchronizable()
			: base()
#endif
			, m_journal(this->hash(), this->buffer(), sizeof(this->data().buffer))
		{
			// Useless without hooks.
			stored_assert(Config::EnableHooks);
		}

		StoreJournal const& journal() const { return m_journal; }
		StoreJournal& journal() { return m_journal; }

	protected:
		void __hookExitX(Type::type type, void* buffer, size_t len, bool changed) {
			if(changed) {
				StoreJournal::Key key = (StoreJournal::Key)this->bufferToKey(buffer);

				if(Config::EnableAssert) {
					bool ok __attribute__((unused)) = true;
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

	class Synchronizer;

	/*!
	 * \brief A one-to-one connection to synchronize one or more stores.
	 * \see #stored::Synchronizer
	 * \ingroup libstored_synchronizer
	 */
	class SyncConnection : public ProtocolLayer {
		CLASS_NOCOPY(SyncConnection)
	public:
		typedef ProtocolLayer base;
		typedef uint16_t Id;

		static char const Hello		= Config::StoreInLittleEndian ? 'h' : 'H';
		static char const Welcome	= Config::StoreInLittleEndian ? 'w' : 'W';
		static char const Update	= Config::StoreInLittleEndian ? 'u' : 'U';
		static char const Bye		= Config::StoreInLittleEndian ? 'b' : 'B';

		SyncConnection(Synchronizer& synchronizer, ProtocolLayer& connection);
		virtual ~SyncConnection() override;

		Synchronizer& synchronizer() const;

		void source(StoreJournal& store);
		void drop(StoreJournal& store);
		void process(StoreJournal& store);
		void decode(void* buffer, size_t len) override final;

	protected:
		Id nextId();
		void encodeCmd(char cmd, bool last = false);
		void encodeId(Id id, bool last = false);
		char decodeCmd(void*& buffer, size_t& len);
		static Id decodeId(void*& buffer, size_t& len);

		void bye();
		void bye(char const* hash);
		void bye(Id id);
		void erase(char const* hash);
		void erase(Id id);

	private:
		Synchronizer& m_synchronizer;

		typedef std::map<StoreJournal*, StoreJournal::Seq> SeqMap;
		SeqMap m_seq;

		// Id determined by remote class (got via Hello message)
		typedef std::map<StoreJournal*, Id> IdOutMap;
		IdOutMap m_idOut;

		// Id determined by this class (set in Hello message)
		typedef std::map<Id, StoreJournal*> IdInMap;
		IdInMap m_idIn;

		Id m_idInNext;
	};

	/*!
	 * \brief The service that manages synchronization of stores over SyncConnections.
	 * \ingroup libstored_synchronizer
	 */
	class Synchronizer {
		CLASS_NOCOPY(Synchronizer)
	public:

		Synchronizer() is_default
		~Synchronizer();

		template <typename Store>
		void map(Synchronizable<Store>& store) {
			m_storeMap.insert(std::make_pair(store.hash(), &store.journal()));
		}

		template <typename Store>
		void unmap(Synchronizable<Store>& store) {
			m_storeMap.erase(store.hash());

			for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
				static_cast<SyncConnection*>(*it)->drop(store.journal());
		}

		StoreJournal* toJournal(char const* hash) const;

		void connect(ProtocolLayer& connection);
		void disconnect(ProtocolLayer& connection);

		template <typename Store>
		void syncFrom(Synchronizable<Store>& store, ProtocolLayer& connection) {
			StoreJournal* j = toJournal(store.hash());
			SyncConnection* c = toConnection(connection);
			if(!c || !j)
				return;
			c->source(*j);
		}

		template <typename Store>
		void process(Synchronizable<Store>& store) {
			process(store.journal());
		}

		template <typename Store>
		void process(ProtocolLayer& connection, Synchronizable<Store>& store) {
			process(connection, store.journal());
		}

		void process();
		void process(StoreJournal& j);
		void process(ProtocolLayer& connection);
		void process(ProtocolLayer& connection, StoreJournal& j);

	protected:
		SyncConnection* toConnection(ProtocolLayer& connection) const;

	private:
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
