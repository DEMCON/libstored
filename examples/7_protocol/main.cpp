#include "ExampleProtocol.h"

#include <stored>

#ifdef STORED_COMPILER_MSVC
#  include <io.h>
#  define read(fd, buffer, len) _read(fd, buffer, (unsigned int)(len))
#  define STDERR_FILENO		_fileno(stderr)
#  define STDOUT_FILENO		_fileno(stdout)
#  define STDIN_FILENO		_fileno(stdin)
#else
#  include <unistd.h>
#endif

#ifdef STORED_OS_WINDOWS
#  include <malloc.h>
#  ifndef alloca
#    define alloca(x) _alloca(x)
#  endif
#else
#  include <alloca.h>
#endif

#include <stdio.h>

static stored::ExampleProtocol store;

class LossyChannel : public stored::ProtocolLayer {
	CLASS_NOCOPY(LossyChannel)
public:
	typedef stored::ProtocolLayer base;

	LossyChannel(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
	{
	}

	virtual ~LossyChannel() override is_default

	virtual void decode(void* buffer, size_t len) override {
		char* buffer_ = static_cast<char*>(buffer);
		for(size_t i = 0; i < len; i++)
			buffer_[i] = lossyByte(buffer_[i]);
		base::decode(buffer, len);
	}

	virtual void encode(void const* buffer, size_t len, bool last = true) override {
		// cppcheck-suppress allocaCalled
		char* buffer_ = (char*)alloca(len);
		for(size_t i = 0; i < len; i++)
			buffer_[i] = lossyByte(static_cast<char const*>(buffer)[i]);

		stored::TerminalLayer::writeToFd_(STDOUT_FILENO, buffer_, len);
		base::encode(buffer_, len, last);
	}

	using base::encode;

	// Byte error rate
	double ber() const { return store.ber; }

	char lossyByte(char b) {
		while(true) {
			double p =
#ifdef STORED_OS_WINDOWS
				(double)::rand() / RAND_MAX;
#else
				drand48();
#endif
			if(p >= ber())
				return b;

			// Inject an error.
			b = b ^ (char)(1 << (rand() % 8));
			store.injected_errors = store.injected_errors + 1;
		}
	}

	virtual size_t mtu() const override { return store.MTU.as<size_t>(); }
};

int main() {
	// Demonstrate a full stack assuming a lossy channel.
	// In this example, the lossy channel is simulated by LossyChannel,
	// which just flips bits, depending on the set bit error rate (BER).

	stored::Debugger debugger;
	debugger.map(store);

	stored::SegmentationLayer segmentation;
	segmentation.wrap(debugger);

	stored::ArqLayer arq;
	arq.wrap(segmentation);

	stored::CrcLayer crc;
	crc.wrap(arq);

	stored::AsciiEscapeLayer escape;
	escape.wrap(crc);

	stored::TerminalLayer terminal;
	terminal.wrap(escape);

	LossyChannel lossy;
	lossy.wrap(terminal);

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	char buffer[16];
	ssize_t len;
	do {
		len = read(STDIN_FILENO, buffer, sizeof(buffer));
		if(len > 0)
			lossy.decode(buffer, (size_t)len);
	} while(len > 0);

	return 0;
}

