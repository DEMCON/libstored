#include <stored>
#include "ZmqServerStore.h"

class ZmqServerStore : public stored::ZmqServerStoreBase<ZmqServerStore> {
	friend class stored::ZmqServerStoreBase<ZmqServerStore>;

public:
	ZmqServerStore() : m_messages() {}

protected:
	void __compute__an_int8_an_int16(bool set, int32_t& value) {
		if(!set)
			value = an_int8().get() + an_int16().get();
	}
	void __compute__circle_area_r___a_double(bool set, double& value) {
		if(!set)
			value = M_PI * a_double().get() * a_double().get();
	}
	void __compute__length_of___a_string(bool set, uint32_t& value) {
		if(!set)
			value = (uint32_t)strlen(static_cast<char*>(a_string().buffer()));

	}
	void __stats__ZMQ_messages(bool set, uint32_t& value) {
		if(!set)
			value = m_messages;
		else
			m_messages = value;
	}

	// TODO
//	void __stats__object_writes(bool set, uint32_t& value) {}
//	void __rand(bool set, double& value) {}
//	void __t_us(bool set, uint64_t& value) {}

public:
	void incMessages() {
		m_messages++;
	}

private:
	uint32_t m_messages;
};

int main() {
	ZmqServerStore store;
	stored::Debugger debugger("zmqserver");
	debugger.map(store);

	stored::ZmqLayer zmqLayer;
	zmqLayer.wrap(debugger);

	printf("Connect via ZMQ to debug this application.\n");

	while(true) {
		if(zmqLayer.recv(true) == 0)
			store.incMessages();
	}
}

