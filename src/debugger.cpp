#include <libstored/debugger.h>

#include <cstring>

namespace stored {

Debugger::~Debugger() {
	for(StoreMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
		delete it->second;
}

static bool checkPrefix(char const* name, char const* prefix, size_t len) {
	stored_assert(name && prefix);
	// Check full prefix, as we only check against prefix and not to others,
	// so we cannot check disambiguity.
	return memcmp(name, prefix, std::min(len, strlen(prefix))) == 0;
}

DebugVariant Debugger::find(char const* name, size_t len) const {
	if(unlikely(!name)) {
notfound:
		return DebugVariant();
	}

	// name contains '/prefix/object', where '/prefix' equals or is an
	// (unambiguous) abbreviation of the mapped name of a store.

	if(m_map.size() == 1) {
		// Don't compare prefix, just forward to the only mapped store.
		return m_map.begin()->second->find(&name[1], len - 1);
	}

	StoreMap::const_iterator it = m_map.upper_bound(name);
	// If found, it->first:
	// - is alphabetically after name, such as /prefix, while name was /pref/object (match)
	// - is alphabetically after name, such as /zz, while name was /pref/object (no match)
	// If not found, it equals end(), but (--it)->first:
	// - is alphabetically before name, such as /prefix, while name was /prefix/object (match)
	// - is alphabetically before name, such as /aa, while name was /prefix/object (no match)
	// So, check both it and --it.

	if(       it != m_map.end() && checkPrefix(name, it->first, len))
		goto gotit;
	else if(--it != m_map.end() && checkPrefix(name, it->first, len))
		goto gotit;
	else
		goto notfound;

gotit:
	// Strip first / from name, and let the store eat the rest of the prefix.
	return it->second->find(&name[1], len - 1);
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

void Debugger::process(void const* data, size_t len) {
	// Default implementation: only process at application level.
	processApplication(data, len);
}

void Debugger::capabilities(char*& list, size_t& len, size_t reserve) {
	list = (char*)spm().alloc(4 + reserve);
	len = 0;

	list[len++] = '?';
	if(Config::DebuggerRead)
		list[len++] = 'r';
	if(Config::DebuggerWrite)
		list[len++] = 'w';

	list[len] = 0;
}

void Debugger::processApplication(void const* frame, size_t len) {
	if(unlikely(!frame || len == 0))
		return;
	
	spm().reset();

	char const* p = (char const*)frame;

	switch(p[0]) {
	case '?': {
		// get capabilities
		// Request: '?'
		// Response: <list of command chars>

		char* c;
		size_t cs;
		capabilities(c, cs);
		respondApplication(c, cs);
		return;
	}
	case 'r': {
		// read an object
		// Request: 'r' </path/to/object>
		// Response: <hex-encoded value>

		if(!Config::DebuggerRead)
			goto error;

		DebugVariant v = find(++p, --len);
		if(unlikely(!v.valid()))
			goto error;

		size_t size = v.size();
		void const* data = spm().alloc(size);
		size = v.get((void*)data, size);
		encodeHex(v.type(), data, size);
		respondApplication(data, size);
		return;
	}
	case 'w': {
		// write an object
		// Request: 'w' <hex-encoded value> </path/to/object>
		// Response: '?' | '!'

		if(!Config::DebuggerWrite)
			goto error;

		void const* value = ++p;
		len--;
		while(len > 0 && *p != '/') { p++; len--; }
		if(len == 0)
			goto error;

		size_t valuelen = (size_t)((uintptr_t)p - (uintptr_t)value);
		DebugVariant variant = find(p, len);
		if(!variant.valid())
			goto error;

		if(!decodeHex(variant.type(), value, valuelen))
			goto error;

		variant.set(value, valuelen);
		break;
	}
	default:
		// Unknown command.
		goto error;
	}

	respondApplication("!", 1);
	return;
error:
	respondApplication("?", 1);
}

void Debugger::respondApplication(void const* UNUSED_PAR(frame), size_t UNUSED_PAR(len)) {
	// Default implementation does nothing.
}

static char encodeNibble(uint8_t n) {
	n &= 0xf;
	return (char)((n < 10 ? '0' : 'a' - 10) + n);
}

void Debugger::encodeHex(Type::type type, void const*& data, size_t& len, bool shortest) {
	if(len == 0)
		return;

	if(!data) {
		len = 0;
		return;
	}

	uint8_t const* src = (uint8_t const*)data;

	if(shortest && type == Type::Bool) {
		// single char
		char* hex = (char*)spm().alloc(1);
		*hex = src[0] ? '1' : '0';
		len = 1;
		data = hex;
		return;
	}
	
	char* hex = (char*)spm().alloc(len * 2);

	if(Type::isFixed(type)) {
		// big endian
		for(size_t i = 0; i < len; i++, src++) {
#ifdef STORED_LITTLE_ENDIAN
			hex[(len - i) * 2 - 2] = encodeNibble((uint8_t)(*src >> 4));
			hex[(len - i) * 2 - 1] = encodeNibble(*src);
#else
			hex[i * 2] = encodeNibble(*src >> 4);
			hex[i * 2 + 1] = encodeNibble(*src);
#endif
		}

		len *= 2;

		if(shortest && Type::isInt(type))
			for(; len > 2 && hex[0] == '0' && hex[1] == '0'; len -= 2, hex += 2);
	} else {
		// just a byte sequence in hex
		char* hex = (char*)spm().alloc(len * 2);
		for(size_t i = 0; i < len; i++, src++) {
			hex[i * 2] = encodeNibble(*src);
			hex[i * 2 + 1] = encodeNibble((uint8_t)(*src >> 4));
		}

		len *= 2;
	}

	data = hex;
}

static uint8_t decodeNibble(char c, bool& ok) {
	if(c >= '0' && c <= '9')
		return (uint8_t)(c - '0');
	if(c >= 'A' && c <= 'F')
		return (uint8_t)(c - 'A' + 10);
	if(c >= 'a' && c <= 'f')
		return (uint8_t)(c - 'a' + 10);

	ok = false;
	return 0;
}

bool Debugger::decodeHex(Type::type type, void const*& data, size_t& len) {
	if(len == 0 || !data)
		return false;
	
	char const* src = (char const*)data;
	size_t binlen;
	uint8_t* bin;

	bool ok = true;

	if(Type::isFixed(type)) {
		binlen = Type::size(type);
		if(binlen * 2 < len)
			return false;

		bin = (uint8_t*)spm().alloc(binlen);
		memset(bin, 0, binlen);

		for(size_t i = 0; i < len; i += 2) {
#ifdef STORED_LITTLE_ENDIAN
			bin[i / 2] =
#else
			bin[(len + i) / 2 - 1] =
#endif
				(uint8_t)(
				(decodeNibble(src[len - i - 2], ok) << 4) |
				 decodeNibble(src[len - i - 1], ok));
		}
	} else {
		if(len & 1)
			// Bytes must come in pair of nibbles.
			return false;

		binlen = (len + 1) / 2;
		bin = (uint8_t*)spm().alloc(binlen);

		for(size_t i = 0; i + 1 < len; i += 2) {
			bin[i] =
				(uint8_t)(
				(decodeNibble(src[i], ok) << 4) |
				 decodeNibble(src[i + 1], ok));
		}
	}

	data = bin;
	len = binlen;
	return ok;
}

ScratchPad& Debugger::spm() {
	return m_scratchpad;
}

} // namespace

