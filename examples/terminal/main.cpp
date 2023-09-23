// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief A stdin/stdout terminal application to test hand-injected Embedded Debugger messages.
 */

#include "ExampleTerminal.h"

#include <stored>

#ifndef STORED_OS_WINDOWS
// The default implementation emits the response to stdout, with APC / ST
// around it. However, your normal terminal strips this out.  If you pipe the
// stdout to a file, you will see these sequences.
//
// If you enable the following line, this example dumps the response to stderr too.
#	define PRINT_TO_STDERR
#endif

// If defined, do not print escaped messages to the console, but silently drop them.
//#define SUPPRESS_ESCAPE

class CaseInverter : public stored::TerminalLayer {
public:
	typedef stored::TerminalLayer base;
	CaseInverter() is_default
	virtual ~CaseInverter() override is_default

protected:
	void nonDebugDecode(void* buffer, size_t len) final
	{
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

#ifdef PRINT_TO_STDERR
	void encode(void const* buffer, size_t len, bool last) final
	{
		base::encode(buffer, len, last);
		fwrite(buffer, len, 1, stderr);
	}
#endif
};

int main()
{
	stored::ExampleTerminal store;

	stored::Debugger debugger;
	debugger.map(store);

	stored::AsciiEscapeLayer escape;
	escape.wrap(debugger);

	CaseInverter ci;
	ci.wrap(escape);

	stored::StdioLayer stdio;
	stdio.wrap(ci);

	printf("Terminal with out-of-band debug messages test\n\n");
	printf("To inject a command, enter `ESC %c <your command> ESC %c`.\n",
	       stored::TerminalLayer::EscStart, stored::TerminalLayer::EscEnd);
	printf("If pressing ESC does not work, try pressing Ctrl+[ instead.\n");
	printf("All other input is considered part of the normal application stream,\n"
	       "which is case-inverted in this example.\n\n");

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	stored::Poller poller;
	stored::PollableFileLayer pollable(stdio, stored::Pollable::PollIn);
	if((errno = poller.add(pollable))) {
		perror("Cannot add pollable");
		return 1;
	}

	while(stdio.isOpen()) {
		poller.poll();
		stdio.recv();
	}

	return 0;
}
