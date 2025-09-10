// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/debugger.h>

#include <cstring>

#ifdef STORED_COMPILER_ARMCC
#  pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace stored {

CLASS_NO_WEAK_VTABLE_DEF(DebugVariantBase)
CLASS_NO_WEAK_VTABLE_DEF(DebugVariant)
CLASS_NO_WEAK_VTABLE_DEF(DebugStoreBase)

/*!
 * \brief Arguments passed to \c listCmdCallback().
 */
struct ListCmdCallbackArg {
	Debugger* that;
	ProtocolLayer* response;
	size_t callbacks;

	/*! \brief Invoke the callback. */
	void operator()(void const* buf, size_t len)
	{
		response->encode(buf, len, false);
		++callbacks;
	}
};

/*!
 * \brief Constructor.
 * \param identification the identification, that is to be returned by #identification().
 * \param versions the version list, that is to be processed by #version().
 * \see #setIdentification()
 * \see #setVersions()
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Debugger::Debugger(char const* identification, char const* versions)
	: m_identification(identification)
	, m_versions(versions)
	, m_macroSize()
	, m_traceMacro()
	, m_traceStream()
	, m_traceDecimate()
	, m_traceCount()
{}

/*!
 * \brief Destructor.
 */
Debugger::~Debugger() noexcept
{
	for(StoreMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
		delete it->second; // NOLINT(cppcoreguidelines-owning-memory)
	for(StreamMap::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
		delete it->second; // NOLINT(cppcoreguidelines-owning-memory)
}

/*! \copydoc stored::DebugStoreBase::find() */
DebugVariant Debugger::find(char const* name, size_t len) const
{
	if(unlikely(!name || !len)) {
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

	switch(m_map.size()) {
	case 0:
		// No store mapped.
		goto notfound;
	case 1: {
		// The name may not contain a prefix. Try to forward it to the only mapped store.
		DebugVariant v = m_map.begin()->second->find(name, len);
		if(v.valid())
			return v;

		// Again, but strip / to drop the prefix and forward it.
		return m_map.begin()->second->find(&name[1], len - 1);
	}
	default:;
	}

	// name contains '/prefix/object', where '/prefix' equals the mapped name of
	// a store.

	size_t prefix_len = 1;
	for(; prefix_len < len && name[prefix_len] && name[prefix_len] != '/'; prefix_len++)
		;
	if(prefix_len == len)
		goto notfound;

	ScratchPad<>::Snapshot snapshot = spm().snapshot();
	char* prefix = spm().alloc<char>(prefix_len + 1);
	memcpy(prefix, name, prefix_len);
	prefix[prefix_len] = '\0';

	// Assume mapped stores [/aa, /ab, /b, /b2, /caa, /cb, /dd]
	// /0  : LB=/aa  LB++=/ab  UB=/aa  no match
	// /a  : LB=/aa  LB++=/ab  UB=/aa  no match
	// /aa : LB=/aa  LB++=/ab  UB=/ab  match (Rule 2)
	// /b  : LB=/b   LB++=/b2  UB=/b2  match (Rule 1)
	// /b2 : LB=/b2  LB++=/caa UB=/caa match (Rule 2)
	// /ca : LB=/caa LB++=/cb  UB=/cb  match (Rule 2)
	// /d  : LB=/dd  LB++=end  UB=/dd  match (Rule 2)
	// /e  : LB=end  LB++=end  UB=end  no match
	//
	// Rule 1: If LB equals the prefix, it's a match.
	// Rule 2: If LB starts with the prefix, it's a match when LB++ does not start with it.

	StoreMap::const_iterator it = m_map.lower_bound(prefix);
	if(it == m_map.end())
		goto notfound;			// NOLINT(bugprone-branch-clone)
	else if(strcmp(prefix, it->first) == 0) // Rule 1.
		goto gotit;			// NOLINT(bugprone-branch-clone)
	else if(::strncmp(prefix, it->first, prefix_len) == 0
		&& (++it == m_map.end() // NOLINT(bugprone-inc-dec-in-conditions)
		    || ::strncmp(prefix, it->first, prefix_len) != 0)) { // Rule 2.
		--it;
		goto gotit;
	} else
		goto notfound;

gotit:
	// Strip first / from name, and let the store eat the rest of the prefix.
	return it->second->find(&name[1], len - 1);
}

/*!
 * \brief Register a store to this Debugger.
 *
 * Don't use this function directly; use #map(Store&, char const*) instead.
 */
void Debugger::map(DebugStoreBase* store, char const* name)
{
	if(!name && store)
		name = store->name();

	if(!name || name[0] != '/' || strchr(name + 1, '/') || !store) {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete store;
		return;
	}

	StoreMap::iterator it = m_map.find(name);

	if(it == m_map.end()) {
		// New; insert.
		m_map.insert(StoreMap::value_type(name, store));
	} else {
		// Replace previous mapping.
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete it->second;
		it->second = store;
	}
}

/*!
 * \brief Unmaps a store from this Debugger.
 *
 * Note that if after unmapping there is only one mapped store left,
 * the prefixes are automatically dropped from all names.
 *
 * \see #map()
 */
void Debugger::unmap(char const* name)
{
	StoreMap::iterator it = m_map.find(name);
	if(it == m_map.end())
		return;

	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	delete it->second;
	m_map.erase(it);
}

/*!
 * \brief Returns the mapped stores.
 */
Debugger::StoreMap const& Debugger::stores() const
{
	return m_map;
}

/*!
 * \brief Returns the registered aliases.
 */
Debugger::AliasMap const& Debugger::aliases() const
{
	return m_aliases;
}

/*!
 * \copydoc aliases() const
 */
Debugger::AliasMap& Debugger::aliases()
{
	return m_aliases;
}

/*!
 * \brief Returns the defined macros.
 */
Debugger::MacroMap const& Debugger::macros() const
{
	return m_macros;
}

/*!
 * \copydoc macros() const
 */
Debugger::MacroMap& Debugger::macros()
{
	return m_macros;
}

/*!
 * \brief Iterates over the directory and invoke a callback for every object.
 * \param f the callback to invoke
 * \param arg an arbitrary argument to be passed to \p f
 */
void Debugger::list(ListCallbackArg* f, void* arg) const
{
	if(!Config::DebuggerListPrefixAlways && m_map.size() == 1)
		m_map.begin()->second->list(f, arg);
	else
		for(StoreMap::const_iterator it = m_map.begin(); it != m_map.end(); ++it)
			it->second->list(f, arg, it->first);
}

/*!
 * \brief Get the capabilities as supported by this Debugger.
 *
 * The \p list is allocated on the #spm().
 * The pointer and the length are returned through the \p list and \p len arguments.
 *
 * \param caps the list of capabilities
 * \param len the size of the buffer of \p list
 * \param reserve when allocating memory for \p list, add this number of bytes
 */
void Debugger::capabilities(char*& caps, size_t& len, size_t reserve)
{
	size_t const maxlen = 16;
	caps = spm().alloc<char>(maxlen + reserve);
	len = 0;

	caps[len++] = CmdCapabilities;
	if(Config::DebuggerRead)
		caps[len++] = CmdRead;
	if(Config::DebuggerWrite)
		caps[len++] = CmdWrite;
	if(Config::DebuggerEcho)
		caps[len++] = CmdEcho;
	if(Config::DebuggerList)
		caps[len++] = CmdList;
	if(Config::DebuggerAlias > 0)
		caps[len++] = CmdAlias;
	if(Config::DebuggerMacro > 0)
		caps[len++] = CmdMacro;
	if(Config::DebuggerIdentification) {
		char const* id = identification();
		if(id && *id)
			caps[len++] = CmdIdentification;
	}
	if(Config::DebuggerVersion > 0)
		caps[len++] = CmdVersion;
	if(Config::DebuggerReadMem)
		caps[len++] = CmdReadMem;
	if(Config::DebuggerWriteMem)
		caps[len++] = CmdWriteMem;
	if(Config::DebuggerStreams > 0 && Config::DebuggerStreamBuffer > 0)
		caps[len++] = CmdStream;
	if(Config::DebuggerTrace)
		caps[len++] = CmdTrace;
	if(Config::CompressStreams)
		caps[len++] = CmdFlush;

	stored_assert(len < maxlen);
	caps[len] = 0;
}

/*!
 * \brief Returns the identification.
 * \see #setIdentification()
 */
char const* Debugger::identification()
{
	return m_identification;
}

/*!
 * \brief Sets the identification.
 *
 * The supplied string is not copied; just the pointer is saved, so the string
 * must remain valid while the Debugger has its pointer.  So, it is fine to
 * supply a string literal.
 *
 * \see #identification()
 */
void Debugger::setIdentification(char const* identification)
{
	m_identification = identification;
}

/*!
 * \brief Push the version string into the given response.
 * \return \c true if the version is pushed, \c false if not available
 * \see #setVersions()
 */
bool Debugger::version(ProtocolLayer& response)
{
	char* buf = nullptr;
	size_t len = encodeHex(Config::DebuggerVersion, buf);
	response.encode(buf, len, false);

	if(m_versions && *m_versions) {
		response.encode(" ", 1, false);
		response.encode(m_versions, strlen(m_versions), false);
	}

	if(Config::Debug)
		response.encode(" debug", 6, false);

	return true;
}

/*!
 * \brief Set the versions as used by #version().
 *
 * The supplied string is not copied; just the pointer is saved, so the string
 * must remain valid while the Debugger has its pointer.  So, it is fine to
 * supply a string literal.
 *
 * \see #version()
 */
void Debugger::setVersions(char const* versions)
{
	m_versions = versions;
}

void Debugger::decode(void* buffer, size_t len)
{
	process(buffer, len, *this);
}

/*!
 * \brief Process a Embedded %Debugger message.
 * \param frame the frame to decode
 * \param len the length of \p frame
 * \param response the layer to push responses into
 */
void Debugger::process(void const* frame, size_t len, ProtocolLayer& response)
{
	if(unlikely(!frame || len == 0))
		return;

	ScratchPad<>::Snapshot snapshot = spm().snapshot();

	char const* p = static_cast<char const*>(frame);

	switch(p[0]) {
	case CmdCapabilities: {
		// get capabilities
		// Request: '?'
		// Response: <list of command chars>

		char* c = nullptr;
		size_t cs = 0;
		capabilities(c, cs);
		response.setPurgeableResponse();
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

		if(v.isVariable())
			response.setPurgeableResponse();

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
		while(len > 1 && *p != '/') {
			p++;
			len--;
		}
		if(len == 0)
			goto error;

		size_t valuelen =
			(size_t)(p - static_cast<char const*>(value));
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

		response.setPurgeableResponse();
		response.encode(p + 1, len - 1, true);
		return;
	}
	case CmdList: {
		// list all objects
		// Request: 'l'
		// Response: ( <type byte in hex> <length in hex> <name of object> '\n' ) *

		if(!Config::DebuggerList)
			goto error;

		response.setPurgeableResponse();
		ListCmdCallbackArg arg = {this, &response, 0};

		list(&listCmdCallback, &arg);

		if(!arg.callbacks)
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
			std::pair<AliasMap::iterator, bool> inserted =
				aliases().insert(std::make_pair(a, variant));
			if(!inserted.second)
				// Already there. Replace it.
				inserted.first->second = variant;
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

			if(it == macros().end())
				// Does not exist.
				break;

			if(it->second.empty())
				// Macro is currently in use.
				goto error;

			stored_assert(it->second.size() <= m_macroSize);
			m_macroSize -= it->second.size();
			macros().erase(it);
			break;
		}

		p += 2;
		len -= 2;

		MacroMap::iterator it = macros().find(m);
		if(it == macros().end()) {
			// New macro.
			size_t newlen = len + m_macroSize;
			if(newlen > Config::DebuggerMacro)
				goto error;

			std::pair<MacroMap::iterator, bool> inserted =
				macros().insert(MacroMap::value_type(m, String::type()));
			inserted.first->second.append(p, len);
			m_macroSize = newlen;
		} else {
			// Update existing macro.
			size_t newlen = len + m_macroSize - it->second.size();
			if(newlen > Config::DebuggerMacro)
				goto error;

			if(it->second.empty())
				// Macro is currently in use.
				goto error;

			it->second.clear();
			it->second.append(p, len);
			m_macroSize = newlen;
		}
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

		response.setPurgeableResponse();
		response.encode(id, strlen(id), true);
		return;
	}
	case CmdVersion: {
		// Return version(s)
		// Request: 'v'
		// Response: <debugger version> ( ' ' <application-defined version> ) *

		if(!Config::DebuggerVersion)
			goto error;

		response.setPurgeableResponse();
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
		size_t buflen = 1;
		p++;
		len -= 2;
		for(; len > 0 && *p != ' '; buflen++, p++, len--)
			;

		void const* buf = addrhex;
		if(!decodeHex(Type::Pointer, buf, buflen))
			goto error;

		stored_assert(buflen == sizeof(void*));
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		char* addr = *reinterpret_cast<char* const*>(buf);
		size_t datalen = sizeof(void*);

		if(len > 1) {
			buf = p + 1;
			buflen = len - 1;
			if(!decodeHex(Type::Uint, buf, buflen))
				goto error;

			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			datalen = (size_t) * reinterpret_cast<unsigned int const*>(buf);
		}

		if(datalen == 0)
			goto error;

		response.setPurgeableResponse();

		// Go read the memory in chunks
		while(datalen > 0) {
			ScratchPad<>::Snapshot chunk_snapshot = spm().snapshot();

			size_t chunk = std::min<size_t>(64U, datalen);
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
		size_t buflen = 1;
		p++;
		len -= 2;
		for(; len > 0 && *p != ' '; buflen++, p++, len--)
			;

		void const* buf = addrhex;
		if(!decodeHex(Type::Pointer, buf, buflen))
			goto error;

		stored_assert(buflen == sizeof(void*));
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		char* addr = *reinterpret_cast<char* const*>(buf);

		// p should point to the ' ' before the data
		if(len < 3) // ' ' and two hex chars of the first byte
			goto error;

		len--;
		p++;

		if(len & 1U)
			// Odd number of nibbles.
			goto error;

		// Process data in chunks.
		while(len > 0) {
			ScratchPad<>::Snapshot chunk_snapshot = spm().snapshot();

			size_t chunk = std::min<size_t>(64U, len);
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
	case CmdStream: {
		// Get stream data
		// Request: 's' ( <stream char> <suffix> ? ) ?
		//
		// Without stream char:
		// Response: '?' | <stream char> +
		//
		// With stream char
		// Response: <stream data> <suffix>

		if(Config::DebuggerStreams < 1)
			goto error;

		if(len == 1) {
			void const* buf = nullptr;
			size_t buflen = 0;
			streams(buf, buflen);
			if(buflen == 0)
				goto error;

			response.encode(buf, buflen);
			return;
		}

		char s = p[1];
		char const* suffix = &p[2];
		size_t suffixlen = len - 2;

		Stream<>* str = stream(s);
		if(!str)
			goto error;

		if(!Config::CompressStreams && tracing() && s == m_traceStream)
			response.setPurgeableResponse();

		if(Config::AvoidDynamicMemory) {
			String::type const& strstr = str->buffer();

			// Note that the buffer should not be realloc'ed during
			// the encode(), which may happen if Zth would do a
			// yield during the (low-level) encode(), or you would
			// have some other strange loop in your program.
			size_t size = strstr.size();
			if(size) {
				char const* data = strstr.data();
				response.encode(data, size, false);
				// cppcheck-suppress[knownConditionTrueFalse,unmatchedSuppression]
				stored_assert(data == strstr.data());

				// Note that the buffer may have grown
				// meanwhile. Only drop the data that we just
				// encoded.
				str->drop(size);
			}
		} else {
			// In this case, the buffer may grow arbitrarily.
			// Swap the buffers...
			String::type strbuf;
			str->swap(strbuf);
			response.encode(strbuf.data(), strbuf.size(), false);

			if(str->buffer().empty()) {
				// Buffer is empty. Clear and swap again to
				// avoid useless malloc/free of the internal
				// buffers of the String.
				strbuf.clear();
				str->swap(strbuf);
			}
			// else: data was added meanwhile, leave it in that
			// buffer and release the old one.
		}

		str->unblock();

		response.encode(suffix, suffixlen);
		return;
	}
	case CmdFlush: {
		// Flush a stream
		// Request: 'f' <stream char> ?
		//
		// Response: '!'

		if(!Config::CompressStreams)
			goto error;

		if(len == 1) {
			for(StreamMap::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
				it->second->flush();
		} else if(len == 2) {
			Stream<>* str = stream(p[1]);
			if(str)
				str->flush();
		} else
			goto error;

		break;
	}
	case CmdTrace: {
		// Configure trace
		//
		// Enable:
		// Request: 't' <macro> <stream> ( <decimate in hex> ) ?
		//
		// Disable:
		// Request: 't'
		//
		// Response: `?` | `!`

		if(!Config::DebuggerTrace)
			goto error;

		// Always disable by default.
		m_traceDecimate = 0;

		if(len == 1)
			// Disable tracing.
			break;

		// Enable tracing
		if(len < 3)
			goto error;

		m_traceMacro = p[1];
		m_traceStream = p[2];

		// Make sure the trace stream exists.
		if(!stream(m_traceStream, true))
			// Cannot allocate.
			goto error;

		if(len > 3) {
			void const* d = &p[3];
			size_t dlen = std::min<size_t>(8, len - 3); // at most 32-bit
			if(!decodeHex(Type::Uint, d, dlen))
				goto error;

			memcpy(&m_traceDecimate, d, sizeof(m_traceDecimate));
		} else
			m_traceDecimate = 1;
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

/*!
 * \brief Helper layer to merge responses for the macro response.
 */
class FrameMerger : public ProtocolLayer {
public:
	typedef ProtocolLayer base;

	explicit FrameMerger(ProtocolLayer& down)
		: base(nullptr, &down)
	{}

	void encode(void const* buffer, size_t len, bool last = true) final
	{
		STORED_UNUSED(last)
		base::encode(buffer, len, false);
	}

	using base::encode;

	void setPurgeableResponse(bool purgeable = true) final
	{
		STORED_UNUSED(purgeable)
	}
};

/*!
 * \brief Execute the given macro, and produce the results in the given response.
 * \return \c false when the macro does not exist
 */
bool Debugger::runMacro(char m, ProtocolLayer& response)
{
	MacroMap::iterator it = macros().find(m);

	if(it == macros().end())
		// Unknown macro.
		return false;

	String::type& definition = it->second;

	switch(definition.size()) {
	case 0:
		// Macro is currently being executed. No recursive calls are allowed.
		return false;
	case 1:
		// Expect the separator and at least one char to execute.  Nothing to do.
		return true;
	default:;
		// Go execute.
	}

	// Mark macro as in use by removing the definition temporarily.
	String::type definition_;
	definition.swap(definition_);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
	String::type const& def = const_cast<String::type const&>(definition_);

	char sep = def[0];

	FrameMerger merger(response);

	size_t pos = 0;
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
	do {
		size_t nextpos = def.find(sep, ++pos);
		size_t len = 0;
		if(nextpos == String::type::npos)
			len = def.size() - pos;
		else
			len = nextpos - pos;
		process(&def[pos], len, merger);
		pos = nextpos;
	} while(pos != String::type::npos);

	response.encode();

	// Restore recursion protection.
	definition.swap(definition_);
	return true;
}

/*!
 * \brief Encode the 4 LSb into ASCII hex.
 */
static char encodeNibble(uint8_t n)
{
	n &= 0xfU;
	return (char)((n < 10 ? '0' : 'a' - 10) + n);
}

/*!
 * \brief Encode data to ASCII hex.
 *
 * 'ASCII hex' is the hexadecimal representation as a ASCII string.  So, the
 * value 100 (decimal) is the value 0x64 (hex), so encoded as "64" (bytes: 0x36
 * 0x34).  The endianness is determined by the type of the data.
 *
 * \param type the type of the data
 * \param data a pointer to the data to be encoded, where the pointer is overwritten by a #spm()
 *	allocated buffer with the result
 * \param len the length of \p data, which is overwritten with the length of the result
 * \param shortest if \c true, trim the 0 from the left, if the type allows that
 * \see #decodeHex()
 */
void Debugger::encodeHex(Type::type type, void*& data, size_t& len, bool shortest)
{
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
			hex[(len - i) * 2 - 2] = encodeNibble((uint8_t)(*src >> 4U));
			hex[(len - i) * 2 - 1] = encodeNibble(*src);
#else
			hex[i * 2] = encodeNibble(*src >> 4U);
			hex[i * 2 + 1] = encodeNibble(*src);
#endif
		}

		len *= 2;

		if(shortest && Type::isInt(type))
			for(; len > 1 && hex[0] == '0'; len--, hex++)
				;
	} else {
		// just a byte sequence in hex
		for(size_t i = 0; i < len; i++, src++) {
			hex[i * 2] = encodeNibble((uint8_t)(*src >> 4U));
			hex[i * 2 + 1] = encodeNibble(*src);
		}

		len *= 2;
	}

	hex[len] = 0;
	data = hex;
}

/*!
 * \brief Decode a nibble.
 * \param c the nibble to decode in ASCII hex
 * \param ok will be set to \c false when decoding failed. The value is untouched when decoding was
 * successful.
 */
static uint8_t decodeNibble(char c, bool& ok)
{
	if(c >= '0' && c <= '9')
		return (uint8_t)(c - '0');
	if(c >= 'A' && c <= 'F')
		return (uint8_t)(c - 'A' + 10);
	if(c >= 'a' && c <= 'f')
		return (uint8_t)(c - 'a' + 10);

	ok = false;
	return 0;
}

/*!
 * \brief Decode ASCII hex.
 * \see #encodeHex(stored::Type::type, void*&, size_t&, bool)
 */
bool Debugger::decodeHex(Type::type type, void const*& data, size_t& len)
{
	if(len == 0 || !data)
		return false;

	char const* src = static_cast<char const*>(data);
	size_t binlen = 0;
	uint8_t* bin = nullptr;

	bool ok = true;

	if(Type::isFixed(type)) {
		binlen = Type::size(type);
		if(binlen * 2 < len)
			return false;

		bin = spm().alloc<uint8_t>(binlen);
		memset(bin, 0, binlen);

		for(size_t i = 0; i < len; i++) {
			uint8_t b = decodeNibble(src[len - i - 1], ok);
			if(i & 1U)
				b = (uint8_t)(b << 4U);

#ifdef STORED_LITTLE_ENDIAN
			bin[i / 2] |= b;
#else
			bin[(len - i - 1) / 2] |= b;
#endif
		}
	} else {
		if(len & 1U)
			// Bytes must come in pair of nibbles.
			return false;

		binlen = (len + 1) / 2;
		bin = spm().alloc<uint8_t>(binlen);

		for(size_t i = 0; i + 1 < len; i += 2) {
			bin[i / 2] =
				(uint8_t)((uint8_t)(decodeNibble(src[i], ok) << 4U) | decodeNibble(src[i + 1], ok));
		}
	}

	data = bin;
	len = binlen;
	return ok;
}

/*!
 * \brief Returns a scratch pad memory.
 */
ScratchPad<>& Debugger::spm() const
{
	return m_scratchpad;
}

/*!
 * \brief #list() callback for processing #CmdList.
 */
// cppcheck-suppress constParameterCallback
void Debugger::listCmdCallback(char const* name, DebugVariant& variant, void* arg)
{
	stored_assert(arg);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	ListCmdCallbackArg* a = (ListCmdCallbackArg*)arg;

	// encodeHex() uses the spm for its result.
	// cppcheck-suppress unreadVariable
	ScratchPad<>::Snapshot snapshot = a->that->spm().snapshot();
	char const* buf = nullptr;
	size_t buflen = 0;

	// Append type.
	buflen = a->that->encodeHex((uint8_t)variant.type(), buf, false);
	(*a)(buf, buflen);

	// Append length.
	buflen = a->that->encodeHex(variant.size(), buf);
	(*a)(buf, buflen);

	// Append name.
	(*a)(name, strlen(name));

	// End of item.
	(*a)("\n", 1);
}

/*!
 * \brief Adds a zero-terminated string to the given stream.
 * \see #stream(char, char const*, size_t)
 */
size_t Debugger::stream(char s, char const* data)
{
	return stream(s, data, strlen(data));
}

/*!
 * \brief Adds a buffer to the given stream.
 *
 * This function does not block when the buffer is full.
 *
 * \param s the stream to append to, which is created if it does not exist yet
 * \param data the data to be appended (which may include 0)
 * \param len the length of \p data
 * \return The length that was appended. On success, this equals \p len,
 *         but may be less if the buffer was full.
 */
size_t Debugger::stream(char s, char const* data, size_t len)
{
	if(Config::DebuggerStreams < 1)
		return 0;

	if(s == '?')
		// This is ambiguous in the 's' command return.
		return 0;

	Stream<>* str = stream(s, true);
	if(!str)
		return 0;

	len = str->fits(len);
	if(len == 0)
		return 0;

	str->encode(data, len);
	return len;
}

/*!
 * \brief Returns the stream buffer given a stream name.
 * \return the stream or \c nullptr when there is no stream with the given name
 */
Stream<> const* Debugger::stream(char s) const
{
	StreamMap::const_iterator it = m_streams.find(s);
	if(it == m_streams.end())
		return nullptr;

	return it->second;
}

/*!
 * \copydoc stream(char) const
 * \param s the stream name
 * \param alloc when set to \c true, try to allocate the stream if it does not exist yet
 */
Stream<>* Debugger::stream(char s, bool alloc)
{
	StreamMap::iterator it = m_streams.find(s);

	if(it != m_streams.end()) {
		return it->second;
	} else if(alloc) {
		StreamMap::mapped_type recycle = nullptr;

		bool cleaned = true;
		while(cleaned && m_streams.size() >= Config::DebuggerStreams) {
			// Out of buffers. Check if we can recycle something.
			cleaned = false;
			for(StreamMap::iterator it2 = m_streams.begin(); it2 != m_streams.end();
			    ++it2)
				if(it2->second->empty()) {
					// Got one.
					if(!recycle) {
						recycle = it2->second;
					} else {
						// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
						delete it2->second;
					}

					m_streams.erase(it2);
					cleaned = true;
					break;
				}
		}

		if(m_streams.size() < Config::DebuggerStreams) {
			// Add a new stream.
			return m_streams
				.insert(std::make_pair(s, recycle ? recycle : new Stream<>()))
				.first->second;
		} else {
			// Out of buffers.
			stored_assert(!recycle);
			return nullptr;
		}
	} else
		return nullptr;
}

/*!
 * \brief Gets the existing streams.
 * \param buffer an #spm() allocated buffer with stream names
 * \param len the length of the resulting \p buffer
 * \return 0-terminated string of stream names (which equals \c buffer)
 */
char const* Debugger::streams(void const*& buffer, size_t& len)
{
	size_t size = m_streams.size();
	char* b = spm().alloc<char>(size + 1);

	len = 0;
	for(StreamMap::const_iterator it = m_streams.begin(); it != m_streams.end(); ++it)
		if(!it->second->empty())
			b[len++] = it->first;

	b[len] = 0;
	buffer = b;
	return b;
}

/*!
 * \brief Executes the trace macro and appends the output to the trace buffer.
 *
 * The application should always call this function as often as required for tracing.
 * For example, if 1 kHz tracing is to be supported, make sure to call this function at (about) 1
 * kHz. Depending on the configuration, decimation is applied. This call will execute the macro,
 * when required, so the call is potentially expensive.
 *
 * The output of the macro is either completely or not at all put in the stream buffer;
 * it is not truncated.
 */
void Debugger::trace()
{
	if(!tracing())
		return;

	if(++m_traceCount < m_traceDecimate)
		// Do not trace yet.
		return;

	m_traceCount = 0;

	Stream<>* str = stream(m_traceStream, true);
	if(!str)
		// Out of streams.
		return;

	if(Config::DebuggerStreamBufferOverflow) {
		// Use the overflow size as an estimate of how large a sample
		// can be.
		if(str->fits(Config::DebuggerStreamBufferOverflow)
		   != Config::DebuggerStreamBufferOverflow)
			return;
	} else {
		// No overflow region. Just proceed when the buffer is not full
		// yet.
		if(str->buffer().size() >= Config::DebuggerStreamBuffer)
			return;
	}

	// Note that runMacro() may overrun the defined maximum buffer size,
	// but samples are never truncated.
	runMacro(m_traceMacro, *str);
	// If runMacro() returns false, the macro does not exist, and the Stream
	// is untouched, or there was an error while executing it. If the Stream
	// is compressed, the internal state cannot be restored once the first
	// byte is pushed in the Stream. In that case, the contents may be
	// corrupt. However, this only happens if one defines a wrong macro,
	// which is a user-error anyway.

	// Success.
}

/*!
 * \brief Checks if tracing is currently enabled and configured.
 */
bool Debugger::tracing() const
{
	return Config::DebuggerTrace && m_traceDecimate > 0;
}

} // namespace stored
