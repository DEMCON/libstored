#ifndef __LIBSTORED_PROTOCOL_H
#define __LIBSTORED_PROTOCOL_H
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

/*!
 * \defgroup libstored_protocol protocol
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <vector>

namespace stored {

	/*!
	 * \ingroup libstored_protocol
	 */
	class ProtocolLayer {
	public:
		explicit ProtocolLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
			: m_up(up), m_down(down)
		{
		}

		virtual ~ProtocolLayer();

		void setUp(ProtocolLayer* up) { m_up = up; }
		void setDown(ProtocolLayer* down) { m_down = down; }

		void wrap(ProtocolLayer& up) {
			ProtocolLayer* d = up.down();

			setDown(d);
			if(d)
				d->setUp(this);

			up.setDown(this);
			setUp(&up);
		}

		void stack(ProtocolLayer& down) {
			ProtocolLayer* u = down.up();

			setUp(u);
			if(u)
				u->setDown(this);

			down.setUp(this);
			setDown(&down);
		}

		ProtocolLayer* up() const { return m_up; }
		ProtocolLayer* down() const { return m_down; }

		virtual void decode(void* buffer, size_t len) {
			if(up())
				up()->decode(buffer, len);
		}

		void encode() {
			encode((void const*)nullptr, 0, true);
		}

		virtual void encode(void* buffer, size_t len, bool last = true) {
			if(down())
				down()->encode(buffer, len, last);
		}

		virtual void encode(void const* buffer, size_t len, bool last = true) {
			if(down())
				down()->encode(buffer, len, last);
		}

#if __cplusplus >= 201103L
	public:
		ProtocolLayer(ProtocolLayer const&) = delete;
		ProtocolLayer(ProtocolLayer&&) = delete;
		void operator=(ProtocolLayer const&) = delete;
		void operator=(ProtocolLayer&&) = delete;
#else
	private:
		ProtocolLayer(ProtocolLayer const&);
		void operator=(ProtocolLayer const&);
#endif

	private:
		ProtocolLayer* m_up;
		ProtocolLayer* m_down;
	};

	/*!
	 * \ingroup libstored_protocol
	 */
	class TerminalLayer : public ProtocolLayer {
	public:
		typedef ProtocolLayer base;

		static char const Esc      = '\x1b'; // ESC
		static char const EscStart = '_';    // APC
		static char const EscEnd   = '\\';   // ST
		enum { MaxBuffer = 1024 };

		explicit TerminalLayer(int nonDebugDecodeFd = -1, int encodeFd = -1, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~TerminalLayer() override;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void* buffer, size_t len, bool last = true) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;

	protected:
		virtual void nonDebugDecode(void* buffer, size_t len);
		void encodeStart();
		void encodeEnd();

		void writeToFd(int fd, void const* buffer, size_t len);

#if __cplusplus >= 201103L
	public:
		TerminalLayer(TerminalLayer const&) = delete;
		TerminalLayer(TerminalLayer&&) = delete;
		void operator=(TerminalLayer const&) = delete;
		void operator=(TerminalLayer&&) = delete;
#else
	private:
		TerminalLayer(TerminalLayer const&);
		void operator=(TerminalLayer const&);
#endif

	private:
		int m_nonDebugDecodeFd;
		int m_encodeFd;

		enum State { StateNormal, StateNormalEsc, StateDebug, StateDebugEsc };
		State m_decodeState;
		std::vector<char> m_buffer;

		bool m_encodeState;
	};


} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_PROTOCOL_H

