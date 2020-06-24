#include "TestStore.h"
#include "gtest/gtest.h"

namespace {

class FunctionTestStore : public stored::TestStoreBase<FunctionTestStore> {
	friend class stored::TestStoreBase<FunctionTestStore>;

public:
	FunctionTestStore() : m_f_read__write(4) {}

protected:

	void __f_read__write(bool set, double& value) {
		if(set)
			m_f_read__write = value;
		else
			value = m_f_read__write;
	}

	void __f_read_only(bool set, uint16_t& value) {
		if(!set)
			value = saturated_cast<uint16_t>(m_f_read__write);
	}

	size_t __f_write_only(bool set, char* buffer, size_t len) {
		if(!set)
			return 0;

		printf("f write-only: %*s\n", (int)len, buffer);
		return len;
	}

private:
	double m_f_read__write;
};

TEST(Function, ReadWrite) {
	FunctionTestStore store;
	EXPECT_DOUBLE_EQ(store.f_read__write()(), 4.0);
	store.f_read__write()(5.0);
	EXPECT_DOUBLE_EQ(store.f_read__write()(), 5.0);
}

TEST(Function, ReadOnly) {
	FunctionTestStore store;
	EXPECT_EQ(store.f_read_only()(), 4u);
	store.f_read__write()(5.6);
	EXPECT_EQ(store.f_read_only()(), 6u);
}

TEST(Function, WriteOnly) {
	FunctionTestStore store;
	char buffer[] = "hello";
	EXPECT_EQ(store.f_write_only().get(buffer, sizeof(buffer)), 0);
	EXPECT_EQ(store.f_write_only().set(buffer, strlen(buffer)), 5u);
}

} // namespace

