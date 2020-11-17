#ifndef __LIBSTORED_COMPRESS_H
#define __LIBSTORED_COMPRESS_H
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

	class CompressLayer : public ProtocolLayer {
		CLASS_NOCOPY(CompressLayer)
	public:
		typedef ProtocolLayer base;

		enum {
			Window = 8,
			Lookahead = 4,
			DecodeInputBuffer = 32,

			FlagEncoding = 1,
			FlagDecoding = 2,
		};

		explicit CompressLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~CompressLayer() override;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;
		virtual size_t mtu() const override;

		void setPurgeableResponse(bool UNUSED_PAR(purgeable) = true) final {}

		bool idle() const;

	protected:
		void encoderPoll();
		void decoderPoll();

	private:
		void* m_encoder;
		void* m_decoder;
		std::vector<uint8_t> m_decodeBuffer;
		size_t m_decodeBufferSize;
		uint8_t m_state;
	};

} // namespace
#endif // STORED_HAVE_HEATSHRINK
#endif // __cplusplus
#endif // LIBSTORED_COMPRESS_H
