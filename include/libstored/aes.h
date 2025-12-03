#ifndef LIBSTORED_AES_H
#define LIBSTORED_AES_H
// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#  include <libstored/macros.h>

#  ifdef STORED_HAVE_AES
#    include <libstored/protocol.h>

namespace stored {

/*!
 * \brief A protocol layer that encrypts data using AES-256 in CTR mode.
 *
 * The pre-shared key should be provided using #setKey() before connecting.  Changing the key during
 * an active connection implies a reconnection.
 *
 * The first message after connecting is a random IV for decryption.  After that, both sides send
 * encrypted data. The last byte of the decoded data indicates the number of bytes to be stripped
 * off (including the last byte itself).
 *
 * This layer assumes that the data through the stack is properly framed.  For example, it runs on
 * top of a #stored::ZmqLayer, #stored::TerminalLayer, or #stored::ArqLayer.
 *
 * The IV is based on a pseudo-random number generator. Make sure to call \c srand() before.
 */
class Aes256BaseLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Aes256BaseLayer)
public:
	typedef ProtocolLayer base;

	static char const CmdReset = 'R';

	enum { KeySize = 32, BlockSize = 16 };

protected:
	explicit Aes256BaseLayer(
		void const* key = nullptr, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);

public:
	virtual ~Aes256BaseLayer() override is_default

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#    ifndef DOXYGEN
	using base::encode;
#    endif

	virtual bool flush() override;
	virtual void reset() override;
	virtual void connected() override;
	virtual void disconnected() override;
	int lastError() const noexcept;

	void setKey(void const* key) noexcept;
	void fillRandom(uint8_t* buffer, size_t len) noexcept;

protected:
	void sendIV() noexcept;

	/*!
	 * \brief Low-level initialization for #encrypt().
	 * \return 0 on success, otherwise an errno
	 */
	virtual int initEncrypt(uint8_t const* key, uint8_t const* iv) noexcept = 0;

	/*!
	 * \brief Low-level initialization for #decrypt().
	 * \return 0 on success, otherwise an errno
	 */
	virtual int initDecrypt(uint8_t const* key, uint8_t const* iv) noexcept = 0;

	/*!
	 * \brief Decrypt data in \p buffer.
	 * \param buffer the data to decrypt
	 * \param len the length of \p buffer
	 * \return 0 on success, otherwise an errno
	 *
	 * This function is expected to call #decodeDecrypted() with the decrypted data, without the
	 * padding bytes.  In-place decryption is allowed.
	 */
	virtual int decrypt(uint8_t* buffer, size_t len) noexcept = 0;

	void decodeDecrypted(void* buffer, size_t len);

	/*!
	 * \brief Encrypt data in \p buffer.
	 * \param buffer the data to encrypt
	 * \param len the length of \p buffer, which is always a multiple of #BlockSize
	 * \param last whether this is the last block of data
	 * \return 0 on success, otherwise an errno
	 *
	 * This function is expected to call #encodeEncrypted() with the encrypted data.  In-place
	 * encryption is not allowed.
	 */
	virtual int encrypt(uint8_t const* buffer, size_t len, bool last) noexcept = 0;

	void encodeEncrypted(void const* buffer, size_t len, bool last = true);

private:
	uint8_t m_key[KeySize];
	uint8_t m_buffer[BlockSize];
	size_t m_bufferLen;

	enum EncState
#    if STORED_cplusplus >= 201103L
		: uint8_t
#    endif
	{
		EncStateDisconnected,
		EncStateConnected,
		EncStateReady,
		EncStateEncoding,
	};
	EncState m_encState;

	enum DecState
#    if STORED_cplusplus >= 201103L
		: uint8_t
#    endif
	{
		DecStateDisconnected,
		DecStateConnected,
		DecStateReady,
	};
	DecState m_decState;

	int m_lastError;
#    ifdef STORED_OS_POSIX
	unsigned int m_seed;
#    endif // STORED_OS_POSIX
};

/*!
 * \brief A protocol layer that encrypts data using AES-256 in CTR mode using tiny-AES-c.
 *
 * This is a software implementation of AES-256. You may want to provide a hardware-accelerated
 * layer, if your platform supports that.
 */
class Aes256Layer : public Aes256BaseLayer {
	STORED_CLASS_NOCOPY(Aes256Layer)
public:
	typedef Aes256BaseLayer base;

	explicit Aes256Layer(
		void const* key = nullptr, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~Aes256Layer() override;

protected:
	virtual int initEncrypt(uint8_t const* key, uint8_t const* iv) noexcept override;
	virtual int initDecrypt(uint8_t const* key, uint8_t const* iv) noexcept override;
	virtual int decrypt(uint8_t* buffer, size_t len) noexcept override;
	virtual int encrypt(uint8_t const* buffer, size_t len, bool last) noexcept override;

private:
	void* m_ctx_enc;
	void* m_ctx_dec;
};

} // namespace stored
#  endif // STORED_HAVE_AES
#endif	 // __cplusplus
#endif	 // LIBSTORED_AES_H
