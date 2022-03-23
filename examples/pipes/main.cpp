/*!
 * \file
 * \brief Pipes example.
 */

#include "ExamplePipes.h"

#include <algorithm>
#include <ratio>
#include <stored>

void simple_pipe()
{
	using namespace stored::pipes;

	auto p = Entry<int>{} >> Buffer<int>{2} >> Exit{};

	printf("extract p: %d\n", (int)p.extract());

	3 >> p;
	printf("extract p: %d\n", (int)p.extract());
}

template <typename T>
struct Quantity {
	using type = T;

	explicit Quantity(type value_ = type{}, std::string unit_ = std::string{}, type ratio_ = 1)
		: value{value_}
		, unit{std::move(unit_)}
		, ratio{ratio_}
	{}

	type value = type{};
	std::string unit;
	type ratio = 1;

	type get() const
	{
		return value * ratio;
	}

	operator type() const
	{
		return get();
	}
};

template <typename Quantity>
class Quantified {
public:
	using type = typename Quantity::type;

	explicit Quantified(Quantity quantity)
		: m_quantity{std::move(quantity)}
	{}

	Quantity inject(type x)
	{
		return exit_cast(x);
	}

	Quantity exit_cast(type x) const
	{
		auto q = m_quantity;
		q.value = x;
		return q;
	}

private:
	Quantity m_quantity;
};

#if STORED_cplusplus >= 201703L
void measurement_pipe17()
{
	using namespace stored::pipes;

	stored::ExamplePipes store;

	auto view = Entry<double>{} >> Quantified{Quantity<double>{0, "km", 0.001}}
		    >> Call{[](Quantity<double> const& q) {
			      printf("changed %g %s\n", q.get(), q.unit.c_str());
		      }}
		    >> Buffer<Quantity<double>>{} >> Exit{};
	auto p = Entry<bool>{} >> Get{store.sensor} >> Cast<float, double>{}
		 >> Changes{view, similar_to<double>{}} >> Exit{};

	store.sensor = 1.2f;
	printf("sensor = %g\n", (double)p.extract());
	auto v = view.extract().get();
	printf("sensor view = %g %s\n", v.get(), v.unit.c_str());

	store.sensor = 1.3f;
	true >> p;
	store.sensor = 1.3001f;
	true >> p;
	v = view.extract();
	printf("sensor = %g\n", (double)p.extract());
	printf("sensor view = %g %s\n", v.get(), v.unit.c_str());
}

template <typename T>
struct Bounded {
	using type = T;

	explicit Bounded(
		type low_ = std::numeric_limits<type>::lowest(),
		type high_ = std::numeric_limits<type>::max())
		: low{low_}
		, high{high_}
	{}

	type low = std::numeric_limits<type>::lowest();
	type high = std::numeric_limits<type>::max();

	type check(type x) const
	{
		return std::min(high, std::max(low, x));
	}
};

template <typename T>
class Constrained {
public:
	using type = T;

	explicit Constrained(Bounded<type> bq)
		: m_bq{std::move(bq)}
	{}

	type inject(type x)
	{
		return exit_cast(x);
	}

	type exit_cast(type x) const
	{
		return m_bq.check(x);
	}

private:
	Bounded<type> m_bq;
};

void setpoint_pipe17()
{
	using namespace stored::pipes;

	stored::ExamplePipes store;

	auto p = Entry<double>{} >> Constrained{Bounded{0.0, 10.0}} >> Cast<double, float>{}
		 >> Set{store.setpoint} >> Exit{};

	2.1 >> p;
	printf("setpoint = %g\n", (double)store.setpoint);

	11.3 >> p;
	printf("setpoint = %g\n", (double)store.setpoint);
}
#endif // C++17

int main()
{
	simple_pipe();
#if STORED_cplusplus >= 201703L
	measurement_pipe17();
	setpoint_pipe17();
#endif // C++17
}
