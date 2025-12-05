// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/aes.h>

#ifdef STORED_HAVE_AES

#  include <new>

extern "C" {
#  include <aes.h>
} // extern "C"

#  ifdef STORED_OS_POSIX
#    include <ctime>
#  endif // STORED_OS_POSIX


namespace stored {



////////////////////////////////////////////////////////////////
// Aes256BaseLayer
//

Aes256BaseLayer::Aes256BaseLayer(void const* key, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_key()
	, m_buffer()
	, m_bufferLen()
	, m_encState(EncStateDisconnected)
	, m_decState(DecStateDisconnected)
	, m_unified()
	, m_lastError(ENOTCONN)
#  ifdef STORED_OS_POSIX
	// NOLINTNEXTLINE
	, m_seed((unsigned int)(uintptr_t)this ^ (unsigned int)time(nullptr))
#  endif // STORED_OS_POSIX
{
	setKey(key);
}

void Aes256BaseLayer::reset()
{
	m_encState = EncStateDisconnected;
	m_decState = DecStateDisconnected;
	m_lastError = ENOTCONN;
	base::reset();
}

void Aes256BaseLayer::connected()
{
	size_t mtu = this->mtu();
	if(mtu && mtu < BlockSize + 1) {
		// MTU too small.
		m_lastError = EMSGSIZE;
		if(m_encState != EncStateDisconnected || m_decState != DecStateDisconnected)
			disconnected();
		return;
	}

	m_encState = EncStateConnected;
	if(m_decState == DecStateDisconnected)
		m_decState = DecStateConnected;
	m_lastError = 0;
	base::connected();
}

void Aes256BaseLayer::disconnected()
{
	m_encState = EncStateDisconnected;
	m_decState = DecStateDisconnected;
	if(!m_lastError)
		m_lastError = ENOTCONN;
	base::disconnected();
}

int Aes256BaseLayer::lastError() const noexcept
{
	return m_lastError;
}

void Aes256BaseLayer::decode(void* buffer, size_t len)
{
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);

again:
	switch(m_decState) {
	case DecStateDisconnected:
		// Ignore data.
		return;
	case DecStateConnected:
	case DecStateReady: {
		// Decrypt data.
		if(len == 0) {
			// Nothing to do.
			return;
		}
		if(len > BlockSize && len % BlockSize == 1) {
			// Got IV for decryption.
			switch(buffer_[0]) {
			case CmdBidirectional:
				if(unified())
					unified(false);
				m_lastError = initDecrypt(m_key, buffer_ + 1);
				break;
			case CmdUnified:
				m_unified = true;
				if(m_encState == EncStateConnected)
					m_encState = EncStateReady;
				m_lastError = initUnified(m_key, buffer_ + 1);
				break;
			default:
				// Invalid command.
				m_lastError = EINVAL;
			}

			if(m_lastError) {
				// Initialization error.
				disconnected();
				return;
			}

			m_decState = DecStateReady;
			buffer_ += BlockSize + 1;
			len -= BlockSize + 1;
			goto again;
		}
		if(m_decState != DecStateReady || len % BlockSize != 0) {
			// Invalid block.
			m_lastError = EINVAL;
			disconnected();
			return;
		}

		m_lastError = decrypt(buffer_, len, unified());
		if(m_lastError) {
			// Decryption error.
			disconnected();
			return;
		}

		// decrypt() should have called base::decode().
		break;
	}
	default:;
	}
}

/*!
 * \brief Pass decrypted data upstream.
 */
void Aes256BaseLayer::decodeDecrypted(void* buffer, size_t len)
{
	base::decode(buffer, len);
}

/*!
 * \brief Pass encrypted data downstream.
 */
void Aes256BaseLayer::encodeEncrypted(void const* buffer, size_t len, bool last)
{
	base::encode(buffer, len, last);
}

void Aes256BaseLayer::encode(void const* buffer, size_t len, bool last)
{
again:
	switch(m_encState) {
	case EncStateDisconnected:
		// Ignore data.
		return;
	case EncStateConnected:
		m_encState = EncStateReady;
		if(unified()) {
			if(m_decState == DecStateConnected)
				m_decState = DecStateReady;
			sendIV(true, false);
		} else {
			sendIV(false, false);
		}
		goto again;
	case EncStateReady: {
		// Encrypt data.
		if(len == 0) {
			// Nothing to do.
			return;
		}

		m_encState = EncStateEncoding;
		m_bufferLen = 0;
		STORED_FALLTHROUGH
	}
	case EncStateEncoding: {
		uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
		int res = 0;
		while(len && !res) {
			if(likely(m_bufferLen == 0)) {
				size_t chunk = len & ~((size_t)BlockSize - 1U);
				if(likely(chunk)) {
					// Full chunks to encrypt directly.
					res = encrypt(buffer_, chunk, false, unified());
					len -= chunk;
					buffer_ += chunk;
					continue;
				}
			}

			// We have partial data in the buffer.
			// Copy first.
			size_t copy = BlockSize - m_bufferLen;
			if(copy > len)
				copy = len;
			memcpy(m_buffer + m_bufferLen, buffer_, copy);
			m_bufferLen += copy;
			len -= copy;
			buffer_ += copy;
			if(m_bufferLen == BlockSize) {
				// Encrypt full buffer.
				res = encrypt(m_buffer, BlockSize, false, unified());
				m_bufferLen = 0;
				continue;
			}
		}

		if(!res && last) {
			// Finalize.
			stored_assert(m_bufferLen < BlockSize);

			// Add PKCS#7 padding.
			size_t padding = BlockSize - m_bufferLen % BlockSize;
			for(size_t i = m_bufferLen; i < BlockSize; ++i)
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				m_buffer[i] = static_cast<uint8_t>(padding);

			res = encrypt(m_buffer, BlockSize, true, unified());
			m_encState = EncStateReady;
		}

		if(res) {
			// Encryption error.
			m_lastError = res;

			if(last)
				base::encode(nullptr, 0, true);

			disconnected();
		}
		break;
	}
	default:;
	}
}

/*!
 * \brief Send out the initialization vector for encryption (so for decryption by the peer).
 */
void Aes256BaseLayer::sendIV(bool unified, bool last) noexcept
{
	uint8_t buf[BlockSize + 1];
	buf[0] = (uint8_t)(unified ? CmdUnified : CmdBidirectional);
	fillRandom(buf + 1, BlockSize);
	// Make sure not to wrap around the counter soon.
	buf[1] = (uint8_t)(buf[1] & 0x0fU);

	m_lastError = unified ? initUnified(m_key, buf + 1) : initEncrypt(m_key, buf + 1);
	if(m_lastError) {
		// Initialization error.
		disconnected();
		return;
	}

	base::encode(buf, sizeof(buf), last);
}

/*!
 * \brief Set the pre-shared key.
 *
 * Make sure to switch the key at the same time on both sides.
 */
void Aes256BaseLayer::setKey(void const* key) noexcept
{
	if(!key)
		memset(m_key, 0, KeySize);
	else
		memcpy(m_key, key, KeySize);

	if(m_encState != EncStateDisconnected)
		m_encState = EncStateConnected;
	if(unified() && m_decState != DecStateDisconnected)
		m_decState = DecStateConnected;
}

/*!
 * \brief Configure unified mode.
 */
void Aes256BaseLayer::unified(bool enable) noexcept
{
	m_unified = enable;

	stored_assert(m_encState != EncStateEncoding);

	if(m_encState != EncStateDisconnected)
		m_encState = EncStateConnected;
	if(m_decState != DecStateDisconnected)
		m_decState = DecStateConnected;
}

bool Aes256BaseLayer::unified() const noexcept
{
	return m_unified;
}

/*!
 * \brief Fill \p buffer with \p len pseudo-random bytes.
 */
void Aes256BaseLayer::fillRandom(uint8_t* buffer, size_t len) noexcept
{
	stored_assert(len == 0 || buffer);

	for(size_t i = 0; i < len; ++i) {
		buffer[i] = (uint8_t)
#  ifdef STORED_OS_POSIX
			rand_r(&m_seed);
#  else	 // !STORED_OS_POSIX
			rand();
#  endif // !STORED_OS_POSIX
	}
}



////////////////////////////////////////////////////////////////
// Aes256Layer using tiny-AES-c
//

#  if !AES256
#    error "AES256 not defined in aes.h"
#  endif

#  if !CTR
#    error "CTR not defined in aes.h"
#  endif

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Aes256Layer::Aes256Layer(void const* key, ProtocolLayer* up, ProtocolLayer* down)
	: base(key, up, down)
	, m_ctx_enc()
	, m_ctx_dec()
{
	static_assert(Aes256Layer::KeySize == AES_KEYLEN, "");
	static_assert(Aes256Layer::BlockSize == AES_BLOCKLEN, "");

	// NOLINTNEXTLINE
	m_ctx_enc = new struct AES_ctx;

	try {
		// NOLINTNEXTLINE
		m_ctx_dec = new struct AES_ctx;
	} catch(...) {
		delete static_cast<struct AES_ctx*>(m_ctx_enc);
		m_ctx_enc = nullptr;
		STORED_rethrow;
	}
}

Aes256Layer::~Aes256Layer()
{
	delete static_cast<struct AES_ctx*>(m_ctx_enc);
	delete static_cast<struct AES_ctx*>(m_ctx_dec);
}

int Aes256Layer::initEncrypt(uint8_t const* key, uint8_t const* iv) noexcept
{
	struct AES_ctx* ctx = static_cast<struct AES_ctx*>(m_ctx_enc);
	AES_init_ctx_iv(ctx, key, iv);
	return 0;
}

int Aes256Layer::initDecrypt(uint8_t const* key, uint8_t const* iv) noexcept
{
	struct AES_ctx* ctx = static_cast<struct AES_ctx*>(m_ctx_dec);
	AES_init_ctx_iv(ctx, key, iv);
	return 0;
}

int Aes256Layer::initUnified(uint8_t const* key, uint8_t const* iv) noexcept
{
	struct AES_ctx* ctx = static_cast<struct AES_ctx*>(m_ctx_uni);
	AES_init_ctx_iv(ctx, key, iv);
	return 0;
}

int Aes256Layer::updateUnified(uint8_t const* iv) noexcept
{
	struct AES_ctx* ctx = static_cast<struct AES_ctx*>(m_ctx_uni);
	AES_ctx_set_iv(ctx, iv);
	return 0;
}

int Aes256Layer::decrypt(uint8_t* buffer, size_t len, bool unified) noexcept
{
	if(!len)
		return 0;

	stored_assert(len % BlockSize == 0);
	stored_assert(buffer);
	AES_CTR_xcrypt_buffer(
		static_cast<struct AES_ctx*>(unified ? m_ctx_uni : m_ctx_dec), buffer, len);

	size_t padding = buffer[len - 1];
	if(padding == 0 || padding > BlockSize)
		// Invalid padding.
		return EINVAL;

	if(padding >= len)
		return 0;

	decodeDecrypted(buffer, len - padding);
	return 0;
}

int Aes256Layer::encrypt(uint8_t const* buffer, size_t len, bool last, bool unified) noexcept
{
	stored_assert(!len || buffer);
	stored_assert(len % BlockSize == 0);

	uint8_t buf[BlockSize];
	for(; len; len -= BlockSize, buffer += BlockSize) {
		memcpy(buf, buffer, BlockSize);
		AES_CTR_xcrypt_buffer(
			static_cast<struct AES_ctx*>(unified ? m_ctx_uni : m_ctx_enc), buf,
			BlockSize);
		encodeEncrypted(buf, BlockSize, last && len == BlockSize);
	}

	return 0;
}

} // namespace stored
#else  // !STORED_HAVE_AES
char dummy_char_to_make_aes_cpp_non_empty; // NOLINT
#endif // STORED_HAVE_AES
