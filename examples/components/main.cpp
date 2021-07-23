#include "ExampleComponents.h"

#include <cmath>
#include <iostream>

template <char Id, size_t index0, char... Ids>
struct find_index {
};

template <char Id, size_t index0, char Id0, char... Ids>
struct find_index<Id,index0,Id0,Ids...> {
	enum { value = find_index<Id, index0 + 1, Ids...>::value };
};

template <char Id, size_t index0, char... Ids>
struct find_index<Id,index0,Id,Ids...> {
	enum { value = index0 };
};

template <char Id, char... Ids>
struct has_id { enum { value = 0 }; };

template <char Id, char... Ids>
struct has_id<Id,Id,Ids...> { enum { value = 1 }; };

template <char Id, char Id0, char... Ids>
struct has_id<Id,Id0,Ids...> { enum { value = has_id<Id,Ids...>::value }; };

template <char... Id>
struct ids { enum { size = sizeof...(Id) }; };

template <typename A, typename B>
struct merge_ids {};

template <char... A, char... B>
struct merge_ids<ids<A...>, ids<B...>> {
	using type = ids<A..., B...>;
};

template <char... Id>
struct is_unique { enum { value = 1 }; };

template <char Id0, char... Id>
struct is_unique<Id0, Id...> { enum { value = !has_id<Id0, Id...>::value && is_unique<Id...>::value }; };

template <typename T>
struct is_unique_ids {};

template <char... Id>
struct is_unique_ids<ids<Id...>> { enum { value = is_unique<Id...>::value }; };

template <typename Subset, typename Set>
struct is_subset {};

template <char... Set>
struct is_subset<ids<>, ids<Set...>> { enum { value = 1 }; };

template <char S0, char... Subset, char... Set>
struct is_subset<ids<S0, Subset...>, ids<Set...>> {
	enum { value = has_id<S0, Set...>::value && is_subset<ids<Subset...>, ids<Set...>>::value };
};

template <typename Select, typename All>
struct optional_subset {};

template <char S0, char... Select, char... All>
struct optional_subset<ids<S0, Select...>, ids<All...>> {
	static_assert(is_subset<ids<S0, Select...>, ids<All...>>::value, "");
	using type = ids<S0, Select...>;
};

template <char... All>
struct optional_subset<ids<>, ids<All...>> {
	using type = ids<All...>;
};

template <typename FreeObjects_, typename FreeObjects_::Flags flags_, bool PostponedBind = true>
class BoundObjects;

template <typename ObjectType, char... Id>
class FreeObjects {
public:
	using This = FreeObjects;
	using FreeObject = ObjectType;
	using type = typename ObjectType::type;
	using Container = typename ObjectType::Container;
	using Ids = ids<Id...>;
	using Flags = unsigned long long;

	template <Flags flags, bool PostponedBind = sizeof(FreeObject) < sizeof(typename FreeObject::Bound_type)>
	using Bound = BoundObjects<FreeObjects, flags, PostponedBind>;

	static_assert(is_unique<Id...>::value, "");

private:
	FreeObject m_objects[sizeof...(Id) == 0 ? 1 : sizeof...(Id)];

	template <char Id_, typename... Args, std::enable_if_t<!has_id<Id_, Id...>::value, int> = 0>
	constexpr size_t init(Args&&...) noexcept {
		return 0;
	}

	template <char Id_, size_t PN, size_t NN, std::enable_if_t<has_id<Id_, Id...>::value, int> = 0>
	constexpr size_t init(char const (&prefix)[PN], char const (&name)[NN]) noexcept {
		char buf[PN + NN] = {};
		size_t len = 0;

		for(; len < PN && prefix[len]; len++)
			buf[len] = prefix[len];

		for(size_t i = 0; i < NN && name[i]; len++, i++)
			buf[len] = name[i];

		auto o = find(buf, FreeObject());

		if(stored::Config::EnableAssert)
			for(size_t i = 0; i < sizeof...(Id); i++) {
				// Check if name resolution is unique.
				stored_assert(!o.valid() || m_objects[i] != o);
			}

		constexpr size_t ix = index<Id_>();
		m_objects[ix] = o;
		return ix;
	}

	template <size_t N>
	static constexpr auto find(char const (&name)[N], stored::FreeVariable<type,Container>) noexcept {
		return Container::template freeVariable<type>(name, N);
	}

	template <size_t N>
	static constexpr auto find(char const (&name)[N], stored::FreeFunction<type,Container>) noexcept {
		return Container::template freeFunction<type>(name, N);
	}

public:
	template <size_t N, char... OnlyId, char... IdMap, typename... LongNames,
		std::enable_if_t<sizeof...(IdMap) == sizeof...(LongNames), int> = 0>
	static constexpr FreeObjects create(char const (&prefix)[N], ids<OnlyId...>, ids<IdMap...>, LongNames&&... longNames) noexcept {
		static_assert(is_unique<OnlyId...>::value, "");
		static_assert(is_unique<IdMap...>::value, "");
		FreeObjects fo;
		size_t dummy[] = {0, (has_id<IdMap,OnlyId...>::value ? fo.init<IdMap>(prefix, std::forward<LongNames>(longNames)) : 0)...};
		(void)dummy;
		return fo;
	}

	template <char... OnlyId, size_t N>
	static constexpr FreeObjects create(char const (&prefix)[N]) noexcept {
		using Name = char[2];
		return create(prefix, typename optional_subset<ids<OnlyId...>, ids<Id...>>::type(), ids<Id...>(), Name{Id}...);
	}

	template <char... OnlyId, size_t N, typename... LongNames,
		std::enable_if_t<(sizeof...(LongNames) > 0 && sizeof...(LongNames) == sizeof...(Id)), int> = 0>
	static constexpr FreeObjects create(char const (&prefix)[N], LongNames&&... longNames) noexcept {
		return create(prefix, typename optional_subset<ids<OnlyId...>, ids<Id...>>::type(), ids<Id...>(), std::forward<LongNames>(longNames)...);
	}

	static constexpr size_t size() noexcept {
		return sizeof...(Id);
	}

	template <char Id_>
	static constexpr bool has() noexcept {
		return has_id<Id_, Id...>::value;
	}

	template <char Id_>
	static constexpr size_t index() noexcept {
		return find_index<Id_, 0, Id...>::value;
	}

	constexpr Flags flags() const noexcept {
		Flags f = 0;
		static_assert(sizeof(f) * 8U >= sizeof...(Id), "");

		for(size_t i = 0; i < sizeof...(Id); i++)
			if(m_objects[i].valid())
				f |= (decltype(f))1 << i;

		return f;
	}

	template <char Id_>
	constexpr bool valid() const noexcept {
		return m_objects[index<Id_>()].valid();
	}

	template <char Id_, std::enable_if_t<has<Id_>(), int> = 0>
	static constexpr bool valid(Flags flags) noexcept {
		return flags & (1ULL << index<Id_>());
	}

	template <char Id_, std::enable_if_t<!has<Id_>(), int> = 0>
	static constexpr bool valid(Flags) noexcept {
		return false;
	}

	constexpr size_t validSize() const noexcept {
		size_t cnt = 0;

		for(size_t i = 0; i < size(); i++)
			if(m_objects[i].valid())
				cnt++;

		return cnt;
	}

	static constexpr size_t validSize(Flags flags) noexcept {
		size_t count = 0;

		for(size_t i = 0; i < size(); i++)
			if((flags & (1ULL << i)))
				count++;

		return count;
	}

	template <char Id_>
	constexpr size_t validIndex() const noexcept {
		size_t vi = 0;

		for(size_t i = 0; i < index<Id_>(); i++)
			if(m_objects[i].valid())
				vi++;

		return vi;
	}

	template <char Id_, std::enable_if_t<has<Id_>(), int> = 0>
	static constexpr size_t validIndex(Flags flags) noexcept {
		size_t count = 0;

		for(size_t i = 0; i < index<Id_>(); i++)
			if((flags & (1ULL << i)))
				count++;

		return count;
	}

	template <char Id_, std::enable_if_t<!has<Id_>(), int> = 0>
	static constexpr size_t validIndex(Flags) noexcept {
		return 0;
	}

	template <char Id_>
	constexpr FreeObject object() const noexcept {
		return m_objects[index<Id_>()];
	}

	template <char Id_>
	auto object(Container& container) const noexcept {
		return object<Id_>().apply(container);
	}
};

template <typename T, typename Container, char... Id>
using FreeVariables = FreeObjects<stored::FreeVariable<T, Container>, Id...>;

template <typename T, typename Container, char... Id>
using FreeFunctions = FreeObjects<stored::FreeFunction<T, Container>, Id...>;

template <typename... B>
class BoundObjectsList;

template <typename... F>
class FreeObjectsList {};

template <typename F0>
class FreeObjectsList<F0> : public F0 {
public:
	using Head = F0;
	using Tail = void;

	constexpr FreeObjectsList() noexcept = default;

	constexpr FreeObjectsList(Head&& head) noexcept
		: Head(std::move(head))
	{}

	constexpr FreeObjectsList(Head const& head) noexcept
		: Head(head)
	{}
};

template <typename F0, typename... F>
class FreeObjectsList<F0, F...> {
public:
	using Head = F0;
	using Tail = FreeObjectsList<F...>;
	using Ids = typename merge_ids<typename Head::Ids, typename Tail::Ids>::type;
	using Flags = typename Head::Flags;
	using Container = typename Head::Container;

	template <Flags flags>
	using Bound = BoundObjectsList<typename Head::template Bound<flags>, typename Tail::template Bound<(flags >> Ids::size)>>;

	static_assert(is_unique_ids<Ids>::value, "");

private:
	Head m_head;
	Tail m_tail;

	constexpr FreeObjectsList(Head&& head, Tail&& tail) noexcept
		: m_head{std::move(head)}, m_tail{std::move(tail)}
	{}

public:
	constexpr FreeObjectsList() noexcept = default;

	static constexpr size_t size() noexcept {
		return Head::size() + Tail::size();
	}

	template <size_t N, char... OnlyId, char... IdMap, typename... LongNames,
		std::enable_if_t<sizeof...(IdMap) == sizeof...(LongNames), int> = 0>
	static constexpr FreeObjectsList create(char const (&prefix)[N], ids<OnlyId...>, ids<IdMap...>, LongNames&&... longNames) noexcept {
		static_assert(is_unique<OnlyId...>::value, "");
		static_assert(is_unique<IdMap...>::value, "");
		return FreeObjectsList{
			Head::template create(prefix, ids<OnlyId...>(), ids<IdMap...>(), longNames...),
			Tail::template create(prefix, ids<OnlyId...>(), ids<IdMap...>(), longNames...)
		};
	}

	template <char... OnlyId, size_t N>
	static constexpr FreeObjectsList create(char const (&prefix)[N]) noexcept {
		static_assert(is_unique<OnlyId...>::value, "");
		return FreeObjectsList{
			Head::template create<OnlyId...>(prefix),
			Tail::template create<OnlyId...>(prefix)
		};
	}

	template <char... OnlyId, size_t N, typename... LongNames,
		std::enable_if_t<(sizeof...(LongNames) > 0 && sizeof...(LongNames) == size()), int> = 0>
	static constexpr FreeObjectsList create(char const (&prefix)[N], LongNames&&... longNames) noexcept {
		static_assert(is_unique<OnlyId...>::value, "");
		return FreeObjectsList{
			Head::create(prefix, typename optional_subset<ids<OnlyId...>, Ids>::type(), Ids(), longNames...),
			Tail::create(prefix, typename optional_subset<ids<OnlyId...>, Ids>::type(), Ids(), longNames...)
		};
	}

	template <char Id>
	static constexpr bool has() noexcept {
		return Head::template has<Id>() || Tail::template has<Id>();
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	static constexpr size_t index() noexcept {
		return Head::template index<Id>();
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	static constexpr size_t index() noexcept {
		return Tail::template index<Id>();
	}

	constexpr Flags flags() const noexcept {
		static_assert(size() <= sizeof(Flags) * 8, "");
		return m_head.flags() | (m_tail.flags() << m_head.size());
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	static constexpr bool valid(Flags flags) noexcept {
		return Head::template valid<Id>(flags);
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	static constexpr bool valid(Flags flags) noexcept {
		return Tail::template valid<Id>(flags >> Head::size());
	}

	constexpr size_t validSize() noexcept {
		return m_head.validSize() + m_tail.validSize();
	}

	static constexpr size_t validSize(Flags flags) noexcept {
		return Head::validSize(flags) + Tail::validSize(flags >> Head::size());
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	constexpr size_t validIndex() const noexcept {
		return m_head.template validIndex<Id>();
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	constexpr size_t validIndex() const noexcept {
		return m_tail.template validIndex<Id>() + m_head.validSize();
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	static constexpr size_t validIndex(Flags flags) noexcept {
		return Head::template validIndex<Id>(flags);
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	static constexpr size_t validIndex(Flags flags) noexcept {
		return Tail::template validIndex<Id>(flags >> Head::size()) + Head::validSize(flags);
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	constexpr auto object() const noexcept {
		return m_head.template object<Id>();
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	constexpr auto object() const noexcept {
		return m_tail.template object<Id>();
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	constexpr auto object(Container& container) const noexcept {
		return m_head.template object<Id>(container);
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	constexpr auto object(Container& container) const noexcept {
		return m_tail.template object<Id>(container);
	}

	constexpr Head const& head() const noexcept {
		return m_head;
	}

	constexpr Tail const& tail() const noexcept {
		return m_tail;
	}
};

template <typename FreeObjects_, typename FreeObjects_::Flags flags_, bool PostponedBind>
class BoundObjects {
public:
	using FreeObjects = FreeObjects_;
	using Container = typename FreeObjects::Container;
	using BoundObject = typename FreeObjects::FreeObject::Bound_type;
	using type = typename BoundObject::type;
	using Ids = typename FreeObjects::Ids;
	enum { flags = flags_ };

private:
	BoundObject m_objects[std::max<size_t>(1, FreeObjects::validSize(flags))];

	template <char Id>
	static size_t init(Container& container, FreeObjects const& fo, BoundObjects& bo) noexcept {
		size_t ix = fo.template validIndex<Id>(flags);
		if(fo.template valid<Id>()) {
			stored_assert(ix < sizeof(m_objects) / sizeof(m_objects[0]));
			bo.m_objects[ix] = fo.template object<Id>().apply_(container);
			stored_assert(bo.m_objects[ix].valid());
		}
		return ix;
	}

	template <char... Id>
	static BoundObjects create(FreeObjects const& fo, Container& container, ids<Id...>) noexcept {
		BoundObjects bo;
		size_t dummy[] = {0, init<Id>(container, fo, bo)...};
		(void)dummy;
		return bo;
	}

public:
	static BoundObjects create(FreeObjects const& fo, Container& container) noexcept {
		return create(fo, container, Ids());
	}

	template <char Id>
	static constexpr bool has() noexcept {
		return FreeObjects::template has<Id>();
	}

	bool valid() const noexcept {
		return m_objects[0].valid();
	}

	template <char Id>
	static constexpr bool valid() noexcept {
		return FreeObjects::template valid<Id>(flags);
	}

	template <char Id,
		std::enable_if_t<valid<Id>(), int> = 0>
	auto& get() noexcept {
		return m_objects[FreeObjects::template validIndex<Id>(flags)];
	}

	template <char Id,
		std::enable_if_t<valid<Id>(), int> = 0>
	auto const& get() const noexcept {
		return m_objects[FreeObjects::template validIndex<Id>(flags)];
	}

	template <char Id,
		std::enable_if_t<!valid<Id>(), int> = 0>
	auto get() const noexcept {
		return BoundObject{};
	}
};

template <typename FreeObjects_, typename FreeObjects_::Flags flags_>
class BoundObjects<FreeObjects_, flags_, true> {
public:
	using FreeObjects = FreeObjects_;
	using Container = typename FreeObjects::Container;
	using FreeObject = typename FreeObjects::FreeObject;
	using BoundObject = typename FreeObjects::FreeObject::Bound_type;
	using type = typename BoundObject::type;
	using Ids = typename FreeObjects::Ids;
	enum { flags = flags_ };

private:
	Container* m_container{};
	FreeObject m_objects[std::max<size_t>(1, FreeObjects::validSize(flags))];

	template <char Id>
	static constexpr size_t init(FreeObjects const& fo, BoundObjects& bo) noexcept {
		size_t ix = fo.template validIndex<Id>(flags);
		if(fo.template valid<Id>()) {
			stored_assert(ix < sizeof(m_objects) / sizeof(m_objects[0]));
			bo.m_objects[ix] = fo.template object<Id>();
			stored_assert(bo.m_objects[ix].valid());
		}
		return ix;
	}

	template <char... Id>
	static constexpr BoundObjects create(FreeObjects const& fo, Container& container, ids<Id...>) noexcept {
		BoundObjects bo;
		bo.m_container = &container;
		size_t dummy[] = {0, init<Id>(fo, bo)...};
		(void)dummy;
		return bo;
	}

public:
	static constexpr BoundObjects create(FreeObjects const& fo, Container& container) noexcept {
		return create(fo, container, Ids());
	}

	template <char Id>
	static constexpr bool has() noexcept {
		return FreeObjects::template has<Id>();
	}

	constexpr bool valid() const noexcept {
		return m_container;
	}

	template <char Id>
	static constexpr bool valid() noexcept {
		return FreeObjects::template valid<Id>(flags);
	}

	template <char Id,
		std::enable_if_t<valid<Id>(), int> = 0>
	auto get() const noexcept {
		stored_assert(m_container);
		return m_objects[FreeObjects::template validIndex<Id>(flags)].apply_(*m_container);
	}

	template <char Id,
		std::enable_if_t<!valid<Id>(), int> = 0>
	auto get() const noexcept {
		return BoundObject{};
	}
};

template <typename... B>
class BoundObjectsList {};

template <typename B0>
class BoundObjectsList<B0> : public B0 {
public:
	using Head = B0;
	using Tail = void;

	constexpr BoundObjectsList() noexcept = default;

	constexpr BoundObjectsList(Head&& head) noexcept
		: Head(std::move(head))
	{}

	constexpr BoundObjectsList(Head const& head) noexcept
		: Head(head)
	{}
};

template <typename B0, typename... B>
class BoundObjectsList<B0,B...> {
public:
	using Head = B0;
	using Tail = BoundObjectsList<B...>;
	using FreeObjects = typename Head::FreeObjects;
	using Container = typename Head::Container;
	using Ids = typename merge_ids<typename Head::Ids, typename Tail::Ids>::type;
	enum { flags = Head::flags };

private:
	Head m_head;
	Tail m_tail;

	constexpr BoundObjectsList(Head&& head, Tail&& tail) noexcept
		: m_head{std::move(head)}, m_tail{std::move(tail)}
	{}

public:
	constexpr BoundObjectsList() noexcept = default;

	template <typename FreeObjectsList,
		std::enable_if_t<std::is_same<BoundObjectsList, typename FreeObjectsList::template Bound<flags>>::value, int> = 0>
	static constexpr BoundObjectsList create(FreeObjectsList const& fo, Container& container) noexcept {
		return BoundObjectsList{
			Head::create(fo.head(), container),
			Tail::create(fo.tail(), container)
		};
	}

	template <char Id>
	static constexpr bool has() noexcept {
		return Head::template has<Id>() || Tail::template has<Id>();
	}

	bool valid() const noexcept {
		return m_head.valid();
	}

	template <typename Id>
	static constexpr bool valid() noexcept {
		return Head::template valid<Id>() || Tail::template valid<Id>();
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	decltype(auto) get() const noexcept {
		return m_head.template get<Id>();
	}

	template <char Id,
		std::enable_if_t<Head::template has<Id>(), int> = 0>
	decltype(auto) get() noexcept {
		return m_head.template get<Id>();
	}

	template <char Id,
		std::enable_if_t<!Head::template has<Id>(), int> = 0>
	decltype(auto) get() const noexcept {
		return m_tail.template get<Id>();
	}
};

template <typename Container, typename T = float>
using AmplifierObjects = FreeObjectsList<
	FreeVariables<T, Container, 'I', 'g', 'o', 'l', 'h', 'f', 'O'>,
	FreeVariables<bool, Container, 'e'>>;

template <typename Container, unsigned long long flags = 0, typename T = float>
class Amplifier {
public:
	using type = T;
	using Bound = typename AmplifierObjects<Container, type>::template Bound<flags>;

	constexpr Amplifier() noexcept = default;

	constexpr Amplifier(AmplifierObjects<Container,type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{}

	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const(&prefix)[N]) noexcept {
		return AmplifierObjects<Container, type>::template create<OnlyId...>(prefix,
			"input", "gain", "offset", "low", "high", "override", "output", "enable");
	}

	decltype(auto) inputObject() const noexcept { return m_o.template get<'I'>(); }
	decltype(auto) inputObject() noexcept { return m_o.template get<'I'>(); }
	type input() const noexcept { decltype(auto) o = inputObject(); return o.valid() ? o.get() : type(); }

	decltype(auto) gainObject() const noexcept { return m_o.template get<'g'>(); }
	decltype(auto) gainObject() noexcept { return m_o.template get<'g'>(); }
	type gain() const noexcept { decltype(auto) o = gainObject(); return o.valid() ? o.get() : type(1); }

	decltype(auto) offsetObject() const noexcept { return m_o.template get<'o'>(); }
	decltype(auto) offsetObject() noexcept { return m_o.template get<'o'>(); }
	type offset() const noexcept { decltype(auto) o = offsetObject(); return o.valid() ? o.get() : type(); }

	decltype(auto) lowObject() const noexcept { return m_o.template get<'l'>(); }
	decltype(auto) lowObject() noexcept { return m_o.template get<'l'>(); }
	type low() const noexcept {
		decltype(auto) o = lowObject();
		if(o.valid())
			return o.get();
		if(std::numeric_limits<type>::has_infinity)
			return -std::numeric_limits<type>::infinity();
		return std::numeric_limits<type>::lowest();
	}

	decltype(auto) highObject() const noexcept { return m_o.template get<'h'>(); }
	decltype(auto) highObject() noexcept { return m_o.template get<'h'>(); }
	type high() const noexcept {
		decltype(auto) o = highObject();
		if(o.valid())
			return o.get();
		if(std::numeric_limits<type>::has_infinity)
			return std::numeric_limits<type>::infinity();
		return std::numeric_limits<type>::max();
	}

	decltype(auto) overrideObject() const noexcept { return m_o.template get<'f'>(); }
	decltype(auto) overrideObject() noexcept { return m_o.template get<'f'>(); }
	type override_() const noexcept { decltype(auto) o = overrideObject(); return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN(); }

	decltype(auto) outputObject() const noexcept { return m_o.template get<'O'>(); }
	decltype(auto) outputObject() noexcept { return m_o.template get<'O'>(); }
	type output() const noexcept { decltype(auto) o = outputObject(); return o.valid() ? o.get() : type(); }

	decltype(auto) enableObject() const noexcept { return m_o.template get<'e'>(); }
	decltype(auto) enableObject() noexcept { return m_o.template get<'e'>(); }
	bool enabled() const noexcept { decltype(auto) o = enableObject(); return !o.valid() || o.get(); }
	void enable(bool value) noexcept { decltype(auto) o = enableObject(); if(o.valid()) o = value; }
	void disable() noexcept { enable(false); }

	type operator()() noexcept {
		return run(input());
	}

	type operator()(type input) noexcept {
		decltype(auto) o = inputObject();
		if(o.valid())
			o = input;

		return run(input);
	}

protected:
	type run(type input) noexcept {
		type output = override_();

		if(!std::isnan(output)) {
			// Keep override value.
		} else {
			if(!enabled())
				output = input;
			else if(std::isnan(output))
				output = input * gain() + offset();

			output = std::min(std::max(low(), output), high());
		}

		decltype(auto) oo = outputObject();
		if(oo.valid())
			oo = output;

		return output;
	}

private:
	Bound m_o;
};

int main()
{
	constexpr auto n1 = FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i'>::create("/amp/");
	static_assert(n1.size() == 1, "");
	static_assert(n1.validSize() == 1, "");

	// j is not valid
	constexpr auto n2 = FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i', 'j'>::create("/amp/");
	static_assert(n2.size() == 2, "");
	static_assert(n2.validSize() == 1, "");

	// find by long name
	constexpr auto n3 = FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i', 'o'>::create("/amp/", "input", "output");
	static_assert(n3.size() == 2, "");
	static_assert(n3.validSize() == 2, "");

	// skip o
	constexpr auto n4 = FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'i', 'o'>::create<'i'>("/amp/", "input", "output");
	static_assert(n4.size() == 2, "");
	static_assert(n4.validSize() == 1, "");

	// fix ambiguity
	constexpr auto n5 = FreeObjects<stored::FreeVariable<float, stored::ExampleComponents>, 'g', 'f', 'o'>::create<'g','o'>("/simple amp/", "gain", "override", "output");
	static_assert(n5.size() == 3, "");
	static_assert(n5.validSize() == 2, "");
	static_assert(n5.template valid<'o'>(), "");
	static_assert(!n5.template valid<'f'>(), "");

	constexpr auto n6 = FreeObjectsList<
		FreeVariables<float, stored::ExampleComponents, 'i', 'g'>,
		FreeVariables<bool, stored::ExampleComponents, 'e'>
	>::create("/amp/");
	static_assert(n6.size() == 3, "");
	static_assert(n6.flags() == 7ULL, "");
	static_assert(n6.valid<'i'>(n6.flags()), "");
	static_assert(n6.valid<'e'>(n6.flags()), "");


	static stored::ExampleComponents store;
	constexpr auto amp_o = Amplifier<stored::ExampleComponents>::objects("/amp/");
	Amplifier<stored::ExampleComponents, amp_o.flags()> amp{amp_o, store};

	std::cout << amp(3) << std::endl;
	std::cout << amp(5) << std::endl;
	std::cout << amp(-2) << std::endl;
	std::cout << sizeof(amp) << std::endl;

	constexpr auto amp_o2 = Amplifier<stored::ExampleComponents>::objects<'g','O'>("/simple amp/");
	Amplifier<stored::ExampleComponents, amp_o2.flags()> amp2{amp_o2, store};
	std::cout << amp2(3) << std::endl;
	std::cout << sizeof(amp2) << std::endl;
}




