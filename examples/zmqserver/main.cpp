#include <stored>
#include "ZmqServerStore.h"

#if __cplusplus >= 201103L
#  include <chrono>
#endif

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

	void __stats__object_writes(bool set, uint32_t& value) {
		if(!set)
			value = m_writes;
	}

	void __rand(bool set, double& value) {
		if(!set) {
#ifdef STORED_OS_WINDOWS
			value = (double)rand() / RAND_MAX;
#else
			value = drand48();
#endif
		}
	}
	void __t_us(bool set, uint64_t& value) {
		if(!set) {
#if __cplusplus >= 201103L
			value = (uint64_t)(std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
#else
			// There is no portable implementation here...
			value = (uint64_t)time() * 1000000ULL;
#endif
		}
	}

	void __hookSet(stored::Type::type UNUSED_PAR(type), void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len)) { m_writes++; }

public:
	void incMessages() {
		m_messages++;
	}

private:
	uint32_t m_messages;
	uint32_t m_writes;
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

