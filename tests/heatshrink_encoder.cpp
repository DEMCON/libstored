// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
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

int main()
{
	EchoLayer e;
	stored::CompressLayer c;
	c.wrap(e);

	stored::PrintLayer log(stderr);
	log.wrap(c);

	stored::StdioLayer stdio;
	stdio.wrap(log);

	while(stdio.recv() == 0)
		;

	e.encode();
	fflush(nullptr);
	return 0;
}
