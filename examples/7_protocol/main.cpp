/*!
 * \file
 * \brief Example with a stack of all default supplied protocol layers.
 *
 * This example simulates a lossy channel by generating random bit errors.  The
 * bit error rate can be configured using the \c ber store variable.  Moreover,
 * the MTU can also dynamically changed.
 *
 * Start this example using the \c stdio wrapper and connect the GUI to it.
 */

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

#include <cstdio>
#include <cinttypes>
#include <ctime>

static stored::ExampleProtocol store;

/*!
 * \brief Print a buffer, for demonstration purposes.
 */
static void printBuffer(void const* buffer, size_t len, char const* prefix = nullptr, FILE* f = stdout) {
	std::string s;
	if(prefix)
		s += prefix;

	uint8_t const* b = static_cast<uint8_t const*>(buffer);
	char buf[16];
	for(size_t i = 0; i < len; i++) {
		switch(b[i]) {
		case '\0': s += "\\0"; break;
		case '\r': s += "\\r"; break;
		case '\n': s += "\\n"; break;
		case '\t': s += "\\t"; break;
		case '\\': s += "\\\\"; break;
		default:
			if(b[i] < 0x20 || b[i] >= 0x7f) {
				snprintf(buf, sizeof(buf), "\\x%02" PRIx8, b[i]);
				s += buf;
			} else {
				s += (char)b[i];
			}
		}
	}

	s += "\n";
	fputs(s.c_str(), f);
}

/*!
 * \brief Simulate a lossy channel.
 *
 * Depending on the bit error rate (ber) set in the store, bits are flipped.
 * Moreover, it allows to set an MTU via the store.
 */
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

		printBuffer(buffer, len, "> ");
		base::decode(buffer, len);
	}

	virtual void encode(void const* buffer, size_t len, bool last = true) override {
		// cppcheck-suppress allocaCalled
		char* buffer_ = (char*)alloca(len);
		for(size_t i = 0; i < len; i++)
			buffer_[i] = lossyByte(static_cast<char const*>(buffer)[i]);

		printBuffer(buffer_, len, "< ");
		stored::TerminalLayer::writeToFd_(STDOUT_FILENO, buffer_, len);
		base::encode(buffer_, len, last);
	}

	using base::encode;

	// Bit error rate
	double ber() const { return store.ber; }

	char lossyByte(char b) {
		for(int i = 0; i < 8; i++) {
			double p =
#ifdef STORED_OS_WINDOWS
				(double)::rand() / RAND_MAX;
#else
				drand48();
#endif
			if(p < ber()) {
				// Inject an error.
				b = b ^ (char)(1 << (rand() % 8));
				store.injected_errors = store.injected_errors + 1;
			}
		}
		return b;
	}

	virtual size_t mtu() const override { return store.MTU.as<size_t>(); }
};

int main() {
	// Demonstrate a full stack assuming a lossy channel.
	// In this example, the lossy channel is simulated by LossyChannel,
	// which just flips bits, depending on the set bit error rate (BER).

	/*
	Consider the received string:
		\x1b_@Y?Ez\x7fI\x1b\\

	This is:
		\x1b_       TerminalLayer: start of message
		  @Y        ArqLayer: seq=89
		    ?       Debugger: capabilities
		  E         SegmentationLayer: last chunk
		  z\7fI     AsciiEscapeLayer: z<tab>
                      Crc16Layer: CRC=0x7a09
		\x1b\\      TerminalLayer: end of message


	To test, run in a shell:
	  echo -e -n '\x1b_\xc0X\xe4\x1c\x1b\\\x1b_@Y?Ez\x7fI\x1b\\' | 7_protocol

	*/

	printf("Demo of a lossy channel.\n");
	printf("Run this example using ed2.wrapper.stdio with the flag\n");
	printf("  -S segment,arq,crc16,ascii,term\n\n");

	stored::Debugger debugger;
	debugger.map(store);

	stored::SegmentationLayer segmentation;
	segmentation.wrap(debugger);

	stored::ArqLayer arq;
	arq.wrap(segmentation);

	stored::Crc16Layer crc;
	crc.wrap(arq);

	stored::AsciiEscapeLayer escape;
	escape.wrap(crc);

	stored::TerminalLayer terminal;
	terminal.wrap(escape);

	stored::BufferLayer buffer;
	buffer.wrap(terminal);

	LossyChannel lossy;
	lossy.wrap(buffer);

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

#ifdef STORED_OS_WINDOWS
	srand((unsigned int)time(NULL));
#else
	srand48((long)time(NULL));
#endif

	char buf[16];
	ssize_t len;
	do {
		len = read(STDIN_FILENO, buf, sizeof(buf));
		if(len > 0)
			lossy.decode(buf, (size_t)len);
	} while(len > 0);

	return 0;
}

