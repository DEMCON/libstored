// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/aes.h>

#ifdef STORED_HAVE_AES

extern "C" {
#  include <aes.h>
} // extern "C"

#  ifdef STORED_OS_POSIX
#    include <time.h>
#  endif // STORED_OS_POSIX


namespace stored {



////////////////////////////////////////////////////////////////
// Aes256BaseLayer
//

Aes256BaseLayer::Aes256BaseLayer(void const* key, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_key()
	, m_iv_enc()
	, m_iv_dec()
	, m_buffer()
	, m_bufferLen()
	, m_state(StateDisconnected)
	, m_lastError(ENOTCONN)
#  ifdef STORED_OS_POSIX
	// NOLINTNEXTLINE
	, m_seed((unsigned int)(uintptr_t)this ^ (unsigned int)time(nullptr))
#  endif // STORED_OS_POSIX
{
	if(key)
		setKey(key);
}

bool Aes256BaseLayer::flush()
{
	bool res = false;

	switch(m_state) {
	case StateConnected:
		sendIV();
		res = true;
		m_state = StateAwaitIV;
		break;
	case StateDisconnected:
	case StateAwaitIV:
	case StateReady:
	case StateEncoding:
	default:;
		// Nothing to do.
	}

	return base::flush() || res;
}

void Aes256BaseLayer::reset()
{
	m_state = StateDisconnected;
	m_lastError = ENOTCONN;
	base::reset();
}

void Aes256BaseLayer::connected()
{
	m_state = StateConnected;
	m_lastError = 0;
}

void Aes256BaseLayer::disconnected()
{
	m_state = StateDisconnected;
	m_lastError = ENOTCONN;
	base::disconnected();
}

void Aes256BaseLayer::decode(void* buffer, size_t len)
{
	uint8_t* buf = static_cast<uint8_t*>(buffer);

again:
	switch(m_state) {
	case StateDisconnected:
		// Ignore data.
		return;
	case StateConnected:
		m_state = StateAwaitIV;
		sendIV();
		goto again;
	case StateAwaitIV:
		// Expect IV.
		if(len != BlockSize + 1 || buf[0] != CmdReset) {
			// Invalid command.
			m_lastError = EINVAL;
			return;
		}

		memcpy(m_iv_dec, buf + 1, BlockSize);
		if((m_lastError = init(m_key, m_iv_enc, m_iv_dec)) != 0) {
			// Initialization error.
			m_state = StateDisconnected;
			return;
		}

		m_state = StateReady;
		base::connected();
		break;
	case StateReady:
	case StateEncoding: {
		// Decrypt data.
		if(len == 0) {
			// Nothing to do.
			return;
		}
		if(len == BlockSize + 1 && buf[0] == CmdReset) {
			// Re-initialization.
			m_state = StateConnected;
			goto again;
		}
		if(len % BlockSize != 0) {
			// Invalid block.
			m_lastError = EINVAL;
			m_state = StateDisconnected;
			base::disconnected();
			return;
		}

		if((m_lastError = decrypt(buf, len)) != 0) {
			// Decryption error.
			m_state = StateDisconnected;
			base::disconnected();
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
	switch(m_state) {
	case StateDisconnected:
		// Ignore data.
		return;
	case StateConnected:
		m_state = StateAwaitIV;
		sendIV();
		goto again;
	case StateAwaitIV:
		// Can't send data before IV exchange.
		m_lastError = EINVAL;
		return;
	case StateReady: {
		// Encrypt data.
		if(len == 0) {
			// Nothing to do.
			return;
		}

		m_state = StateEncoding;
		m_bufferLen = 0;
		STORED_FALLTHROUGH
	}
	case StateEncoding: {
		uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
		int res = 0;
		while(len && !res) {
			if(likely(m_bufferLen == 0)) {
				size_t chunk = len & ~(BlockSize - 1);
				if(likely(chunk)) {
					// Full chunks to encrypt directly.
					res = encrypt(buffer_, chunk, false);
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
				res = encrypt(m_buffer, BlockSize, false);
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
				m_buffer[i] = static_cast<uint8_t>(padding);

			res = encrypt(m_buffer, BlockSize, true);
			m_state = StateReady;
		}

		if(res) {
			// Encryption error.
			m_lastError = res;
			m_state = StateDisconnected;
			base::disconnected();

			if(last)
				base::encode(nullptr, 0, true);
		}
		break;
	}
	default:;
	}
}

/*!
 * \brief Send out the initialization vector for encryption (so for decryption by the peer).
 */
void Aes256BaseLayer::sendIV() noexcept
{
	fillRandom(m_iv_enc, BlockSize);
	uint8_t cmd = (uint8_t)CmdReset;
	base::encode(&cmd, 1, false);
	base::encode(m_iv_enc, BlockSize, true);
}

/*!
 * \brief Set the pre-shared key.
 *
 * Terminates the current connection if any.
 */
void Aes256BaseLayer::setKey(void const* key)
{
	stored_assert(key);

	memcpy(m_key, key, KeySize);

	switch(m_state) {
	case StateDisconnected:
		// Nothing to do.
		break;
	case StateReady:
	case StateEncoding:
		m_state = StateDisconnected;
		base::disconnected();
		STORED_FALLTHROUGH
	case StateConnected:
	case StateAwaitIV:
	default:
		sendIV();
		m_state = StateAwaitIV;
		break;
	}
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

static_assert(Aes256Layer::KeySize == AES_KEYLEN, "");

Aes256Layer::Aes256Layer(void const* key, ProtocolLayer* up, ProtocolLayer* down)
	: base(key, up, down)
	, m_ctx_enc()
	, m_ctx_dec()
{
	m_ctx_enc = new struct AES_ctx;
	m_ctx_dec = new struct AES_ctx;
}

Aes256Layer::~Aes256Layer()
{
	delete static_cast<struct AES_ctx*>(m_ctx_enc);
	delete static_cast<struct AES_ctx*>(m_ctx_dec);
}

int Aes256Layer::init(uint8_t const* key, uint8_t const* iv_enc, uint8_t const* iv_dec) noexcept
{
	AES_init_ctx_iv(static_cast<struct AES_ctx*>(m_ctx_enc), key, iv_enc);
	AES_init_ctx_iv(static_cast<struct AES_ctx*>(m_ctx_dec), key, iv_dec);
	return 0;
}

int Aes256Layer::decrypt(uint8_t* buffer, size_t len) noexcept
{
	if(!len)
		return 0;

	stored_assert(len % BlockSize == 0);
	stored_assert(buffer);
	AES_CTR_xcrypt_buffer(static_cast<struct AES_ctx*>(m_ctx_dec), buffer, len);

	size_t padding = buffer[len - 1];
	if(padding >= len)
		return 0;

	decodeDecrypted(buffer, len - padding);
	return 0;
}

int Aes256Layer::encrypt(uint8_t const* buffer, size_t len, bool last) noexcept
{
	stored_assert(!len || buffer);
	stored_assert(len % BlockSize == 0);

	uint8_t buf[BlockSize];
	for(size_t offset = 0; offset < len; offset += BlockSize) {
		memcpy(buf, buffer + offset, BlockSize);
		AES_CTR_xcrypt_buffer(static_cast<struct AES_ctx*>(m_ctx_enc), buf, BlockSize);
		encodeEncrypted(buf, BlockSize, false);
	}

	if(last)
		encodeEncrypted(nullptr, 0, last);

	return 0;
}

} // namespace stored
#else  // !STORED_HAVE_AES
char dummy_char_to_make_aes_cpp_non_empty; // NOLINT
#endif // STORED_HAVE_AES
