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
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>
#include <libstored/types.h>
#include <libstored/protocol.h>

namespace stored {

#if 0
	class Connection : public ProtocolLayer {
		CLASS_NOCOPY(Connection)
	public:
		Connection(Synchronizer& synchronizer)
			: m_synchronizer(synchronizer)
		{}

		void decode(void* buffer, size_t len) final {
			m_synchronizer.decode(buffer, len, *this);
		}

		void link(StoreData store);
		void unlink(StoreData store);
		void process(StoreChanges changes) {
			// for all changes newer than the seq of the given store,
			// encode(changes)
		}

	private:
		Synchronizer& m_synchronizer;
		std::map<StoreData,int /*seq*/> m_stores;
	};
#endif

	class StoreJournal {
		CLASS_NOCOPY(StoreJournal)
	public:
		typedef uint16_t Id;
		typedef uint64_t Seq;
		typedef uint16_t ShortSeq;
		typedef uint32_t Key;
		typedef Key Size;

		enum {
			SeqLowerMargin = 1u << (sizeof(ShortSeq) * 8u - 2u),
			SeqCleanThreshold = SeqLowerMargin * 2u,
		};

		StoreJournal(void* buffer, size_t size);

		static uint8_t keySize(size_t bufferSize);
		void* keyToBuffer(Key key, Size len = 0, bool* ok = nullptr) const;

		Id id() const;
		Seq seq() const;

		void clean(Seq oldest = 0);
		void changed(Key key, size_t len);
		bool hasChanged(Key key, Seq since) const;

		void encodeId(ProtocolLayer& p, bool last = false);
		void encodeBuffer(ProtocolLayer& p, bool last = false);
		Seq encodeUpdates(ProtocolLayer& p, Seq sinceSeq, bool last = false);

		bool decodeUpdates(void* buffer, size_t len);

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

		Seq bumpSeq();
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

	private:
		void* const m_buffer;
		size_t const m_bufferSize;
		uint8_t const m_keySize;
		static Id m_idNext;
		Id const m_id;
		Seq m_seq;
		Seq m_seqLower;
		bool m_partialSeq;


		// sorted based on key
		// set: binary tree lookup, update highest_seq while traversing the tree
		//      if new, full tree regeneration required (only startup effect)
		// iterate with lower bound on seq: DFS through tree, stop at highest_seq < given seq
		// no auto-remove objects (manual cleanup call required)
		std::vector<ObjectInfo> m_changes;
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
			, m_journal(this->buffer(), sizeof(this->data().buffer))
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

#if 0
	class Synchronizer {
		CLASS_NOCOPY(Synchronizer)
	protected:
	public:

		template <typename Store>
		void map(Store& store, char const* id = nullptr) {
			if(!id)
				id = store.hash();

			StoreData storeData = { store.m_data.buffer, sizeof(store.m_data.buffer) };
			m_storeMap.insert(std::make_pair(id, storeData));
			store.notifyOnSet(this);
		}

		template <typename Store>
		void unmap(Store& store) {
			unmap(store.hash());
		}

		void unmap(char const* id) {
			m_storeMap.erase(id);
		}

		void connect(ProtocolLayer& connection) {
			Connection* c = new Connection(*this);
			connection.wrap(c);

			m_connectionMap.insert(std::make_pair(c, NULL));
		}

		void disconnect(ProtocolLayer& connection) {
			m_connectionMap.erase(&connection.up());
			delete connection.up();
		}

		void process() {
			for(it : m_connectionMap)
				process(it->second, it->first);
		}

		void process(ProtocolLayer& connection) {
			for(it : m_connectionMap.find(&connection.up()))
				process(it->second, connection);
		}

		void process(char const* id) {
			for(it : m_connectionMap)
				if(it->second == id)
					process(it->second, it->first);
		}

		void process(char const* id, ProtocolLayer& connection) {
			ProtocolLayer* c = connection.up();
		}

		void storeSet(char const* id, void* buffer, size_t len) {
			m_changedMap[id].insert(make_pair(seq, buffer, len));
		}

	protected:
		void decode(void* buffer, size_t len, ProtocolLayer& connection) {
			switch(buffer[0]) {
			case 'h':
				m_connectionMap.insert(&connection, buffer[1:]);
				encode("H", 1, false, connection);
				encode(m_storeMap[id].buffer, m_storeMap[id].length, true, connection);
				break;
			case 'b':
				m_connectionMap.erase(&connection, buffer[1:]);
				encode("B", 1, true, connection);
				break;
			}
		}

		void encode(void const* buffer, size_t len, bool last, ProtocolLayer& connection) {
			connection.encode(buffer, len, last);
		}

		typedef std::map<char const*,StoreData> StoreMap;
		StoreMap m_storeMap;

		typedef std::multimap<ProtocolLayer*, char const* id> ConnectionMap;
		ConnectionMap m_connectionMap;
	};
#endif

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_SYNCHRONIZER_H
