#ifndef __LIBSTORED_PROTOCOL_H
#define __LIBSTORED_PROTOCOL_H

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <vector>

namespace stored {

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
	private:
		ProtocolLayer* m_up;
		ProtocolLayer* m_down;
	};

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

