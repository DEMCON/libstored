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
 * \brief Distributed store synchornizer.
 *
 * The communication is done in the store's endianness.
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
	 */
	template <typename Base>
	class Synchronizable : public Base {
		CLASS_NOCOPY(Synchronizable<Base>)
	public:
		typedef Base base;
		using base::Objects;
		using base::Implementation;

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
