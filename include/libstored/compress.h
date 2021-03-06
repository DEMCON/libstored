#ifndef LIBSTORED_COMPRESS_H
#define LIBSTORED_COMPRESS_H
/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
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

#include <libstored/macros.h>
#include <libstored/protocol.h>

#ifdef __cplusplus
#ifndef STORED_HAVE_HEATSHRINK
namespace stored {
	// No compression available, just use a pass-through.
	typedef ProtocolLayer CompressLayer;
}
#else // STORED_HAVE_HEATSHRINK

#include <vector>

namespace stored {

	/*!
	 * \brief Compress/decompress streams.
	 *
	 * The compress layer uses heatshrink for compression.  It is a
	 * general-purpose algorithm, which is not the best compression, and not
	 * also not the fastest, but has a limited memory usage and allows streams,
	 * which makes is appropriate for embedded systems.
	 *
	 * Compression works best on longer streams, but this layer works per
	 * message. So, although it may be stacked in any protocol stack, the
	 * compression ratio may be limited. It is nicely used in stored::Stream,
	 * where it compresses a full stream (not separate messages), which are
	 * sent in chunks to the other side.
	 *
	 * When heatshrink is not available, this layer is just a pass-through.
	 */
	class CompressLayer : public ProtocolLayer {
		CLASS_NOCOPY(CompressLayer)
	public:
		typedef ProtocolLayer base;

		enum {
			/*! \brief Window size. See heatshrink documentation. */
			Window = 8,
			/*! \brief Lookahead. See heatshrink documentation. */
			Lookahead = 4,
			/*! \brief Include buffer size in bytes. See heatshrink documentation. */
			DecodeInputBuffer = 32,

			/*! \brief Flag for \c m_state to indicate an active encoder. */
			FlagEncoding = 1,
			/*! \brief Flag for \c m_state to indicate an active decoder. */
			FlagDecoding = 2,
		};

		explicit CompressLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~CompressLayer() override;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
#ifndef DOXYGEN
		using base::encode;
#endif
		virtual size_t mtu() const override;

		void setPurgeableResponse(bool UNUSED_PAR(purgeable) = true) final {}

		bool idle() const;

	protected:
		void encoderPoll();
		void decoderPoll();

	private:
		/*!
		 * \brief The encoder.
		 * \details This a \c heatshrink_encoder*. Allocated when required.
		 * */
		void* m_encoder;
		/*!
		 * \brief The decoder.
		 * \details This a \c heatshrink_decoder*. Allocated when required.
		 * */
		void* m_decoder;
		/*!
		 * \brief Fully decoded message.
		 * \details This is used to pass a buffer upstream.
		 *          The buffer is only allocated, not released.
		 */
		std::vector<uint8_t> m_decodeBuffer;
		/*!
		 * \brief Actual data in #m_decodeBuffer.
		 */
		size_t m_decodeBufferSize;
		/*!
		 * \brief Current state of the encoder and decoder.
		 */
		uint8_t m_state;
	};

} // namespace
#endif // STORED_HAVE_HEATSHRINK
#endif // __cplusplus
#endif // LIBSTORED_COMPRESS_H
