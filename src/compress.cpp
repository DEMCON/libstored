// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/macros.h>
#include <libstored/compress.h>

#ifdef STORED_HAVE_HEATSHRINK
extern "C" {
#	include <heatshrink_decoder.h>
#	include <heatshrink_encoder.h>
}

/*!
 * \brief Helper to get a \c heatshrink_encoder reference from \c CompressLayer::m_encoder.
 */
static heatshrink_encoder& encoder_(void* e)
{
	stored_assert(e);
	return *static_cast<heatshrink_encoder*>(e);
}
#	define encoder() (encoder_(m_encoder)) // NOLINT(cppcoreguidelines-macro-usage)

/*!
 * \brief Helper to get a \c heatshrink_decoder reference from \c CompressLayer::m_decoder.
 */
static heatshrink_decoder& decoder_(void* d)
{
	stored_assert(d);
	return *static_cast<heatshrink_decoder*>(d);
}
#	define decoder() (decoder_(m_decoder)) // NOLINT(cppcoreguidelines-macro-usage)

namespace stored {

/*!
 * \brief Ctor.
 */
CompressLayer::CompressLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_encoder()
	, m_decoder()
	, m_decodeBufferSize()
	, m_state()
{}

/*!
 * \brief Dtor.
 */
CompressLayer::~CompressLayer()
{
	if(m_encoder)
		heatshrink_encoder_free(&encoder());

	if(m_decoder)
		heatshrink_decoder_free(&decoder());
}

void CompressLayer::decode(void* buffer, size_t len)
{
	stored_assert(len == 0 || buffer);

	if(!buffer || !len)
		return;

	if(unlikely(!m_decoder)) {
		m_decoder = heatshrink_decoder_alloc(DecodeInputBuffer, Window, Lookahead);
		if(!m_decoder) {
#	ifdef STORED_cpp_exceptions
			throw std::bad_alloc();
#	else
			std::terminate();
#	endif
		}
	}

	m_state |= (uint8_t)FlagDecoding;

	m_decodeBufferSize = 0;

	uint8_t* in_buf = static_cast<uint8_t*>(buffer);
	while(len > 0) {
		size_t sunk = 0;
		HSD_sink_res res = heatshrink_decoder_sink(&decoder(), in_buf, len, &sunk);
		stored_assert(res == HSDR_SINK_OK);
		(void)res;
		stored_assert(sunk <= len);
		len -= sunk;
		in_buf += sunk;

		decoderPoll();
	}

	while(heatshrink_decoder_finish(&decoder()) == HSDR_FINISH_MORE)
		decoderPoll();

	base::decode(m_decodeBuffer.data(), m_decodeBufferSize);
	m_state &= (uint8_t) ~(uint8_t)FlagDecoding;
}

/*!
 * \brief Check if there is data to be extracted from the decoder.
 */
void CompressLayer::decoderPoll()
{
	while(true) {
		m_decodeBuffer.resize(m_decodeBufferSize + 128);
		size_t output_size = 0;
		HSD_poll_res res = heatshrink_decoder_poll(
			&decoder(), &m_decodeBuffer[m_decodeBufferSize],
			m_decodeBuffer.size() - m_decodeBufferSize, &output_size);

		m_decodeBufferSize += output_size;
		stored_assert(m_decodeBufferSize <= m_decodeBuffer.size());

		switch(res) {
		case HSDR_POLL_EMPTY:
			return;
		case HSDR_POLL_MORE:
			break;
		case HSDR_POLL_ERROR_NULL:
		case HSDR_POLL_ERROR_UNKNOWN:
		default:
			// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
			stored_assert(false);
		}
	}
}

void CompressLayer::encode(void const* buffer, size_t len, bool last)
{
	stored_assert(len == 0 || buffer);

	if(unlikely(!m_encoder)) {
		m_encoder = heatshrink_encoder_alloc(Window, Lookahead);
		if(!m_encoder) {
#	ifdef STORED_cpp_exceptions
			throw std::bad_alloc();
#	else
			std::terminate();
#	endif
		}
	}

	m_state |= (uint8_t)FlagEncoding;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	uint8_t* in_buf = (uint8_t*)buffer;

	while(len > 0) {
		size_t sunk = 0;
		HSE_sink_res res = heatshrink_encoder_sink(&encoder(), in_buf, len, &sunk);
		stored_assert(res == HSER_SINK_OK);
		(void)res;
		stored_assert(sunk <= len);
		len -= sunk;
		in_buf += sunk;

		encoderPoll();
	}

	if(last) {
		while(heatshrink_encoder_finish(&encoder()) == HSER_FINISH_MORE)
			encoderPoll();
		base::encode(nullptr, 0, true);
		heatshrink_encoder_reset(&encoder());
		m_state &= (uint8_t) ~(uint8_t)FlagEncoding;
	}
}

/*!
 * \brief Check if there is data to be extracted from the encoder.
 */
void CompressLayer::encoderPoll()
{
	uint8_t out_buf[128];

	while(true) {
		size_t output_size = 0;
		HSE_poll_res res =
			heatshrink_encoder_poll(&encoder(), out_buf, sizeof(out_buf), &output_size);

		if(output_size > 0)
			base::encode(out_buf, output_size, false);

		switch(res) {
		case HSER_POLL_EMPTY:
			return;
		case HSER_POLL_MORE:
			break;
		case HSER_POLL_ERROR_NULL:
		case HSER_POLL_ERROR_MISUSE:
		default:
			// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
			stored_assert(false);
		}
	}
}

size_t CompressLayer::mtu() const
{
	// This is a stream; we cannot handle limited messages.
	// Use the SegmentationLayer for that.
	stored_assert(base::mtu() == 0);
	return 0;
}

/*!
 * \brief Check if the encoder and decoder are both in idle state.
 * \return \c true if there is no data stuck in any internal buffer.
 */
bool CompressLayer::idle() const
{
	return m_state == 0;
}

} // namespace stored
#else  // !STORED_HAVE_HEATSHRINK
char dummy_char_to_make_compress_cpp_non_empty; // NOLINT
#endif // STORED_HAVE_HEATSHRINK
