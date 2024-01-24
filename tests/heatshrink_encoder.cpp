// SPDX-FileCopyrightText: 2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <stored>

class EchoLayer : public stored::ProtocolLayer {
	STORED_CLASS_NOCOPY(EchoLayer)
public:
	typedef stored::ProtocolLayer base;

	EchoLayer(stored::ProtocolLayer* up = nullptr, stored::ProtocolLayer* down = nullptr)
		: base(up, down)
	{}

	virtual ~EchoLayer() override is_default

	virtual void decode(void* buffer, size_t len) override
	{
		encode(buffer, len, false);
	}
};

class StdioLayer : public stored::StdioLayer {
	STORED_CLASS_NOCOPY(StdioLayer)
public:
	typedef stored::StdioLayer base;

	StdioLayer(stored::ProtocolLayer& pipe)
		: base()
		, m_pipe(&pipe)
	{}

	virtual ~StdioLayer() override is_default

	virtual void close() override
	{
		// Before we close stdout, make sure we flush the encode pipe.

		if(m_pipe) {
			m_pipe->encode();
			m_pipe->flush();
		}

		m_pipe = nullptr;
	}

private:
	stored::ProtocolLayer* m_pipe;
};

int main()
{
	EchoLayer e;
	stored::CompressLayer c;
	stored::PrintLayer log(stderr);
	StdioLayer stdio(e);
	log.setUp(&e);
	e.setDown(&c);
	c.setDown(&log);
	stdio.wrap(log);

	while(stdio.recv() == 0)
		;

	fflush(nullptr);
	return 0;
}
