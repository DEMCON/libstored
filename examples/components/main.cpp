#include "ExampleComponents.h"

#include <libstored/components.h>

#include <iostream>

int main()
{
	constexpr auto n1 = stored::FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i'>::create("/amp/");
	static_assert(n1.size() == 1, "");
	static_assert(n1.validSize() == 1, "");

	// j is not valid
	constexpr auto n2 = stored::FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i', 'j'>::create("/amp/");
	static_assert(n2.size() == 2, "");
	static_assert(n2.validSize() == 1, "");

	// find by long name
	constexpr auto n3 = stored::FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i', 'o'>::create("/amp/", "input", "output");
	static_assert(n3.size() == 2, "");
	static_assert(n3.validSize() == 2, "");

	// skip o
	constexpr auto n4 = stored::FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i', 'o'>::create<'i'>("/amp/", "input", "output");
	static_assert(n4.size() == 2, "");
	static_assert(n4.validSize() == 1, "");

	// fix ambiguity
	constexpr auto n5 = stored::FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'g', 'f', 'o'>::create<'g','o'>("/simple amp/", "gain", "override", "output");
	static_assert(n5.size() == 3, "");
	static_assert(n5.validSize() == 2, "");
	static_assert(n5.template valid<'o'>(), "");
	static_assert(!n5.template valid<'f'>(), "");

	constexpr auto n6 = stored::FreeObjectsList<
		stored::FreeVariables<float, stored::ExampleComponents, 'i', 'g'>,
		stored::FreeVariables<bool, stored::ExampleComponents, 'e'>
	>::create("/amp/");
	static_assert(n6.size() == 3, "");
	static_assert(n6.flags() == 7ULL, "");
	static_assert(n6.valid<'i'>(n6.flags()), "");
	static_assert(n6.valid<'e'>(n6.flags()), "");


	stored::ExampleComponents store;
	constexpr auto amp_o = stored::Amplifier<stored::ExampleComponents>::objects("/amp/");
	stored::Amplifier<stored::ExampleComponents, amp_o.flags()> amp{amp_o, store};

	std::cout << amp(3) << std::endl;
	std::cout << amp(5) << std::endl;
	std::cout << amp(-2) << std::endl;
	std::cout << sizeof(amp) << std::endl;

	constexpr auto amp_o2 = stored::Amplifier<stored::ExampleComponents>::objects<'g','O'>("/simple amp/");
	using Amp2 = stored::Amplifier<stored::ExampleComponents, amp_o2.flags()>;
	Amp2 amp2;
	amp2 = Amp2{amp_o2, store};
	std::cout << amp2(3) << std::endl;
	std::cout << sizeof(amp2) << std::endl;

	constexpr auto gpio_in_o = stored::PinIn<stored::ExampleComponents>::objects("/gpio in/");
	stored::PinIn<stored::ExampleComponents, gpio_in_o.flags()> gpio_in{gpio_in_o, store};

	std::cout << gpio_in() << std::endl;
	store.gpio_in__override = 1;
	std::cout << gpio_in() << std::endl;

	constexpr auto gpio_out_o = stored::PinOut<stored::ExampleComponents>::objects("/gpio out/");
	stored::PinOut<stored::ExampleComponents, gpio_out_o.flags()> gpio_out{gpio_out_o, store};

	std::cout << gpio_out() << std::endl;
	std::cout << gpio_out(true) << std::endl;
	gpio_out.override_(0);
	std::cout << gpio_out() << std::endl;
}
