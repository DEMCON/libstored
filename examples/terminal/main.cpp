#include "ExampleTerminal.h"

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

#include <stdio.h>

// The default implementation emits the response to stdout, with APC / ST
// arround it. However, your normal terminal strips this out.  If you pipe the
// stdout to a file, you will see these sequences.
//
// If you enable the following line, this example dumps the response to stderr
// instead.
#define TERM_NO_ESCAPE

class CaseInverter : public stored::TerminalLayer {
public:
	typedef stored::TerminalLayer base;
	explicit CaseInverter(stored::Debugger* debugger = nullptr)
		: base(-1,
#ifdef TERM_NO_ESCAPE
			-1
#else
			STDOUT_FILENO
#endif
			, debugger)
	{}

	virtual ~CaseInverter() {}

protected:
	void nonDebugDecode(void* buffer, size_t len) override final {
		for(char* p = (char*)buffer; len > 0; len--, p++) {
			char c = *p;
			if(c >= 'a' && c <= 'z')
				putchar(c - 'a' + 'A');
			else if(c >= 'A' && c <= 'Z')
				putchar(c - 'A' + 'a');
			else
				putchar(c);
		}
	}

#ifdef TERM_NO_ESCAPE
	void encode(void* buffer, size_t len, bool last) override final {
		base::encode(buffer, len, last);
		encode((void const*)buffer, len, last);
	}

	void encode(void const* buffer, size_t len, bool last) override final {
		base::encode(buffer, len, last);
		writeToFd(STDERR_FILENO, buffer, len);
	}
#endif
};

int main() {
	stored::ExampleTerminal store;

	stored::Debugger debugger;
	debugger.map(store);

	CaseInverter phy;
	phy.wrap(debugger);

	printf("Terminal with out-of-band debug messages test\n\n");
	printf("To inject a command, enter `ESC %c <your command> ESC %c`.\n",
		stored::TerminalLayer::EscStart,
		stored::TerminalLayer::EscEnd);
	printf("If pressing ESC does not work, try pressing Ctrl+[ instead.\n");
	printf("All other input is considered part of the normal application stream,\n"
		"which is case-invered in this example.\n\n");

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	char buffer[16];
	ssize_t len;
	do {
		len = read(STDIN_FILENO, buffer, sizeof(buffer));
		if(len > 0)
			phy.decode(buffer, (size_t)len);
	} while(len > 0);

	return 0;
}

