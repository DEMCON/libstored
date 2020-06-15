#include <libstored/debugger.h>

#include <cstring>

namespace stored {

Debugger::~Debugger() {
	for(StoreMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
		delete it->second;
}

static bool checkPrefix(char const* name, char const* prefix) {
	stored_assert(name && prefix);
	// Check full prefix, as we only check against prefix and not to others,
	// so we cannot check disambiguity.
	return memcmp(name, prefix, strlen(prefix)) == 0;
}

DebugVariant Debugger::find(char const* name) const {
	if(unlikely(!name)) {
notfound:
		return DebugVariant();
	}

	// name contains '/prefix/object', where '/prefix' equals or is an
	// (unambiguous) abbreviation of the mapped name of a store.

	if(m_map.size() == 1) {
		// Don't compare prefix, just forward to the only mapped store.
		return m_map.begin()->second->find(&name[1]);
	}

	StoreMap::const_iterator it = m_map.upper_bound(name);
	// If found, it->first:
	// - is alphabetically after name, such as /prefix, while name was /pref/object (match)
	// - is alphabetically after name, such as /zz, while name was /pref/object (no match)
	// If not found, it equals end(), but (--it)->first:
	// - is alphabetically before name, such as /prefix, while name was /prefix/object (match)
	// - is alphabetically before name, such as /aa, while name was /prefix/object (no match)
	// So, check both it and --it.

	if(       it != m_map.end() && checkPrefix(name, it->first))
		goto gotit;
	else if(--it != m_map.end() && checkPrefix(name, it->first))
		goto gotit;
	else
		goto notfound;

gotit:
	// Strip first / from name, and let the store eat the rest of the prefix.
	return it->second->find(&name[1]);
}

void Debugger::map(DebugStoreBase* store, char const* name) {
	if(!name && store)
		name = store->name();

	if(!name || name[0] != '/' || !store) {
		// This is probably not what you meant...
		stored_assert(false);
		return;
	}

	StoreMap::iterator it = m_map.find(name);

	if(it == m_map.end()) {
		// New; insert.
		m_map.insert(std::make_pair(name, store));
	} else {
		// Replace previous mapping.
		delete it->second;
		it->second = store;
	}
}

void Debugger::unmap(char const* name) {
	StoreMap::iterator it = m_map.find(name);
	if(it == m_map.end())
		return;

	delete it->second;
	m_map.erase(it);
}

Debugger::StoreMap const& Debugger::stores() const {
	return m_map;
}

void Debugger::list(ListCallbackArg* f, void* arg) const {
	for(StoreMap::const_iterator it = m_map.begin(); it != m_map.end(); ++it)
		it->second->list(f, arg);
}

void Debugger::list(ListCallback* f) const {
	list((DebugStoreBase::ListCallbackArg*)f, nullptr);
}

} // namespace

