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

#include <libstored/debugger.h>

#include <cstring>
#include <string>

template <typename T>
static void cleanup(T* t) {
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	delete t;
}

#if __cplusplus >= 201103L
template <typename T>
static void cleanup(std::unique_ptr<T>& UNUSED_PAR(t)) {
}
#endif

namespace stored {

struct ListCmdCallbackArg {
	Debugger* that;
	ProtocolLayer* response;
	bool gotSomething;

	void operator()(void const* buf, size_t len) {
		response->encode(buf, len, false);
		gotSomething = true;
	}
};

Debugger::Debugger(char const* identification)
	: m_identification(identification)
	, m_versions()
	, m_macroSize()
{
}

Debugger::~Debugger()
#if __cplusplus < 201103L
{
	for(StoreMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
		cleanup(it->second);
}
#else
	= default;
#endif

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

	if(Config::DebuggerAlias > 0 && len == 1 && name[0] != '/') {
		// This is probably an alias.
		AliasMap::const_iterator it = aliases().find(name[0]);
		if(it != aliases().end())
			// Got it.
			return it->second;
	}

	if(m_map.size() == 1) {
		// Don't compare prefix, just forward to the only mapped store.
		return m_map.begin()->second->find(name, len);
	}
	
	// name contains '/prefix/object', where '/prefix' equals he mapped name of
	// a store.

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

	if(!name || name[0] != '/' || !store)
		return;

	StoreMap::iterator it = m_map.find(name);

	if(it == m_map.end()) {
		// New; insert.
		m_map.insert(std::make_pair(name, store));
	} else {
		// Replace previous mapping.
#if __cplusplus >= 201103L
		it->second.reset(store);
#else
		cleanup(it->second);
		it->second = store;
#endif
	}
}

void Debugger::unmap(char const* name) {
	StoreMap::iterator it = m_map.find(name);
	if(it == m_map.end())
		return;

	cleanup(it->second);
	m_map.erase(it);
}

Debugger::StoreMap const& Debugger::stores() const {
	return m_map;
}

Debugger::AliasMap& Debugger::aliases() {
	return m_aliases;
}

Debugger::MacroMap const& Debugger::macros() const {
	return m_macros;
}

Debugger::MacroMap& Debugger::macros() {
	return m_macros;
}

Debugger::AliasMap const& Debugger::aliases() const {
	return m_aliases;
}

void Debugger::list(ListCallbackArg* f, void* arg) const {
	if(m_map.size() == 1)
		m_map.begin()->second->list(f, arg);
	else
		for(StoreMap::const_iterator it = m_map.begin(); it != m_map.end(); ++it)
			it->second->list(f, arg, it->first);
}

void Debugger::capabilities(char*& list, size_t& len, size_t reserve) {
	size_t const maxlen = 16;
	list = spm().alloc<char>(maxlen + reserve);
	len = 0;

	list[len++] = CmdCapabilities;
	if(Config::DebuggerRead)
		list[len++] = CmdRead;
	if(Config::DebuggerWrite)
		list[len++] = CmdWrite;
	if(Config::DebuggerEcho)
		list[len++] = CmdEcho;
	if(Config::DebuggerList)
		list[len++] = CmdList;
	if(Config::DebuggerAlias > 0)
		list[len++] = CmdAlias;
	if(Config::DebuggerMacro > 0)
		list[len++] = CmdMacro;
	if(Config::DebuggerIdentification > 0)
		list[len++] = CmdIdentification;
	if(Config::DebuggerVersion > 0)
		list[len++] = CmdVersion;

	stored_assert(len < maxlen);
	list[len] = 0;
}

char const* Debugger::identification() {
	return m_identification;
}

void Debugger::setIdentification(char const* identification) {
	m_identification = identification;
}

bool Debugger::version(ProtocolLayer& response) {
	char* buffer;
	size_t len = encodeHex(Config::DebuggerVersion, buffer);
	response.encode(buffer, len, false);

	if(m_versions && *m_versions) {
		response.encode(" ", 1, false);
		response.encode(m_versions, strlen(m_versions), false);
	}

	return true;
}

void Debugger::setVersions(char const* versions) {
	m_versions = versions;
}

void Debugger::decode(void* buffer, size_t len) {
	process(buffer, len, *this);
}

void Debugger::process(void const* frame, size_t len, ProtocolLayer& response) {
	if(unlikely(!frame || len == 0))
		return;
	
	ScratchPad::Snapshot snapshot = spm().snapshot();

	char const* p = static_cast<char const*>(frame);

	switch(p[0]) {
	case CmdCapabilities: {
		// get capabilities
		// Request: '?'
		// Response: <list of command chars>

		char* c;
		size_t cs;
		capabilities(c, cs);
		response.encode(c, cs, true);
		return;
	}
	case CmdRead: {
		// read an object
		// Request: 'r' </path/to/object>
		// Response: <hex-encoded value>

		if(!Config::DebuggerRead)
			goto error;

		DebugVariant v = find(++p, --len);
		if(unlikely(!v.valid()))
			goto error;

		size_t size = v.size();
		void* data = spm().alloc<char>(size);
		size = v.get(data, size);
		encodeHex(v.type(), data, size);
		response.encode(data, size, true);
		return;
	}
	case CmdWrite: {
		// write an object
		// Request: 'w' <hex-encoded value> </path/to/object>
		// Response: '?' | '!'

		if(!Config::DebuggerWrite)
			goto error;

		void const* value = ++p;
		len--;
		// Find / that indicates the object.
		// Always use last char, which may be an alias.
		while(len > 1 && *p != '/') { p++; len--; }
		if(len == 0)
			goto error;

		size_t valuelen = (size_t)(static_cast<char const*>(p) - static_cast<char const*>(value));
		DebugVariant variant = find(p, len);
		if(!variant.valid())
			goto error;

		if(!decodeHex(variant.type(), value, valuelen))
			goto error;

		variant.set(value, valuelen);
		break;
	}
	case CmdEcho: {
		// echo the request
		// Request: 'e' <any data>
		// Response: <the data>

		if(!Config::DebuggerEcho)
			goto error;

		if(len <= 1)
			goto error;

		response.encode(p + 1, len - 1, true);
		return;
	}
	case CmdList: {
		// list all objects
		// Request: 'l'
		// Response: ( <type byte in hex> <length in hex> <name of object> '\n' ) *

		if(!Config::DebuggerList)
			goto error;

		ListCmdCallbackArg arg = {this, &response, false};

		list(&listCmdCallback, &arg);

		if(!arg.gotSomething)
			goto error;

		response.encode();
		return;
	}
	case CmdAlias: {
		// define an alias
		// Request: 'a' <char> </path/to/object> ?
		// Response: '!' | '?'

		if(Config::DebuggerAlias <= 0)
			goto error;

		if(len < 2)
			goto error;

		char a = p[1];
		if(a < 0x20 || a > 0x7e || a == '/')
			// invalid
			goto error;

		if(len == 2) {
			// erase given alias
			aliases().erase(a);
			break;
		}

		DebugVariant variant = find(&p[2], len - 2);
		if(!variant.valid())
			goto error;

		if(aliases().size() == Config::DebuggerAlias) {
			// Only accept this alias if it replaces one.
			AliasMap::iterator it = aliases().find(a);
			if(it == aliases().end())
				// Overflow.
				goto error;

			// Replace.
			it->second = variant;
		} else {
			// Add new alias.
			aliases().insert(std::make_pair(a, variant));
		}
		break;
	}
	case CmdMacro: {
		// define a macro
		// Request: 'm' <char> ( <sep> <any command> ) *
		// Response: '!' | '?'

		if(Config::DebuggerMacro <= 0)
			goto error;

		if(len < 2)
			goto error;

		char m = p[1];
		if(len == 2) {
			// Erase macro
			MacroMap::iterator it = macros().find(m);
			if(it != macros().end()) {
				stored_assert(it->second.size() <= m_macroSize);
				m_macroSize -= it->second.size();
				macros().erase(it);
			}
			break;
		}

		p += 2;
		len -= 2;
		if(len + m_macroSize > Config::DebuggerMacro)
			goto error;

		macros().insert(MacroMap::value_type(m, std::string(p, len)));
		m_macroSize += len;
		break;
	}
	case CmdIdentification: {
		// Return identification string
		// Request: 'i'
		// Response: <UTF-8 encoded string>

		if(!Config::DebuggerIdentification)
			goto error;
	
		char const* id = identification();
		if(!id || !*id)
			// Not supported, apparently.
			goto error;

		response.encode(id, strlen(id), true);
		return;
	}
	case CmdVersion: {
		// Return version(s)
		// Request: 'v'
		// Response: <debugger version> ( ' ' <application-defined version> ) *

		if(!Config::DebuggerVersion)
			goto error;
	
		if(!version(response))
			// No version output.
			goto error;

		response.encode();
		return;
	}
	case CmdReadMem: {
		// Read memory directly.
		// Request: 'R' <ASCII-hex address> ( ' ' <length> ) ?
		// Response: '?' | <ASCII-hex bytes>

		if(!Config::DebuggerReadMem)
			goto error;
	
		if(len < 2)
			goto error;

		char const* addrhex = ++p;

		// Find end of address
		size_t bufferlen = 1;
		p++;
		len -= 2;
		for(; len > 0 && *p != ' '; bufferlen++, p++, len--);

		void const* buffer = addrhex;
		if(!decodeHex(Type::Pointer, buffer, bufferlen))
			goto error;
		
		stored_assert(bufferlen == sizeof(void*));
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		char* addr = *reinterpret_cast<char* const*>(buffer);
		size_t datalen = sizeof(void*);

		if(len > 1) {
			buffer = p + 1;
			bufferlen = len - 1;
			if(!decodeHex(Type::Uint, buffer, bufferlen))
				goto error;

			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			datalen = (size_t)*reinterpret_cast<unsigned int const*>(buffer);
		}

		if(datalen == 0)
			goto error;

		// Go read the memory in chunks
		while(datalen > 0) {
			ScratchPad::Snapshot snapshot = spm().snapshot();

			size_t chunk = std::min<size_t>(64u, datalen);
			void* r = addr;
			size_t rlen = chunk;
			encodeHex(Type::Blob, r, rlen);
			response.encode(r, rlen, false);
			addr += chunk;
			datalen -= chunk;
		}

		response.encode();
		return;
	}
	case CmdWriteMem: {
		// Write memory directly.
		// Request: 'W' <ASCII-hex address> ' ' <ASCII-hex bytes>
		// Response: '!' | '?'
		//
		if(!Config::DebuggerWriteMem)
			goto error;
	
		if(len < 2)
			goto error;
	
		char const* addrhex = ++p;

		// Find length of address
		size_t bufferlen = 1;
		p++;
		len -= 2;
		for(; len > 0 && *p != ' '; bufferlen++, p++, len--);

		void const* buffer = addrhex;
		if(!decodeHex(Type::Pointer, buffer, bufferlen))
			goto error;
		
		stored_assert(bufferlen == sizeof(void*));
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		char* addr = *reinterpret_cast<char* const*>(buffer);

		// p should point to the ' ' before the data
		if(len < 3) // ' ' and two hex chars of the first byte
			goto error;

		len--;
		p++;

		if(len & 1u)
			// Odd number of nibbles.
			goto error;

		// Process data in chunks.
		while(len > 0) {
			ScratchPad::Snapshot snapshot = spm().snapshot();

			size_t chunk = std::min<size_t>(64u, len);
			void const* w = p;
			size_t wlen = chunk;
			if(!decodeHex(Type::Blob, w, wlen))
				goto error;
			memcpy(addr, w, wlen);
			addr += chunk;
			p += chunk;
			len -= chunk;
		}
		break;
	}
	default:
		// Unknown command.

		if(Config::DebuggerMacro && !macros().empty()) {
			// Try parsing this as a macro.
			if(runMacro(p[0], response))
				return;
		}

		goto error;
	}

	{
		char ack = Ack;
		response.encode(&ack, 1, true);
	}
	return;
error:
	char nack = Nack;
	response.encode(&nack, 1, true);
}

class FrameMerger : public ProtocolLayer {
public:
	typedef ProtocolLayer base;

	explicit FrameMerger(ProtocolLayer& down)
		: base(nullptr, &down)
	{}

	void encode(void const* buffer, size_t len, bool UNUSED_PAR(last) = true) final {
		base::encode(buffer, len, false);
	}

	void encode(void* buffer, size_t len, bool UNUSED_PAR(last) = true) final {
		base::encode(buffer, len, false);
	}
};

bool Debugger::runMacro(char m, ProtocolLayer& response) {
	MacroMap::iterator it = macros().find(m);

	if(it == macros().end())
		// Unknown macro.
		return false;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
	std::string const& definition = const_cast<std::string const&>(it->second);

	// Expect the separator and at least one char to execute.
	if(definition.size() < 2)
		// Nothing to do.
		return true;

	char sep = definition[0];

	FrameMerger merger(response);

	size_t pos = 0;
	do {
		size_t nextpos = definition.find(sep, ++pos);
		size_t len;
		if(nextpos == std::string::npos)
			len = definition.size() - pos;
		else
			len = nextpos - pos;
		process(&definition[pos], len, merger);
		pos = nextpos;
	} while(pos != std::string::npos);

	response.encode();
	return true;
}

static char encodeNibble(uint8_t n) {
	n &= 0xf;
	return (char)((n < 10 ? '0' : 'a' - 10) + n);
}

void Debugger::encodeHex(Type::type type, void*& data, size_t& len, bool shortest) {
	if(len == 0)
		return;

	if(!data) {
		len = 0;
		return;
	}

	uint8_t const* src = static_cast<uint8_t const*>(data);

	if(shortest && type == Type::Bool) {
		// single char
		char* hex = spm().alloc<char>(2);
		hex[0] = src[0] ? '1' : '0';
		hex[1] = 0;
		len = 1;
		data = hex;
		return;
	}
	
	char* hex = spm().alloc<char>(len * 2 + 1);

	if(Type::isFixed(type)) {
		// big endian
		for(size_t i = 0; i < len; i++, src++) {
#ifdef STORED_LITTLE_ENDIAN
			hex[(len - i) * 2 - 2] = encodeNibble((uint8_t)(*src >> 4u));
			hex[(len - i) * 2 - 1] = encodeNibble(*src);
#else
			hex[i * 2] = encodeNibble(*src >> 4u);
			hex[i * 2 + 1] = encodeNibble(*src);
#endif
		}

		len *= 2;

		if(shortest && Type::isInt(type))
			for(; len > 1 && hex[0] == '0'; len--, hex++);
	} else {
		// just a byte sequence in hex
		for(size_t i = 0; i < len; i++, src++) {
			hex[i * 2] = encodeNibble((uint8_t)(*src >> 4u));
			hex[i * 2 + 1] = encodeNibble(*src);
		}

		len *= 2;
	}

	hex[len] = 0;
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
	
	char const* src = static_cast<char const*>(data);
	size_t binlen;
	uint8_t* bin;

	bool ok = true;

	if(Type::isFixed(type)) {
		binlen = Type::size(type);
		if(binlen * 2 < len)
			return false;

		bin = spm().alloc<uint8_t>(binlen);
		memset(bin, 0, binlen);

		for(size_t i = 0; i < len; i++) {
			uint8_t b = decodeNibble(src[len - i - 1], ok);
			if(i & 1u)
				b = (uint8_t)(b << 4u);

#ifdef STORED_LITTLE_ENDIAN
			bin[i / 2] |= b;
#else
			bin[(len - i - 1) / 2] |= b;
#endif
		}
	} else {
		if(len & 1u)
			// Bytes must come in pair of nibbles.
			return false;

		binlen = (len + 1) / 2;
		bin = spm().alloc<uint8_t>(binlen);

		for(size_t i = 0; i + 1 < len; i += 2) {
			bin[i / 2] = (uint8_t)(
				(uint8_t)(decodeNibble(src[i], ok) << 4u) |
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

void Debugger::listCmdCallback(char const* name, DebugVariant& variant, void* arg) {
	stored_assert(arg);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	ListCmdCallbackArg* a = (ListCmdCallbackArg*)arg;

	// encodeHex() uses the spm for its result.
	ScratchPad::Snapshot snapshot = a->that->spm().snapshot();
	char const* buf;
	size_t buflen;

	// Append type.
	buflen = a->that->encodeHex((uint8_t)variant.type(), buf, false);
	(*a)(buf, buflen);

	// Append length.
	a->that->encodeHex(variant.size(), buf, buflen);
	(*a)(buf, buflen);

	// Append name.
	(*a)(name, strlen(name));

	// End of item.
	(*a)("\n", 1);
}

} // namespace

