/*!
 * \file
 * \brief Config for tests
 */

#ifndef LIBSTORED_CONFIG_H
#	error Do not include this file directly, include <stored> instead.
#endif

#ifndef STORED_CONFIG_H
#	define STORED_CONFIG_H

#	ifdef __cplusplus
#		include <cstdio>
#		include <cstdlib>
#		include <functional>
#		include <typeinfo>

class TestAllocatorBase {
public:
	struct Stats {
		size_t calls = 0;
		size_t objects = 0;
		size_t total = 0;
	};

	static Stats allocate_stats;
	static Stats deallocate_stats;

	static void allocate_report(std::type_info const& t, void* p, size_t size, size_t n)
	{
		if(n == 1U)
			printf("Allocated %s at %p\n", t.name(), p);
		else
			printf("Allocated %s[%zu] at %p\n", t.name(), n, p);

		allocate_stats.calls++;
		allocate_stats.objects += n;
		allocate_stats.total += size * n;
	}

	static std::function<void(std::type_info const&, void*, size_t, size_t)> allocate_cb;

	static void deallocate_report(std::type_info const& t, void* p, size_t size, size_t n)
	{
		if(n == 1U)
			printf("Deallocate %s at %p\n", t.name(), p);
		else
			printf("Deallocate %s[%zu] at %p\n", t.name(), n, p);

		deallocate_stats.calls++;
		deallocate_stats.objects += n;
		deallocate_stats.total += size * n;
	}

	static std::function<void(std::type_info const&, void*, size_t, size_t)> deallocate_cb;
};

template <typename T>
class TestAllocator : public TestAllocatorBase {
public:
	using value_type = T;

	TestAllocator() noexcept = default;

	template <typename A>
	TestAllocator(TestAllocator<A> const&) noexcept
	{}

	template <typename A>
	TestAllocator(TestAllocator<A>&&) noexcept
	{}

	value_type* allocate(size_t n)
	{
		value_type* p = (value_type*)malloc(sizeof(value_type) * n);
		if(!p)
			throw std::bad_alloc();

		if(allocate_cb)
			allocate_cb(typeid(value_type), p, sizeof(value_type), n);

		return p;
	}

	void deallocate(value_type* p, size_t n) noexcept
	{
		if(deallocate_cb)
			deallocate_cb(typeid(value_type), p, sizeof(value_type), n);

		free(p);
	}

	constexpr bool operator==(TestAllocator&) noexcept
	{
		return true;
	}
	constexpr bool operator!=(TestAllocator&) noexcept
	{
		return false;
	}
};

namespace stored {
struct Config : public DefaultConfig {
	template <typename T>
	struct Allocator {
		typedef TestAllocator<T> type;
	};
};
} // namespace stored
#	endif // __cplusplus
#endif	       // STORED_CONFIG_H
