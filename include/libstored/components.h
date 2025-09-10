#ifndef LIBSTORED_COMPONENTS_H
#define LIBSTORED_COMPONENTS_H
// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/macros.h>

#if defined(__cplusplus) && STORED_cplusplus >= 201402L && defined(STORED_DRAFT_API)

#  include <libstored/config.h>
#  include <libstored/types.h>
#  include <libstored/util.h>

#  include <cmath>
#  include <type_traits>
#  include <utility>

namespace stored {

//////////////////////////////////////////////////////////
// Store lookup and free/bound variables processing
//////////////////////////////////////////////////////////

/*
 * This is how it works:
 *
 * The directory can be searched at compile time.  So, given a name of
 * an object, the meta data (offset of a variable, or function number)
 * can be determined.  This results in a FreeVariable or FreeFunction.
 * Free objects hold the meta data, but are not bound to a specific
 * store instance.
 *
 * A component (Amplifier, PID, LowPass, etc.) consists of multiple
 * objects of different types.  For every set of objects of the same
 * type, a FreeObjects list is constructed, which holds an array of
 * FreeVariables or FreeFunctions as returned by the directory lookup.
 *
 * To create a single thing that holds all objects of a component, the
 * FreeObjects are chained together as a FreeObjectsList. This is a
 * recursive data type, which is in fact a list of FreeObjects.
 *
 * Objects may not be in the store. In that case, the FreeVariables or
 * FreeFunctions are invalid.  As we know this at compile time, it
 * would be waste of memory to allocate components with all objects, as
 * we could also leave the invalid ones out. Moreover, if we know which
 * objects are not there and do not contribute to the component output,
 * the implementation may be optimized.
 *
 * So, the analysis of the free objects must be passed to a concrete
 * implementation that leaves out instances of the bound objects.  For
 * this, we must pass this analysis as a template parameter to the
 * concrete implementation. Bit flags (unsigned long long) are used for
 * this; every object has a specific index in the flags.
 *
 * When the free objects are to be bound to a store instance, the flags
 * indicate which objects must be processed.  The component has a
 * BoundObjectsList, which mirrors the FreeObjectsList, but does not
 * instantiate a Variable or Function for the masked out objects by the
 * flags.
 *
 * The reason for using arrays is because leaving objects out won't
 * consume memory in that case.  You could create a class that either
 * holds the object, or is empty, but the empty object must consume at
 * least one byte, which may become a word after alignment in another
 * class. If bound objects are organized in an array, this byte
 * overhead is gone, except for the case when the whole array is empty.
 *
 * Summarized:
 *
 * - constexpr directory find results in a free object, which is
 *   combined into a FreeObjectsList, which flags indicating which
 *   objects are in the store.  It holds an array of FreeObjects.
 * - construct a BoundObjectsList type, given the flags of the
 *   FreeObjectsList.  It holds an array of Variables or Functions, but
 *   only those in the flags.
 * - During run-time, construct the BoundObjectLists and bind the valid
 *   free functions to a specific store.
 *
 * Objects have an index in the flags. This is not really manageable.
 * Therefore, every object has a char alias, which maps to the index.
 */

namespace impl {
/*!
 * \brief Find the index of the given \p Id in \p Ids.
 *
 * The member \c value will be set to the index, if it can be found.
 */
template <char Id, size_t index0, char... Ids>
struct find_index {};

template <char Id, size_t index0, char Id0, char... Ids>
struct find_index<Id, index0, Id0, Ids...> {
	enum { value = find_index<Id, index0 + 1, Ids...>::value };
};

template <char Id, size_t index0, char... Ids>
struct find_index<Id, index0, Id, Ids...> {
	enum { value = index0 };
};

/*!
 * \brief Check if the given \p Id is in the list \p Ids.
 *
 * The member \c value is set to non-zero when it is in the list.
 */
template <char Id, char... Ids>
struct has_id {
	enum { value = 0 };
};

template <char Id, char... Ids>
struct has_id<Id, Id, Ids...> {
	enum { value = 1 };
};

template <char Id, char Id0, char... Ids>
struct has_id<Id, Id0, Ids...> {
	enum { value = has_id<Id, Ids...>::value };
};

/*!
 * \brief Container to hold a list of \c Ids.
 *
 * The member \c size is set to the length of the list.
 */
template <char... Id>
struct ids {
	enum { size = sizeof...(Id) };
};

/*!
 * \brief Check if the given \p Id is in the \c ids list.
 *
 * The member \c value is set to non-zero when it is in the list.
 */
template <char Id, typename Ids>
struct has_id_in_ids {};

template <char Id, char... Ids>
struct has_id_in_ids<Id, ids<Ids...>> {
	enum { value = has_id<Id, Ids...>::value };
};

/*!
 * \brief Concatenate two \c Id lists.
 *
 * The member \c type will be set to the new #stored::impl::ids list.
 */
template <typename A, typename B>
struct merge_ids {};

template <char... A, char... B>
struct merge_ids<ids<A...>, ids<B...>> {
	using type = ids<A..., B...>;
};

/*!
 * \brief Check if there are no duplicates in the list of \p Ids.
 *
 * The member \c value is set to 1 if that is the case.
 */
template <char... Id>
struct is_unique {
	enum { value = 1 };
};

template <char Id0, char... Id>
struct is_unique<Id0, Id...> {
	enum { value = !has_id<Id0, Id...>::value && is_unique<Id...>::value };
};

/*!
 * \brief Check if there are no duplicates in the list of T, which must be \c ids.
 *
 * The member \c value is set to 1 when that is the case.
 */
template <typename T>
struct is_unique_ids {};

template <char... Id>
struct is_unique_ids<ids<Id...>> {
	enum { value = is_unique<Id...>::value };
};

/*!
 * \brief Check if all members of the \p Subset #stored::impl::ids exist in the \p Set
 * #stored::impl::ids.
 *
 * The member \c value is set to 1 if that is the case.
 */
template <typename Subset, typename Set>
struct is_subset {};

template <char... Set>
struct is_subset<ids<>, ids<Set...>> {
	enum { value = 1 };
};

template <char S0, char... Subset, char... Set>
struct is_subset<ids<S0, Subset...>, ids<Set...>> {
	enum { value = has_id<S0, Set...>::value && is_subset<ids<Subset...>, ids<Set...>>::value };
};

/*!
 * \brief Either return the specified subset, or the whole set, if no subset was given.
 *
 * \p Select and \p All must be #stored::impl::ids.
 * The member \c type will be set to \p All if \p Select is empty, or \p Select otherwise.
 */
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

} // namespace impl

using impl::ids;

template <typename FreeObjects_, typename FreeObjects_::Flags flags_, bool PostponedBind = true>
class BoundObjects;

/*!
 * \brief A list of free objects with the same type.
 *
 * A free object is of type \p ObjectType, which must be a
 * #stored::FreeVariable or #stored::FreeFunction.  This class does a
 * lookup in the store's directory, and saves the offset of the
 * variable, or index of the function, without saving the instance of
 * the store. In that sense, the object is free; it is not bound to a
 * specific store instance, it only holds the metadata of the object.
 *
 * This class holds a list of such objects, which must be all of the
 * same type.  Lookup and analysis can be done at compile time.
 *
 * Objects may not exist in the store. A set of flags is created, which
 * indicate which variables exist and which do not. Based on this set
 * of flags, a #stored::BoundObjects list can be constructed, tailored
 * to which variables actually exist. For this, the #flags() must be
 * passed as template parameter to #stored::BoundObjects.
 *
 * The \p Id list aliases the members of elements passed to #create().
 */
template <typename ObjectType, char... Id>
class FreeObjects {
public:
	using This = FreeObjects;
	using FreeObject = ObjectType;
	using type = typename ObjectType::type;
	using Container = typename ObjectType::Container;
	using Ids = ids<Id...>;
	using Flags = unsigned long long;

	template <Flags flags>
	using Bound = BoundObjects<
		FreeObjects, flags, sizeof(FreeObject) < sizeof(typename FreeObject::Bound_type)>;

	static_assert(impl::is_unique<Id...>::value, "");

private:
	/*! \brief The list of free objects. */
	FreeObject m_objects[sizeof...(Id) == 0 ? 1 : sizeof...(Id)]{};

	/*!
	 * \brief Initialize the given \p Id_, which does not exist in this list of free variables.
	 */
	template <
		char Id_, typename... Args,
		std::enable_if_t<!impl::has_id<Id_, Id...>::value, int> = 0>
	constexpr size_t init(Args&&...) noexcept
	{
		return 0;
	}

	/*!
	 * \brief Initialize the given \p Id_ with a name in the store.
	 *
	 * The name of \p Id_ consists of \p prefix and \p name.
	 *
	 * \return the index of this variable in #m_objects.
	 */
	template <
		char Id_, size_t PN, size_t NN,
		std::enable_if_t<impl::has_id<Id_, Id...>::value, int> = 0>
	constexpr size_t init(char const (&prefix)[PN], char const (&name)[NN]) noexcept
	{
		// flawfinder: ignore
		char buf[PN + NN] = {};
		size_t len = 0;

		for(; len < PN && prefix[len]; len++)
			buf[len] = prefix[len];

		for(size_t i = 0; i < NN && name[i]; len++, i++)
			buf[len] = name[i];

		auto o = find(buf, FreeObject());

		if(Config::EnableAssert)
			for(size_t i = 0; i < sizeof...(Id); i++) {
				// Check if name resolution is unique.
				stored_assert(
					!o.valid()
					|| m_objects[i] != o); // If this fails, the names are not
							       // unique. Provide the ids of the
							       // objects that are in the store.
			}

		constexpr size_t ix = index<Id_>();
		m_objects[ix] = o;
		return ix;
	}

	/*!
	 * \brief Finds the given variable name in the store.
	 */
	template <size_t N>
	static constexpr auto find(char const (&name)[N], FreeVariable<type, Container>) noexcept
	{
		return Container::template freeVariable<type>(name, N);
	}

	/*!
	 * \brief Finds the given function name in the store.
	 */
	template <size_t N>
	static constexpr auto find(char const (&name)[N], FreeFunction<type, Container>) noexcept
	{
		return Container::template freeFunction<type>(name, N);
	}

public:
	/*!
	 * \brief Create the list of free objects.
	 *
	 * The list of \p longNames are appended to the \p prefix.
	 * The \p longNames must match the \p IdMap in length and order.
	 * It only initializes the \p OnlyId subset of \p IdMap.
	 */
	template <
		size_t N, char... OnlyId, char... IdMap, typename... LongNames,
		std::enable_if_t<sizeof...(IdMap) == sizeof...(LongNames), int> = 0>
	static constexpr FreeObjects
	create(char const (&prefix)[N], ids<OnlyId...>, ids<IdMap...>,
	       LongNames&&... longNames) noexcept
	{
		static_assert(impl::is_unique<OnlyId...>::value, "");
		static_assert(impl::is_unique<IdMap...>::value, "");
		FreeObjects fo;
		using onlyIds = ids<OnlyId...>;
		size_t dummy[] = {
			0, (impl::has_id_in_ids<IdMap, onlyIds>::value
				    ? fo.init<IdMap>(prefix, std::forward<LongNames>(longNames))
				    : 0)...};
		(void)dummy;
		return fo;
	}

	/*!
	 * \brief Create the list of free objects.
	 *
	 * This overload requires an explicit list of ids, which is also used as name.
	 * For this, the names must be unambiguous and correspond to the id.
	 */
	template <char... OnlyId, size_t N>
	static constexpr FreeObjects create(char const (&prefix)[N]) noexcept
	{
		using Name = char[2];
		return create(
			prefix, typename impl::optional_subset<ids<OnlyId...>, ids<Id...>>::type(),
			ids<Id...>(), Name{Id}...);
	}

	/*!
	 * \brief Create the list of free objects.
	 *
	 * When called without explicit list of \p OnlyId, all names
	 * are processed.  The names in the store should not be
	 * ambiguous.
	 *
	 * If they are ambiguous, specify \p OnlyId to force only
	 * looking for specific subset of variables.
	 */
	template <
		char... OnlyId, size_t N, typename... LongNames,
		std::enable_if_t<
			(sizeof...(LongNames) > 0 && sizeof...(LongNames) == sizeof...(Id)), int> =
			0>
	static constexpr FreeObjects
	create(char const (&prefix)[N], LongNames&&... longNames) noexcept
	{
		return create(
			prefix, typename impl::optional_subset<ids<OnlyId...>, ids<Id...>>::type(),
			ids<Id...>(), std::forward<LongNames>(longNames)...);
	}

	/*!
	 * \brief Return the number of free variables.
	 */
	static constexpr size_t size() noexcept
	{
		return sizeof...(Id);
	}

	/*!
	 * \brief Check if the given \p Id_ exists in this list of free variables.
	 *
	 * This does not mean that it also exists in the store.
	 *
	 * \see #valid()
	 */
	template <char Id_>
	static constexpr bool has() noexcept
	{
		return (bool)impl::has_id<Id_, Id...>::value;
	}

	/*!
	 * \brief Return the index of the given \p Id_.
	 *
	 * The \p Id_ must exist in \p Id.
	 */
	template <char Id_>
	static constexpr size_t index() noexcept
	{
		return impl::find_index<Id_, 0, Id...>::value;
	}

	/*!
	 * \brief Return the flags.
	 *
	 * The flags indicate which free objects exist in the store.
	 * The order of the flags correspond to #m_objects.
	 */
	constexpr Flags flags() const noexcept
	{
		Flags f = 0;
		static_assert(sizeof(f) * 8U >= sizeof...(Id), "");

		for(size_t i = 0; i < sizeof...(Id); i++)
			if(m_objects[i].valid())
				f |= (decltype(f))1 << i;

		return f;
	}

	/*!
	 * \brief Check if the given \p Id_ exists in the store.
	 */
	template <char Id_>
	constexpr bool valid() const noexcept
	{
		return m_objects[index<Id_>()].valid();
	}

	/*!
	 * \brief Check if the given \p Id_ exists in the store, based on a set of \p flags.
	 */
	template <char Id_>
	static constexpr decltype(std::enable_if_t<has<Id_>(), bool>()) valid(Flags flags) noexcept
	{
		return flags & (1ULL << index<Id_>());
	}

	/*!
	 * \brief Check if the given \p Id_ exists in the store, based on a set of \p flags.
	 */
	template <char Id_>
	static constexpr decltype(std::enable_if_t<!has<Id_>(), bool>()) valid(Flags) noexcept
	{
		return false;
	}

	/*!
	 * \brief Return the number of valid objects in the store.
	 */
	constexpr size_t validSize() const noexcept
	{
		size_t cnt = 0;

		for(size_t i = 0; i < size(); i++)
			if(m_objects[i].valid())
				cnt++;

		return cnt;
	}

	/*!
	 * \brief Return the number of valid objects in the store, based on the given \p flags.
	 */
	static constexpr size_t validSize(Flags flags) noexcept
	{
		size_t count = 0;

		for(size_t i = 0; i < size(); i++)
			if((flags & (1ULL << i)))
				count++;

		return count;
	}

	/*!
	 * \brief Return the index of the given \p Id_ in the list of valid objects.
	 */
	template <char Id_>
	constexpr size_t validIndex() const noexcept
	{
		size_t vi = 0;

		for(size_t i = 0; i < index<Id_>(); i++)
			if(m_objects[i].valid())
				vi++;

		return vi;
	}

	/*!
	 * \brief Return the index of the given \p Id_ in the list of valid objects, given a set of
	 * \p flags.
	 */
	template <char Id_>
	static constexpr decltype(std::enable_if_t<has<Id_>(), size_t>())
	validIndex(Flags flags) noexcept
	{
		size_t count = 0;

		for(size_t i = 0; i < index<Id_>(); i++)
			if((flags & (1ULL << i)))
				count++;

		return count;
	}

	/*!
	 * \brief Return the index of the given \p Id_ in the list of valid objects, given a set of
	 * \p flags.
	 *
	 * This overload covers the case that \p Id_ is not in this list of free variables.
	 */
	template <char Id_>
	static constexpr decltype(std::enable_if_t<!has<Id_>(), size_t>())
	validIndex(Flags) noexcept
	{
		return 0;
	}

	/*!
	 * \brief Return the free object corresponding to the given \p Id_.
	 */
	template <char Id_>
	constexpr FreeObject object() const noexcept
	{
		return m_objects[index<Id_>()];
	}

	/*!
	 * \brief Return the bound object corresponding to the given \p Id_.
	 */
	template <char Id_>
	auto object(Container& container) const noexcept
	{
		return object<Id_>().apply(container);
	}
};

// We only have free variables and free functions.  Create a few
// convenience types for them.
template <typename T, typename Container, char... Id>
using FreeVariables = FreeObjects<FreeVariable<T, Container>, Id...>;

template <typename T, typename Container, char... Id>
using FreeFunctions = FreeObjects<FreeFunction<T, Container>, Id...>;

template <typename... B>
class BoundObjectsList;

/*!
 * \brief Create a list of #stored::FreeObjects, holding any type of free objects.
 *
 * A #stored::FreeObjects can only hold a list of objects of the same
 * type.  A \c FreeObjectsList is a recursive data structure, which
 * links multiple #stored::FreeObjects with different types. So, it is
 * a list of lists of free objects.
 *
 * The list has a \c Head, which is just a FreeObjects, and a \c Tail
 * that is a FreeObjectsList with the remaining types. The \c Tail can
 * be \c void to indicate the end.
 *
 * The API of the FreeObjectsList corresponds to a FreeObjects, but
 * combines all remaining FreeObjects of the list, such as the total
 * size, or doing a id lookup.
 */
template <typename... F>
class FreeObjectsList {};

// This is the end of the list.
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

// The non-last (init) part of the list.
template <typename F0, typename... F>
class FreeObjectsList<F0, F...> {
public:
	using Head = F0;
	using Tail = FreeObjectsList<F...>;
	using Ids = typename impl::merge_ids<typename Head::Ids, typename Tail::Ids>::type;
	using Flags = typename Head::Flags;
	using Container = typename Head::Container;

	template <Flags flags>
	using Bound = BoundObjectsList<
		typename Head::template Bound<flags>,
		typename Tail::template Bound<(flags >> Head::Ids::size)>>;

	static_assert(impl::is_unique_ids<Ids>::value, "");

private:
	Head m_head;
	Tail m_tail;

	constexpr FreeObjectsList(Head&& head, Tail&& tail) noexcept
		: m_head{std::move(head)}
		, m_tail{std::move(tail)}
	{}

public:
	constexpr FreeObjectsList() noexcept = default;

	/*!
	 * \brief Return the total number of objects in this list of lists.
	 */
	static constexpr size_t size() noexcept
	{
		return Head::size() + Tail::size();
	}

	/*!
	 * \brief Create the list of free variables.
	 *
	 * It is like #stored::FreeObjects::create(), but it combines all ids and names of all
	 * chained FreeObjects.
	 */
	template <
		size_t N, char... OnlyId, char... IdMap, typename... LongNames,
		std::enable_if_t<sizeof...(IdMap) == sizeof...(LongNames), int> = 0>
	static constexpr FreeObjectsList
	create(char const (&prefix)[N], ids<OnlyId...>, ids<IdMap...>,
	       LongNames&&... longNames) noexcept
	{
		static_assert(impl::is_unique<OnlyId...>::value, "");
		static_assert(impl::is_unique<IdMap...>::value, "");
		return FreeObjectsList{
			Head::create(
				prefix, ids<OnlyId...>(), ids<IdMap...>(), longNames...),
			Tail::create(
				prefix, ids<OnlyId...>(), ids<IdMap...>(), longNames...)};
	}

	/*!
	 * \brief Create the list of free variables.
	 * \see #stored::FreeObjects::create()
	 */
	template <char... OnlyId, size_t N>
	static constexpr FreeObjectsList create(char const (&prefix)[N]) noexcept
	{
		static_assert(impl::is_unique<OnlyId...>::value, "");
		return FreeObjectsList{
			Head::template create<OnlyId...>(prefix),
			Tail::template create<OnlyId...>(prefix)};
	}

	/*!
	 * \brief Create the list of free variables.
	 * \see #stored::FreeObjects::create()
	 */
	template <
		char... OnlyId, size_t N, typename... LongNames,
		std::enable_if_t<
			(sizeof...(LongNames) > 0 && sizeof...(LongNames) == Ids::size), int> = 0>
	static constexpr FreeObjectsList
	create(char const (&prefix)[N], LongNames&&... longNames) noexcept
	{
		static_assert(impl::is_unique<OnlyId...>::value, "");
		return FreeObjectsList{
			Head::create(
				prefix, typename impl::optional_subset<ids<OnlyId...>, Ids>::type(),
				Ids(), longNames...),
			Tail::create(
				prefix, typename impl::optional_subset<ids<OnlyId...>, Ids>::type(),
				Ids(), longNames...)};
	}

	/*!
	 * \brief Checks if the given \p Id is in one of the free objects lists.
	 */
	template <char Id>
	static constexpr bool has() noexcept
	{
		return Head::template has<Id>() || Tail::template has<Id>();
	}

	/*!
	 * \brief Returns the index of the free object of the given \p Id.
	 *
	 * This overload covers the indices of the head.
	 */
	template <char Id>
	static constexpr decltype(std::enable_if_t<Head::template has<Id>(), size_t>())
	index() noexcept
	{
		return Head::template index<Id>();
	}

	/*!
	 * \brief Returns the index of the free object of the given \p Id.
	 *
	 * This overload covers the indices of the tail.
	 * The offset of the indices is the size of the head.
	 */
	template <char Id>
	static constexpr decltype(std::enable_if_t<!Head::template has<Id>(), size_t>())
	index() noexcept
	{
		return Tail::template index<Id>();
	}

	/*!
	 * \brief Return the combined flags of all lists of free objects.
	 *
	 * The flags may be passed to a BoundObjectsLists's template
	 * parameter.  Therefore, the type must be primitive type.  So,
	 * it is bounded in size (typically 64 bit, so 64 objects).
	 */
	constexpr Flags flags() const noexcept
	{
		static_assert(size() <= sizeof(Flags) * 8, "");
		return m_head.flags() | (m_tail.flags() << m_head.size());
	}

	/*!
	 * \brief Check if the given \p Id is a valid object in the store.
	 *
	 * This overload covers the case that \p Id is in the head.
	 */
	template <char Id>
	static constexpr decltype(std::enable_if_t<Head::template has<Id>(), bool>())
	valid(Flags flags) noexcept
	{
		return Head::template valid<Id>(flags);
	}

	/*!
	 * \brief Check if the given \p Id is a valid object in the store.
	 *
	 * This overload covers the case that \p Id is in the tail.
	 */
	template <char Id>
	static constexpr decltype(std::enable_if_t<!Head::template has<Id>(), bool>())
	valid(Flags flags) noexcept
	{
		return Tail::template valid<Id>(flags >> Head::size());
	}

	/*!
	 * \brief Return the total number of valid free objects.
	 */
	constexpr size_t validSize() noexcept
	{
		return m_head.validSize() + m_tail.validSize();
	}

	/*!
	 * \brief Return the total number of valid free objects, given a set of \p flags.
	 */
	static constexpr size_t validSize(Flags flags) noexcept
	{
		return Head::validSize(flags) + Tail::validSize(flags >> Head::size());
	}

	/*!
	 * \brief Return the index within the list of valid objects, given an \p Id.
	 *
	 * This overload covers the case that it is in the head.
	 */
	template <char Id>
	constexpr decltype(std::enable_if_t<Head::template has<Id>(), size_t>())
	validIndex() const noexcept
	{
		return m_head.template validIndex<Id>();
	}

	/*!
	 * \brief Return the index within the list of valid objects, given an \p Id.
	 *
	 * This overload covers the case that it is in the tail.
	 */
	template <char Id>
	constexpr decltype(std::enable_if_t<!Head::template has<Id>(), size_t>())
	validIndex() const noexcept
	{
		return m_tail.template validIndex<Id>() + m_head.validSize();
	}

	/*!
	 * \brief Return the index within the list of valid objects, given an \p Id, given a set of
	 * \p flags.
	 *
	 * This overload covers the case that it is in the head.
	 */
	template <char Id>
	static constexpr decltype(std::enable_if_t<Head::template has<Id>(), size_t>())
	validIndex(Flags flags) noexcept
	{
		return Head::template validIndex<Id>(flags);
	}

	/*!
	 * \brief Return the index within the list of valid objects, given an \p Id, given a set of
	 * \p flags.
	 *
	 * This overload covers the case that it is in the tail.
	 */
	template <char Id>
	static constexpr decltype(std::enable_if_t<!Head::template has<Id>(), size_t>())
	validIndex(Flags flags) noexcept
	{
		return Tail::template validIndex<Id>(flags >> Head::size())
		       + Head::validSize(flags);
	}

	/*!
	 * \brief Return the free object, given an \c Id.
	 *
	 * This overload covers the case that it is in the head.
	 */
	template <char Id>
	constexpr auto
	object(decltype(std::enable_if_t<Head::template has<Id>(), int>()) = 0) const noexcept
	{
		return m_head.template object<Id>();
	}

	/*!
	 * \brief Return the free object, given an \c Id.
	 *
	 * This overload covers the case that it is in the tail.
	 */
	template <char Id>
	constexpr auto
	object(decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) const noexcept
	{
		return m_tail.template object<Id>();
	}

	/*!
	 * \brief Return the bound object, given an \c Id.
	 *
	 * This overload covers the case that it is in the head.
	 */
	template <char Id>
	constexpr auto
	object(Container& container,
	       decltype(std::enable_if_t<Head::template has<Id>(), int>()) = 0) const noexcept
	{
		return m_head.template object<Id>(container);
	}

	/*!
	 * \brief Return the bound object, given an \c Id.
	 *
	 * This overload covers the case that it is in the tail.
	 */
	template <char Id>
	constexpr auto
	object(Container& container,
	       decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) const noexcept
	{
		return m_tail.template object<Id>(container);
	}

	/*!
	 * \brief Return the head of the list of lists of free objects.
	 */
	constexpr Head const& head() const noexcept
	{
		return m_head;
	}

	/*!
	 * \brief Return the tail of the list of lists of free objects.
	 */
	constexpr Tail const& tail() const noexcept
	{
		return m_tail;
	}
};

/*!
 * \brief A bound list of objects.
 *
 * The #stored::FreeObjects hold only store-instance-independent
 * metadata of the objects.  The BoundObjects lists is the same list,
 * but tailored towards only having #stored::Variable and
 * #stored::Function that are actually in the store. So, only exactly
 * enough memory is allocated to hold the administration of the
 * variables/functions that actually exist.
 *
 * During construction, it takes only processes the objects from \p
 * FreeObjects_, for which the corresponding \p flags_ are set.
 *
 * Variables and Functions are almost free to construct, given a
 * FreeVariable and FreeFunction and a store reference.  Therefore,
 * BoundObjects can implement \p PostponedBind; when \c true, it holds
 * a free object and applies the store when the bound object is
 * required, when \c false, it holds the bound objects.  Depending on
 * which object is smaller (a FreeVariable or just a Variable), one or
 * both strategies are chosen.
 */
// This is the implementation of the bound objects.
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
	/*! \brief The list of bound objects. */
	BoundObject m_objects[std::max<size_t>(1, FreeObjects::validSize(flags))]{};

	/*!
	 * \brief Initialize the object with the given \p Id.
	 */
	template <char Id>
	static size_t init(Container& container, FreeObjects const& fo, BoundObjects& bo) noexcept
	{
		size_t ix = fo.template validIndex<Id>(flags);
		if(fo.template valid<Id>()) {
			stored_assert(ix < sizeof(m_objects) / sizeof(m_objects[0]));
			bo.m_objects[ix] = fo.template object<Id>().apply_(container);
			stored_assert(bo.m_objects[ix].valid());
		}
		return ix;
	}

	/*!
	 * \brief Helper function to invoke #init() for every \p Id.
	 */
	template <char... Id>
	static BoundObjects create(FreeObjects const& fo, Container& container, ids<Id...>) noexcept
	{
		BoundObjects bo;
		size_t dummy[] = {0, init<Id>(container, fo, bo)...};
		(void)dummy;
		return bo;
	}

public:
	/*!
	 * \brief Initialize the list of bound objects, given a list of free objects and a concrete
	 * container.
	 *
	 * Make sure that \p fo's flags correspond to \p flags_.
	 */
	static BoundObjects create(FreeObjects const& fo, Container& container) noexcept
	{
		return create(fo, container, Ids());
	}

	/*!
	 * \brief Check if the given \p Id exists in the list of free objects.
	 */
	template <char Id>
	static constexpr bool has() noexcept
	{
		return FreeObjects::template has<Id>();
	}

	/*!
	 * \brief Check if this bound list is initialized and valid.
	 */
	bool valid() const noexcept
	{
		// All objects are valid by definition, so only check the first one.
		return m_objects[0].valid();
	}

	/*!
	 * \brief Check if the given \p Id is valid.
	 *
	 * If so, it can be accessed using #get().
	 */
	template <char Id>
	static constexpr bool valid() noexcept
	{
		return FreeObjects::template valid<Id>(flags);
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	auto& get(std::enable_if_t<valid<Id>(), int> = 0) noexcept
	{
		return m_objects[FreeObjects::template validIndex<Id>(flags)];
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	auto const& get(std::enable_if_t<valid<Id>(), int> = 0) const noexcept
	{
		return m_objects[FreeObjects::template validIndex<Id>(flags)];
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 *
	 * This overload covers the case that the given \p Id does not
	 * correspond to a valid object.  Note that this overload does
	 * not return a reference.  So, always use \c decltype(auto) to
	 * save the returned object in.
	 */
	template <char Id>
	auto get(decltype(std::enable_if_t<!valid<Id>(), int>()) = 0) const noexcept
	{
		return BoundObject{};
	}
};

// This is the implementation of BoundObjects, where only free objects
// are saved and bound upon request.
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
	/*! \brief The container. */
	Container* m_container{};
	/*! \brief The free objects, which can be bound to #m_container. */
	FreeObject m_objects[std::max<size_t>(1, FreeObjects::validSize(flags))];

	/*!
	 * \brief Initialize the given free object.
	 */
	template <char Id>
	static constexpr size_t init(FreeObjects const& fo, BoundObjects& bo) noexcept
	{
		size_t ix = fo.template validIndex<Id>(flags);
		if(fo.template valid<Id>()) {
			stored_assert(ix < sizeof(m_objects) / sizeof(m_objects[0]));
			bo.m_objects[ix] = fo.template object<Id>();
			stored_assert(bo.m_objects[ix].valid());
		}
		return ix;
	}

	/*!
	 * \brief Helper to call #init() for every \p Id.
	 */
	template <char... Id>
	static constexpr BoundObjects
	create(FreeObjects const& fo, Container& container, ids<Id...>) noexcept
	{
		BoundObjects bo;
		bo.m_container = &container;
		size_t dummy[] = {0, init<Id>(fo, bo)...};
		(void)dummy;
		return bo;
	}

public:
	/*!
	 * \brief Initialize the list of bound objects, given a list of free objects and a concrete
	 * container.
	 *
	 * Make sure that \p fo's flags correspond to \p flags_.
	 */
	static constexpr BoundObjects create(FreeObjects const& fo, Container& container) noexcept
	{
		return create(fo, container, Ids());
	}

	/*!
	 * \brief Check if the given \p Id exists in the list of free objects.
	 */
	template <char Id>
	static constexpr bool has() noexcept
	{
		return FreeObjects::template has<Id>();
	}

	/*!
	 * \brief Check if this bound list is initialized and valid.
	 */
	constexpr bool valid() const noexcept
	{
		return m_container;
	}

	/*!
	 * \brief Check if the given \p Id is valid.
	 *
	 * If so, it can be accessed using #get().
	 */
	template <char Id>
	static constexpr bool valid() noexcept
	{
		return FreeObjects_::template valid<Id>(flags);
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	auto get(decltype(std::enable_if_t<valid<Id>(), int>()) = 0) const noexcept
	{
		stored_assert(m_container);
		// It might sound expensive, but it is really only like
		// saving m_container + offset in a Variable.
		return m_objects[FreeObjects::template validIndex<Id>(flags)].apply_(*m_container);
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	auto get(decltype(std::enable_if_t<!valid<Id>(), int>()) = 0) const noexcept
	{
		return BoundObject{};
	}
};

/*!
 * \brief A list of bound objects.
 *
 * Like BoundObjects binds a FreeObjects to a store, similarly binds
 * BoundObjectsLists a FreeObjectsLists to a store.  It is, like
 * FreeObjectsLists, a recursive data type, which links BoundObjects
 * with different object types into a single list.
 *
 * The API is the same as a BoundObjects, but with all ids and indices
 * combined into a single interface.
 */
template <typename... B>
class BoundObjectsList {};

// This is the end of the list.
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

// This is the all-but-last (init) of the list.
template <typename B0, typename... B>
class BoundObjectsList<B0, B...> {
public:
	using Head = B0;
	using Tail = BoundObjectsList<B...>;
	using FreeObjects = typename Head::FreeObjects;
	using Container = typename Head::Container;
	using Ids = typename impl::merge_ids<typename Head::Ids, typename Tail::Ids>::type;
	enum { flags = Head::flags };

private:
	Head m_head;
	Tail m_tail;

	constexpr BoundObjectsList(Head&& head, Tail&& tail) noexcept
		: m_head{std::move(head)}
		, m_tail{std::move(tail)}
	{}

public:
	/*!
	 * \brief Default ctor to create a non-initialized list.
	 */
	constexpr BoundObjectsList() noexcept = default;

	/*!
	 * \brief Binds a free list to a store.
	 */
	template <typename FreeObjectsList>
	static constexpr std::enable_if_t<
		std::is_same<
			BoundObjectsList, typename FreeObjectsList::template Bound<flags>>::value,
		BoundObjectsList>
	create(FreeObjectsList const& fo, Container& container) noexcept
	{
		return BoundObjectsList{
			Head::create(fo.head(), container), Tail::create(fo.tail(), container)};
	}

	/*!
	 * \brief Check if the given \p Id exists in the list of free objects.
	 */
	template <char Id>
	static constexpr bool has() noexcept
	{
		return Head::template has<Id>() || Tail::template has<Id>();
	}

	/*!
	 * \brief Check if this bound list is initialized and valid.
	 */
	bool valid() const noexcept
	{
		return m_head.valid();
	}

	/*!
	 * \brief Check if the given \p Id is valid.
	 *
	 * If so, it can be accessed using #get().
	 */
	template <char Id>
	static constexpr bool valid() noexcept
	{
		return Head::template valid<Id>() || Tail::template valid<Id>();
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	decltype(auto)
	get(decltype(std::enable_if_t<Head::template has<Id>(), int>()) = 0) const noexcept
	{
		return m_head.template get<Id>();
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	decltype(auto) get(std::enable_if_t<Head::template has<Id>(), int> = 0) noexcept
	{
		return m_head.template get<Id>();
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	decltype(auto)
	get(decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) const noexcept
	{
		return m_tail.template get<Id>();
	}

	/*!
	 * \brief Return the object with the given \p Id.
	 */
	template <char Id>
	decltype(auto)
	get(decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) noexcept
	{
		return m_tail.template get<Id>();
	}
};



//////////////////////////////////////////////////////////
// Amplifier
//////////////////////////////////////////////////////////

// Definition of the Amplifier objects.
template <typename Container, typename T = float>
using AmplifierObjects = FreeObjectsList<
	FreeVariables<T, Container, 'I', 'g', 'o', 'l', 'h', 'F', 'O'>,
	FreeVariables<bool, Container, 'e'>>;

/*!
 * \brief An offset/gain amplifier, based on store variables.
 *
 * This class comes in very handy when converting ADC inputs to some
 * SI-value. It includes an override field to force inputs to some test
 * value.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     float input
 *     bool=true enable
 *     float=1 gain
 *     float=0 offset
 *     float=-inf low
 *     float=inf high
 *     float=nan override
 *     float output
 * } amp
 * \endcode
 *
 * All fields are optional.  All variables of type \c float can be any
 * other type, as long as it matches the template parameter \p T.
 *
 * When not all fields are in the store, names may become ambiguous.
 * For example, if override and output are not there, the store's
 * directory may resolve \c o to any of the three fields. In this case,
 * you have to specify which fields are to be processed. For this, use
 * the following ids:
 *
 * field    | id
 * -------- | ----
 * input    | \c I
 * enable   | \c e
 * gain     | \c g
 * offset   | \c o
 * low      | \c l
 * high     | \c h
 * override | \c F
 * output   | \c O
 *
 * The amplifier basically does:
 *
 * \code
 * if(override is nan)
 *     output = min(high, max(low, input * gain + offset))
 * else
 *     output = override
 * \endcode
 *
 * Then, instantiate the amplifier like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto amp_o = stored::Amplifier<stored::YourStore>::objects("/amp/");
 *
 * // Instantiate an Amplifier, tailored to the available fields in the store.
 * stored::Amplifier<stored::YourStore, amp_o.flags()> amp{amp_o, yourStore};
 * \endcode
 *
 * Or, for example when you know there are only the offset and gain
 * fields in the store, and ambiguity must be resolved:
 *
 * \code
 * // Construct a compile-time object, which resolves only two fields in your store.
 * constexpr auto amp_o = stored::Amplifier<stored::YourStore>::objects<'o','g'>("/amp/");
 * stored::Amplifier<stored::YourStore, amp_o.flags()> amp{amp_o, yourStore};
 * \endcode
 *
 * Calling \c amp() now uses the \c input and produces the value in \c
 * output.  Alternatively, or when the \c input field is absent in the
 * store, call \c amp(x), where \c x is the input.
 */
template <typename Container, unsigned long long flags = 0, typename T = float>
class Amplifier {
public:
	using type = T;
	using Bound = typename AmplifierObjects<Container, type>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed.  Do not access or
	 * run the Amplifier instance, as it does not hold proper
	 * references to a store.  You can just assign another
	 * Amplifier instance.
	 */
	constexpr Amplifier() noexcept = default;

	/*!
	 * \brief Initialize the Amplifier, given a list of objects and a container.
	 */
	constexpr Amplifier(AmplifierObjects<Container, type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return AmplifierObjects<Container, type>::template create<OnlyId...>(
			prefix, "input", "gain", "offset", "low", "high", "override", "output",
			"enable");
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() const noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c input value, or 0 when not available. */
	type input() const noexcept
	{
		decltype(auto) o = inputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the \c gain object. */
	decltype(auto) gainObject() const noexcept
	{
		return m_o.template get<'g'>();
	}

	/*! \brief Return the \c gain object. */
	decltype(auto) gainObject() noexcept
	{
		return m_o.template get<'g'>();
	}

	/*! \brief Return the \c gain value, or 1 when not available. */
	type gain() const noexcept
	{
		decltype(auto) o = gainObject();
		return o.valid() ? o.get() : type(1);
	}

	/*! \brief Return the \c offset object. */
	decltype(auto) offsetObject() const noexcept
	{
		return m_o.template get<'o'>();
	}

	/*! \brief Return the \c offset object. */
	decltype(auto) offsetObject() noexcept
	{
		return m_o.template get<'o'>();
	}

	/*! \brief Return the \c offset value, or 1 when not available. */
	type offset() const noexcept
	{
		decltype(auto) o = offsetObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the \c low object. */
	decltype(auto) lowObject() const noexcept
	{
		return m_o.template get<'l'>();
	}
	/*! \brief Return the \c low object. */
	decltype(auto) lowObject() noexcept
	{
		return m_o.template get<'l'>();
	}

	/*! \brief Return the \c low value, or -inf when not available. */
	type low() const noexcept
	{
		decltype(auto) o = lowObject();
		if(o.valid())
			return o.get();
		if(std::numeric_limits<type>::has_infinity)
			return -std::numeric_limits<type>::infinity();
		return std::numeric_limits<type>::lowest();
	}

	/*! \brief Return the \c high object. */
	decltype(auto) highObject() const noexcept
	{
		return m_o.template get<'h'>();
	}

	/*! \brief Return the \c high object. */
	decltype(auto) highObject() noexcept
	{
		return m_o.template get<'h'>();
	}

	/*! \brief Return the \c high value, or inf when not available. */
	type high() const noexcept
	{
		decltype(auto) o = highObject();
		if(o.valid())
			return o.get();
		if(std::numeric_limits<type>::has_infinity)
			return std::numeric_limits<type>::infinity();
		return std::numeric_limits<type>::max();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or NaN when not available. */
	type override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() const noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output value, or 0 when not available. */
	type output() const noexcept
	{
		decltype(auto) o = outputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() const noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable value, which is \c true when not available. */
	bool enabled() const noexcept
	{
		decltype(auto) o = enableObject();
		return !o.valid() || o.get();
	}

	/*!
	 * \brief Enable (or disable) the Amplifier.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void enable(bool value = true) noexcept
	{
		decltype(auto) o = enableObject();
		if(o.valid())
			o = value;
	}

	/*!
	 * \brief Disable the Amplifier.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void disable() noexcept
	{
		enable(false);
	}

	/*!
	 * \brief Compute the Amplifier output, given the input as stored in the store.
	 */
	type operator()() noexcept
	{
		return run(input());
	}

	/*!
	 * \brief Compute the Amplifier output, given an input.
	 */
	type operator()(type input) noexcept
	{
		decltype(auto) o = inputObject();
		if(o.valid())
			o = input;

		return run(input);
	}

protected:
	/*!
	 * \brief Compute the Amplifier output.
	 */
	type run(type input) noexcept
	{
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



//////////////////////////////////////////////////////////
// PinIn
//////////////////////////////////////////////////////////

template <typename Container>
using PinInObjects = FreeObjectsList<
	FreeFunctions<bool, Container, 'p'>, FreeVariables<int8_t, Container, 'F'>,
	FreeVariables<bool, Container, 'i'>>;

/*!
 * \brief An GPIO input pin, based on store variables.
 *
 * This class comes in very handy when a GPIO input should be observed and
 * overridden while debugging.  It gives some interface between the
 * hardware pin and the input that the application sees.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (bool) pin
 *     int8=-1 override
 *     bool input
 *     (bool) get
 * } pin
 * \endcode
 *
 * All fields are optional. You can implement the store's \c pin function,
 * override the virtual \c pin() function of the PinIn class, or pass the
 * hardware pin value as an argument to the PinIn::operator().
 *
 * The pin basically does:
 *
 * \code
 * switch(override) {
 * case -1: input = pin; break;
 * case  0: input = false; break;
 * case  1: input = true; break;
 * case  2: input = !pin; break;
 * }
 * \endcode
 *
 * Then, instantiate the pin like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto pin_o = stored::PinIn<stored::YourStore>::objects("/pin/");
 *
 * // Instantiate an PinIn, tailored to the available fields in the store.
 * stored::PinIn<stored::YourStore, pin_o.flags()> pin{pin_o, yourStore};
 * \endcode
 *
 * When \c pin() is called, it will invoke the \c pin function to get the
 * actual hardware pin status.  Then, it will set the \c input variable.
 *
 * The \c get function is not used/provided by this \c PinIn. Implement
 * this store function such that it calls and returns \c pin(). When
 * applications read the \c get function, they will always get the
 * appropriate/actual pin value.
 */
template <typename Container, unsigned long long flags = 0>
class PinIn {
public:
	using Bound = typename PinInObjects<Container>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr PinIn() noexcept = default;

	/*!
	 * \brief Dtor.
	 */
	virtual ~PinIn() = default;

	/*!
	 * \brief Initialize the pin, given a list of objects and a container.
	 */
	constexpr PinIn(PinInObjects<Container> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return PinInObjects<Container>::template create<OnlyId...>(
			prefix, "pin", "override", "input");
	}

	/*! \brief Return the \c pin object. */
	decltype(auto) pinObject() const noexcept
	{
		return m_o.template get<'p'>();
	}

	/*!
	 * \brief Return the hardware pin value.
	 *
	 * By default, it calls the \c pin function in the store.
	 * Override in a subclass to implement other behavior.
	 */
	virtual bool pin() const noexcept
	{
		decltype(auto) o = pinObject();
		return o.valid() ? o() : false;
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or -1 when the object is not available. */
	int8_t override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : -1;
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() const noexcept
	{
		return m_o.template get<'i'>();
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() noexcept
	{
		return m_o.template get<'i'>();
	}

	/*!
	 * \brief Return the last computed \c input value, or compute the pin state when the object
	 *	is not available.
	 */
	template <char I = 'i', std::enable_if_t<Bound::template has<I>(), int> = 0>
	bool input() const noexcept
	{
		return inputObject().get();
	}

	/*!
	 * \brief Return the last computed \c input value, or compute the pin state when the object
	 *	is not available.
	 */
	template <char I = 'i', std::enable_if_t<!Bound::template has<I>(), int> = 0>
	bool input() noexcept
	{
		return (*this)();
	}

	/*! \brief Determine pin input, given the current hardware state. */
	bool operator()() noexcept
	{
		return (*this)(pin());
	}

	/*! \brief Determine pin input, given the provided hardware state. */
	bool operator()(bool pin) noexcept
	{
		bool i;
		switch(override_()) {
		default:
		case -1:
			i = pin;
			break;
		case 0:
			i = false;
			break;
		case 1:
			i = true;
			break;
		case 2:
			i = !pin;
			break;
		}

		decltype(auto) io = inputObject();
		if(io.valid())
			io = i;

		return i;
	}

private:
	Bound m_o;
};



//////////////////////////////////////////////////////////
// PinOut
//////////////////////////////////////////////////////////

template <typename Container>
using PinOutObjects =
	FreeObjectsList<FreeVariables<bool, Container, 'o'>, FreeFunctions<bool, Container, 'p'>>;

/*!
 * \brief An GPIO output pin, based on store variables.
 *
 * This class comes in very handy when a GPIO output should be observed and
 * overridden while debugging.  It gives some interface between the
 * hardware pin and the output that the application wants.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (bool) set
 *     bool output
 *     (int8) override
 *     (bool) pin
 * } pin
 * \endcode
 *
 * All fields are optional, except \c output. You can implement the store's
 * \c pin function, override the virtual \c pin() function of the PinOut
 * class, or forward the return value of PinOut::operator() to the hardware
 * pin.
 *
 * The pin basically does:
 *
 * \code
 * switch(override) {
 * case -1: pin = output; break;
 * case  0: pin = false; break;
 * case  1: pin = true; break;
 * case  2: pin = !output; break;
 * }
 * \endcode
 *
 * Then, instantiate the pin like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto pin_o = stored::PinOut<stored::YourStore>::objects("/pin/");
 *
 * // Instantiate an PinOut, tailored to the available fields in the store.
 * stored::PinOut<stored::YourStore, pin_o.flags()> pin{pin_o, yourStore};
 * \endcode
 *
 * The \c set function is not used/provided by this \c PinOut. Implement
 * this store function such that it calls \c pin() with the provided value.
 * When applications write the \c set function, they will immediately
 * control the hardware pin.
 *
 * Similar holds for the \c override function; implement it to call the
 * #override_() of PinOut. This way, if one sets the override value, the
 * hardware pin is updated accordingly.
 */
template <typename Container, unsigned long long flags = 0>
class PinOut {
public:
	using Bound = typename PinOutObjects<Container>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr PinOut() noexcept = default;

	/*!
	 * \brief Dtor.
	 */
	virtual ~PinOut() = default;

	/*!
	 * \brief Initialize the pin, given a list of objects and a container.
	 */
	constexpr PinOut(PinOutObjects<Container> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{
		static_assert(Bound::template valid<'o'>(), "'output' variable is mandatory");
	}

	/*!
	 * \brief Create the list of objects in the store, used to compute the
	 *	\p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return PinOutObjects<Container>::template create<OnlyId...>(
			prefix, "output", "pin");
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() const noexcept
	{
		return m_o.template get<'o'>();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() noexcept
	{
		return m_o.template get<'o'>();
	}

	/*! \brief Return the \c output value. */
	bool output() const noexcept
	{
		return outputObject().get();
	}

	/*! \brief Return the override value. */
	int8_t override_() const noexcept
	{
		return m_override;
	}

	/*! \brief Set the override value. */
	void override_(int8_t x) noexcept
	{
		m_override = x;
		(*this)();
	}

	/*! \brief Return the \c pin object. */
	decltype(auto) pinObject() noexcept
	{
		return m_o.template get<'p'>();
	}

	/*!
	 * \brief Set the hardware pin state.
	 *
	 * The default implementation calls the store's \c pin
	 * function.  Override in a subclass to implement custom
	 * behavior.
	 */
	virtual void pin(bool value) noexcept
	{
		decltype(auto) o = pinObject();
		if(o.valid())
			o(value);
	}

	/*!
	 * \brief Compute and set the hardware pin status, given the last
	 *	provided application's output value.
	 */
	bool operator()() noexcept
	{
		return run(output());
	}

	/*!
	 * \brief Compute and set the hardware pin status, given the
	 *	application's output value.
	 */
	bool operator()(bool output) noexcept
	{
		outputObject() = output;
		return run(output);
	}

protected:
	/*!
	 * \brief Compute and set the output pin value.
	 */
	bool run(bool output) noexcept
	{
		bool p;
		switch(override_()) {
		default:
		case -1:
			p = output;
			break;
		case 0:
			p = false;
			break;
		case 1:
			p = true;
			break;
		case 2:
			p = !output;
			break;
		}

		pin(p);
		return p;
	}

private:
	Bound m_o;
	int8_t m_override{-1};
};



//////////////////////////////////////////////////////////
// PID
//////////////////////////////////////////////////////////

template <typename Container, typename T = float>
using PIDObjects = FreeObjectsList<
	FreeFunctions<float, Container, 'f'>,
	FreeVariables<
		T, Container, 'y', 's', 'p', 'i', 'd', 'k', 'I', 'L', 'H', 'l', 'h', 'E', '3', 'F',
		'u'>,
	FreeVariables<bool, Container, 'e', 'r'>>;

/*!
 * \brief PID controller, based on store variables.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (float) frequency (Hz)
 *     float y
 *     float setpoint
 *     bool=true enable
 *     float=1 Kp
 *     float=inf Ti (s)
 *     float=0 Td (s)
 *     float=0 Kff
 *     float int
 *     float=-inf int low
 *     float=inf int high
 *     float=-inf low
 *     float=inf high
 *     float=inf error max
 *     float=inf epsilon
 *     bool reset
 *     float=nan override
 *     float u
 * } pid
 * \endcode
 *
 * Only \c frequency, \c setpoint, and \c Kp are mandatory.  All
 * variables of type \c float, except for \c frequency, can be any
 * other type, as long as it matches the template parameter \p T.
 *
 * It has the following objects:
 * - \c frequency: the control frequency; the application must invoke
 *   the PID controller at this frequency
 * - \c y: the process variable (output of the plant)
 * - \c setpoint: the setpoint to control \c y to
 * - \c Kp: P coefficient
 * - \c Ti: I time constant
 * - \c Td: D time constant
 * - \c Kff: feed-forward coefficient
 * - \c int: current integral value
 * - <tt>int low</tt>: lower bound for \c int
 * - <tt>int high</tt>: upper bound for \c int
 * - \c low: lower bound for computed \c u
 * - \c high: upper bound for computed \c u
 * - \c epsilon: minimum value of the error ( | \c setpoint - \c y | ),
 *   which must result in a change of the output \c u. Otherwise,
 *   numerical stability is compromised. See #isHealthy().
 * - \c reset: when set to \c true, recompute and apply changed control
 *   parameters
 * - \c override: when not NaN, force \c u to this value, without \c
 *   low and \c high applied
 * - \c u: control output (input for the plant)
 *
 * Then, instantiate the controller like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto pid_o = stored::PID<stored::YourStore>::objects("/pid/");
 *
 * // Instantiate a PID, tailored to the available fields in the store.
 * stored::PID<stored::YourStore, pid_o.flags()> pid{pid_o, yourStore};
 * \endcode
 *
 * The PID controller has the following properties:
 *
 * - The parameters specify a serial PID.
 * - The integral windup prevention stops the integral when the output clips.
 * - Changing Ti is implemented smoothly; changing the parameters (and
 *   setting \c reset afterwards) can be done while running.
 * - #isHealthy() checks for numerical stability.
 */
template <typename Container, unsigned long long flags = 0, typename T = float>
class PID {
public:
	using type = T;
	using Bound = typename PIDObjects<Container, type>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr PID() noexcept = default;

	/*!
	 * \brief Initialize the pin, given a list of objects and a container.
	 */
	constexpr PID(PIDObjects<Container, type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{
		static_assert(Bound::template valid<'f'>(), "'frequency' function is mandatory");
		static_assert(Bound::template valid<'s'>(), "'setpoint' variable is mandatory");
		static_assert(Bound::template valid<'p'>(), "'Kp' variable is mandatory");

		decltype(auto) uo = uObject();
		if(uo.valid())
			m_u = uo.get();
		else
			m_u = std::max<type>(low(), 0);
	}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return PIDObjects<Container, type>::template create<OnlyId...>(
			prefix, "frequency", "y", "setpoint", "Kp", "Ti", "Td", "Kff", "int",
			"int low", "int high", "low", "high", "error max", "epsilon", "override",
			"u", "enable", "reset");
	}

	/*! \brief Return the \c frequency object. */
	decltype(auto) frequencyObject() const noexcept
	{
		return m_o.template get<'f'>();
	}

	/*! \brief Return the control frequency. */
	float frequency() const noexcept
	{
		return frequencyObject()();
	}

	/*! \brief Return the \c y object. */
	decltype(auto) yObject() const noexcept
	{
		return m_o.template get<'y'>();
	}

	/*! \brief Return the \c y object. */
	decltype(auto) yObject() noexcept
	{
		return m_o.template get<'y'>();
	}

	/*! \brief Return the \c y value, or 0 when not available. */
	type y() const noexcept
	{
		decltype(auto) o = yObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the \c setpoint object. */
	decltype(auto) setpointObject() const noexcept
	{
		return m_o.template get<'s'>();
	}

	/*! \brief Return the \c setpoint object. */
	decltype(auto) setpointObject() noexcept
	{
		return m_o.template get<'s'>();
	}

	/*! \brief Return the \c setpoint value. */
	type setpoint() const noexcept
	{
		return setpointObject().get();
	}

	/*! \brief Return the \c Kp object. */
	decltype(auto) KpObject() const noexcept
	{
		return m_o.template get<'p'>();
	}

	/*! \brief Return the \c Kp object. */
	decltype(auto) KpObject() noexcept
	{
		return m_o.template get<'p'>();
	}

	/*! \brief Return the \c Kp value. */
	type Kp() const noexcept
	{
		return KpObject().get();
	}

	/*! \brief Return the \c Ti object. */
	decltype(auto) TiObject() const noexcept
	{
		return m_o.template get<'i'>();
	}

	/*! \brief Return the \c Ti object. */
	decltype(auto) TiObject() noexcept
	{
		return m_o.template get<'i'>();
	}

	/*! \brief Return the \c Ti value, or inf when not available. */
	type Ti() const noexcept
	{
		decltype(auto) o = TiObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the computed Ki value. */
	type Ki() const noexcept
	{
		return m_Ki;
	}

	/*! \brief Return the \c Td object. */
	decltype(auto) TdObject() const noexcept
	{
		return m_o.template get<'d'>();
	}

	/*! \brief Return the \c Td object. */
	decltype(auto) TdObject() noexcept
	{
		return m_o.template get<'d'>();
	}

	/*! \brief Return the \c Td value, or 0 when not available. */
	type Td() const noexcept
	{
		decltype(auto) o = TdObject();
		return o.valid() ? o.get() : (type)0;
	}

	/*! \brief Return the computed Kd value. */
	type Kd() const noexcept
	{
		return m_Kd;
	}

	/*! \brief Return the \c Kff object. */
	decltype(auto) KffObject() const noexcept
	{
		return m_o.template get<'k'>();
	}

	/*! \brief Return the \c Kff object. */
	decltype(auto) KffObject() noexcept
	{
		return m_o.template get<'k'>();
	}

	/*! \brief Return the \c Kff value, or 0 when not available. */
	type Kff() const noexcept
	{
		decltype(auto) o = KffObject();
		return o.valid() ? o.get() : (type)0;
	}

	/*! \brief Return the \c int object. */
	decltype(auto) intObject() const noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c int object. */
	decltype(auto) intObject() noexcept
	{
		return m_o.template get<'I'>();
	}

	/*!
	 * \brief Return the current integral value.
	 *
	 * This is the integral of the <tt>(setpoint - y) * Ki</tt>.
	 * So, when Ti (and there fore Ki) changes, it may take a while
	 * till the new Ti is in effect, depending on the current
	 * integral value.
	 */
	type int_() const noexcept
	{
		return m_int;
	}

	/*! \brief Return the <tt>int low</tt> object. */
	decltype(auto) intLowObject() const noexcept
	{
		return m_o.template get<'L'>();
	}

	/*! \brief Return the <tt>int low</tt> object. */
	decltype(auto) intLowObject() noexcept
	{
		return m_o.template get<'L'>();
	}

	/*! \brief Return the <tt>int low</tt> value, or -inf when not available. */
	type intLow() const noexcept
	{
		decltype(auto) o = intLowObject();
		return o.valid() ? o.get() : -std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the <tt>int high</tt> object. */
	decltype(auto) intHighObject() const noexcept
	{
		return m_o.template get<'H'>();
	}

	/*! \brief Return the <tt>int high</tt> object. */
	decltype(auto) intHighObject() noexcept
	{
		return m_o.template get<'H'>();
	}

	/*! \brief Return the <tt>int high</tt> vale, or inf when not available. */
	type intHigh() const noexcept
	{
		decltype(auto) o = intHighObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the \c low object. */
	decltype(auto) lowObject() const noexcept
	{
		return m_o.template get<'l'>();
	}

	/*! \brief Return the \c low object. */
	decltype(auto) lowObject() noexcept
	{
		return m_o.template get<'l'>();
	}

	/*! \brief Return the \c low value, or -inf when not available. */
	type low() const noexcept
	{
		decltype(auto) o = lowObject();
		return o.valid() ? o.get() : -std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the \c high object. */
	decltype(auto) highObject() const noexcept
	{
		return m_o.template get<'h'>();
	}

	/*! \brief Return the \c high object. */
	decltype(auto) highObject() noexcept
	{
		return m_o.template get<'h'>();
	}

	/*! \brief Return the \c high value, or inf when not available. */
	type high() const noexcept
	{
		decltype(auto) o = highObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the <tt>error max</tt> object. */
	decltype(auto) errorMaxObject() const noexcept
	{
		return m_o.template get<'E'>();
	}

	/*! \brief Return the <tt>error max</tt> object. */
	decltype(auto) errorMaxObject() noexcept
	{
		return m_o.template get<'E'>();
	}

	/*! \brief Return the <tt>error max</tt> value, or inf when not available. */
	type errorMax() const noexcept
	{
		decltype(auto) o = errorMaxObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the \c epsilon object. */
	decltype(auto) epsilonObject() const noexcept
	{
		return m_o.template get<'3'>();
	}

	/*! \brief Return the \c epsilon object. */
	decltype(auto) epsilonObject() noexcept
	{
		return m_o.template get<'3'>();
	}

	/*! \brief Return the \c epsilon value, or inf when not available. */
	type epsilon() const noexcept
	{
		decltype(auto) o = epsilonObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or NaN when not available. */
	type override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN();
	}

	/*! \brief Return the \c u object. */
	decltype(auto) uObject() const noexcept
	{
		return m_o.template get<'u'>();
	}

	/*! \brief Return the \c u object. */
	decltype(auto) uObject() noexcept
	{
		return m_o.template get<'u'>();
	}

	/*! \brief Return the \c u value, with the override applied. */
	type u() const noexcept
	{
		type o = override_();
		return std::isnan(o) ? m_u : o;
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() const noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable value, or \c true when not available. */
	bool enabled() const noexcept
	{
		decltype(auto) o = enableObject();
		return !o.valid() || o.get();
	}

	/*!
	 * \brief Enable (or disable) the PID.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void enable(bool value = true) noexcept
	{
		decltype(auto) o = enableObject();
		if(o.valid())
			o = value;
	}

	/*!
	 * \brief Disable the PID.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void disable() noexcept
	{
		enable(false);
	}

	/*! \brief Return the \c reset object. */
	decltype(auto) resetObject() const noexcept
	{
		return m_o.template get<'r'>();
	}

	/*! \brief Return the \c reset object. */
	decltype(auto) resetObject() noexcept
	{
		return m_o.template get<'r'>();
	}

	/*! \brief Return the \c reset value, or \c false when not available. */
	bool reset() const noexcept
	{
		decltype(auto) o = resetObject();
		return o.valid() && o.get();
	}

	/*!
	 * \brief Compute the PID output, given a \c y.
	 */
	type operator()(type y) noexcept
	{
		decltype(auto) o = yObject();
		if(o.valid())
			o = y;

		return run(y);
	}

	/*!
	 * \brief Compute the PID output, given the \c y as stored in the store.
	 */
	type operator()() noexcept
	{
		return run(y());
	}

	/*!
	 * \brief Check numerical stability.
	 *
	 * #epsilon() is the smallest change in error ( | #setpoint() -
	 * #y() | ) that must have influence the output #u().  If the
	 * error is smaller, the output may remain the same. This
	 * function checks if that is still the case.
	 *
	 * The integrator is especially interesting. If it becomes too
	 * large, successive (small) errors may not be able to reduce
	 * it anymore, because of rounding. If that is the case, it is
	 * considered unhealthy.
	 *
	 * This value can optionally be defined in the store. If
	 * omitted, this function always returns \c true.
	 *
	 * You may want to check (or assert on) this function once in a
	 * while, like once per second or after every run, to detect a
	 * stuck controller within reasonable time for your
	 * application.
	 */
	bool isHealthy() const noexcept
	{
		auto k = Ki();
		if(k == 0)
			return true;

		auto e = epsilon();
		auto i = std::fabs(int_());

		// If the result is true, the integrator is not too
		// large, such that smallest error can still reduce it.
		return i - e * k < i;
	}

protected:
	/*!
	 * \brief Compute control output.
	 */
	type run(type y) noexcept
	{
		type u = override_();

		if(likely(std::isnan(u))) {
			if(!enabled())
				return m_u;

			bool doReset = false;
			decltype(auto) reset_o = resetObject();
			if(reset_o.valid()) {
				if(unlikely(reset_o.get())) {
					doReset = true;
					reset_o = false;
				}
			} else if(unlikely(std::isnan(m_y_prev))) {
				doReset = true;
			}

			type sp = setpoint();
			type e = sp - y;

			auto e_o = errorMaxObject();
			if(e_o.valid()) {
				auto em = e_o.get();
				if(e < -em)
					e = -em;
				else if(e > em)
					e = em;
			}

			if(unlikely(doReset)) {
				float f = frequency();
				m_Ki = 0;
				m_Kd = 0;
				m_y_prev = y;

				if(!std::isnan(f) && f > 0) {
					float dt = 1.0f / frequency();
					if(Ti() != 0)
						m_Ki = Kp() * dt / Ti();
					m_Kd = -Kp() * Td() / dt;
				}

				decltype(auto) io = intObject();
				if(io.valid())
					m_int = io.get();
			}

			u = Kp() * e + m_int + Kff() * sp;

			type di = Ki() * e;
			if(likely((u >= low() || di > 0) && (u <= high() || di < 0))) {
				// Anti-windup: only update m_int when we are within output
				// bounds, or if we get back into those bounds.
				type i = std::max(intLow(), std::min(intHigh(), m_int + di));
				u += i - m_int;
				m_int = i;

				decltype(auto) io = intObject();
				if(io.valid())
					io = m_int;
			}

			if(Kd() != 0) {
				u += Kd() * (y - m_y_prev);
				m_y_prev = y;
			}

			m_u = u = std::max(low(), std::min(high(), u));
		}

		decltype(auto) uo = uObject();
		if(uo.valid())
			uo = u;

		return u;
	}

private:
	Bound m_o;
	type m_y_prev{std::numeric_limits<type>::quiet_NaN()};
	type m_Ki{};
	type m_Kd{};
	type m_int{};
	type m_u{};
};



//////////////////////////////////////////////////////////
// Sine
//////////////////////////////////////////////////////////

template <typename T>
constexpr T pi = T(3.141592653589793238462643383279502884L);

template <typename Container, typename T = float>
using SineObjects = FreeObjectsList<
	FreeFunctions<float, Container, 's'>,
	FreeVariables<T, Container, 'A', 'f', 'p', 'o', 'F', 'O'>,
	FreeVariables<bool, Container, 'e'>>;

/*!
 * \brief Sine wave generator, based on store variables.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (float) sample frequency (Hz)
 *     float=1 amplitude
 *     float=0.159 frequency (Hz)
 *     float=0 phase (rad)
 *     float=0 offset
 *     bool=true enable
 *     float=nan override
 *     float output
 * } sine
 * \endcode
 *
 * Only <tt>sample frequency</tt> is mandatory.  All variables of type
 * \c float, except for <tt>sample frequency</tt>, can be any other
 * type, as long as it matches the template parameter \p T.
 *
 * When either \c override or \c output is omitted, names may become
 * ambiguous.  In that case, provide the ids of the fields that are in
 * the store, as template parameters to #objects():
 *
 * field            | id
 * ---------------- | ----
 * sample frequency | \c s
 * amplitude        | \c A
 * frequency        | \c f
 * phase            | \c p
 * offset           | \c o
 * enable           | \c e
 * override         | \c F
 * output           | \c O
 *
 * Then, instantiate the sine wave generator like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto sine_o = stored::Sine<stored::YourStore>::objects("/sine/");
 *
 * // Instantiate the generator, tailored to the available fields in the store.
 * stored::Sine<stored::YourStore, sine_o.flags()> sine{sine_o, yourStore};
 * \endcode
 *
 * When the parameters of the sine wave are changed while running, they
 * are applied immediately, without a smooth transition.
 */
template <typename Container, unsigned long long flags = 0, typename T = float>
class Sine {
public:
	using type = T;
	using Bound = typename SineObjects<Container, type>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr Sine() noexcept = default;

	/*!
	 * \brief Initialize the sine, given a list of objects and a container.
	 */
	constexpr Sine(SineObjects<Container, type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{
		static_assert(
			Bound::template valid<'s'>(), "'sample frequency' function is mandatory");
	}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return SineObjects<Container, type>::template create<OnlyId...>(
			prefix, "sample frequency", "amplitude", "frequency", "phase", "offset",
			"override", "output", "enable");
	}

	/*! \brief Return the <tt>sample frequency</tt> object. */
	decltype(auto) sampleFrequencyObject() const noexcept
	{
		return m_o.template get<'s'>();
	}

	/*! \brief Return the sample frequency. */
	float sampleFrequency() const noexcept
	{
		return sampleFrequencyObject()();
	}

	/*! \brief Return the \c amplitude object. */
	decltype(auto) amplitudeObject() const noexcept
	{
		return m_o.template get<'A'>();
	}

	/*! \brief Return the \c amplitude object. */
	decltype(auto) amplitudeObject() noexcept
	{
		return m_o.template get<'A'>();
	}

	/*! \brief Return the \c amplitude value, or 1 when not available. */
	type amplitude() const noexcept
	{
		decltype(auto) o = amplitudeObject();
		return o.valid() ? o.get() : (type)1;
	}

	/*! \brief Return the \c frequency object. */
	decltype(auto) frequencyObject() const noexcept
	{
		return m_o.template get<'f'>();
	}

	/*! \brief Return the \c frequency object. */
	decltype(auto) frequencyObject() noexcept
	{
		return m_o.template get<'f'>();
	}

	/*! \brief Return the \c frequency value, or 1/2pi when not specified. */
	type frequency() const noexcept
	{
		decltype(auto) o = frequencyObject();
		return o.valid() ? o.get() : (type)0.5 / pi<type>;
	}

	/*! \brief Return the \c phase object. */
	decltype(auto) phaseObject() const noexcept
	{
		return m_o.template get<'p'>();
	}

	/*! \brief Return the \c phase object. */
	decltype(auto) phaseObject() noexcept
	{
		return m_o.template get<'p'>();
	}

	/*! \brief Return the \c phase value, or 0 when not available. */
	type phase() const noexcept
	{
		decltype(auto) o = phaseObject();
		return o.valid() ? o.get() : (type)0;
	}

	/*! \brief Return the \c offset object. */
	decltype(auto) offsetObject() const noexcept
	{
		return m_o.template get<'o'>();
	}

	/*! \brief Return the \c offset object. */
	decltype(auto) offsetObject() noexcept
	{
		return m_o.template get<'o'>();
	}

	/*! \brief Return the \c offset value, or 0 when not available. */
	type offset() const noexcept
	{
		decltype(auto) o = offsetObject();
		return o.valid() ? o.get() : (type)0;
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or NaN when not available. */
	type override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() const noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output value, or 0 when not available. */
	type output() const noexcept
	{
		decltype(auto) o = outputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() const noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable value, or \c true when not available. */
	bool enabled() const noexcept
	{
		decltype(auto) o = enableObject();
		return !o.valid() || o.get();
	}

	/*!
	 * \brief Enable (or disable) the sine wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void enable(bool value = true) noexcept
	{
		decltype(auto) o = enableObject();
		if(o.valid())
			o = value;
	}

	/*!
	 * \brief Disable the sine wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void disable() noexcept
	{
		enable(false);
	}

	/*!
	 * \brief Compute the sine output.
	 */
	type operator()() noexcept
	{
		auto f = frequency();
		type period = f > 0 ? (type)1 / f : 0;

		type output = override_();

		if(likely(std::isnan(output))) {
			if(likely(enabled()))
				output = amplitude()
					 * std::sin((type)2 * pi<type> * f * m_t + phase());
			else
				output = 0;

			output += offset();
		}

		if(likely(period > 0)) {
			auto sf = sampleFrequency();
			if(likely(sf > 0)) {
				type dt = (type)(1.0f / sf);
				m_t = std::fmod(m_t + dt, period);
			}
		}

		decltype(auto) oo = outputObject();
		if(oo.valid())
			oo = output;

		return output;
	}

	/*!
	 * \brief Check numerical stability.
	 *
	 * This function checks if for every control interval (1 /
	 * #sampleFrequency()), the output is actually updated.
	 * Especially the period and phase values are checked if they
	 * are not too big.
	 *
	 * You may want to check (or assert on) this function once in a
	 * while, like once per second or after every run, to detect a
	 * stuck controller within reasonable time for your
	 * application.
	 */
	bool isHealthy() const noexcept
	{
		auto sf = sampleFrequency();
		if(sf <= 0)
			return true;

		auto f = frequency();
		if(f <= 0)
			return true;

		auto dt = 1.0f / sf;
		auto period = (type)1 / f;
		if(period + dt == period)
			return false;

		auto ph = phase();
		auto ph_test = (type)10 * f * dt;
		return ph_test + ph != ph;
	}

private:
	Bound m_o;
	type m_t{};
};



//////////////////////////////////////////////////////////
// PulseWave
//////////////////////////////////////////////////////////

template <typename Container, typename T = float>
using PulseWaveObjects = FreeObjectsList<
	FreeFunctions<float, Container, 's'>,
	FreeVariables<T, Container, 'A', 'f', 'p', 'd', 'F', 'O'>,
	FreeVariables<bool, Container, 'e'>>;

/*!
 * \brief Pulse wave generator, based on store variables.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (float) sample frequency (Hz)
 *     float=1 amplitude
 *     float=1 frequency (Hz)
 *     float=0 phase (rad)
 *     float=0.5 duty cycle
 *     bool=true enable
 *     float=nan override
 *     float output
 * } pulse
 * \endcode
 *
 * Only <tt>sample frequency</tt> is mandatory.  All variables of type
 * \c float, except for <tt>sample frequency</tt>, can be any other
 * type, as long as it matches the template parameter \p T.
 *
 * When either \c override or \c output is omitted, names may become
 * ambiguous.  In that case, provide the ids of the fields that are in
 * the store, as template parameters to #objects():
 *
 * field            | id
 * ---------------- | ----
 * sample frequency | \c s
 * amplitude        | \c A
 * frequency        | \c f
 * phase            | \c p
 * duty cycle       | \c d
 * enable           | \c e
 * override         | \c F
 * output           | \c O
 *
 * Then, instantiate the controller like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto pulse_o = stored::PulseWave<stored::YourStore>::objects("/pulse/");
 *
 * // Instantiate the generator, tailored to the available fields in the store.
 * stored::PulseWave<stored::YourStore, pulse_o.flags()> pulse{pulse_o, yourStore};
 * \endcode
 */
template <typename Container, unsigned long long flags = 0, typename T = float>
class PulseWave {
public:
	using type = T;
	using Bound = typename PulseWaveObjects<Container, type>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr PulseWave() noexcept = default;

	/*!
	 * \brief Initialize the pulse wave, given a list of objects and a container.
	 */
	constexpr PulseWave(PulseWaveObjects<Container, type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{
		static_assert(
			Bound::template valid<'s'>(), "'sample frequency' function is mandatory");
	}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return PulseWaveObjects<Container, type>::template create<OnlyId...>(
			prefix, "sample frequency", "amplitude", "frequency", "phase", "duty cycle",
			"override", "output", "enable");
	}

	/*! \brief Return the <tt>sample frequency</tt> object. */
	decltype(auto) sampleFrequencyObject() const noexcept
	{
		return m_o.template get<'s'>();
	}

	/*! \brief Return the sample frequency. */
	float sampleFrequency() const noexcept
	{
		return sampleFrequencyObject()();
	}

	/*! \brief Return the \c amplitude object. */
	decltype(auto) amplitudeObject() const noexcept
	{
		return m_o.template get<'A'>();
	}

	/*! \brief Return the \c amplitude object. */
	decltype(auto) amplitudeObject() noexcept
	{
		return m_o.template get<'A'>();
	}

	/*! \brief Return the \c amplitude value, or 1 when not available. */
	type amplitude() const noexcept
	{
		decltype(auto) o = amplitudeObject();
		return o.valid() ? o.get() : (type)1;
	}

	/*! \brief Return the \c frequency object. */
	decltype(auto) frequencyObject() const noexcept
	{
		return m_o.template get<'f'>();
	}

	/*! \brief Return the \c frequency object. */
	decltype(auto) frequencyObject() noexcept
	{
		return m_o.template get<'f'>();
	}

	/*! \brief Return the \c frequency value, or 1 when not specified. */
	type frequency() const noexcept
	{
		decltype(auto) o = frequencyObject();
		return o.valid() ? o.get() : (type)1;
	}

	/*! \brief Return the \c phase object. */
	decltype(auto) phaseObject() const noexcept
	{
		return m_o.template get<'p'>();
	}

	/*! \brief Return the \c phase object. */
	decltype(auto) phaseObject() noexcept
	{
		return m_o.template get<'p'>();
	}

	/*! \brief Return the \c phase value, or 0 when not available. */
	type phase() const noexcept
	{
		decltype(auto) o = phaseObject();
		return o.valid() ? o.get() : (type)0;
	}

	/*! \brief Return the <tt>duty cycle</tt> object. */
	decltype(auto) dutyCycleObject() const noexcept
	{
		return m_o.template get<'d'>();
	}

	/*! \brief Return the <tt>duty cycle</tt> object. */
	decltype(auto) dutyCycleObject() noexcept
	{
		return m_o.template get<'d'>();
	}

	/*! \brief Return the <tt>duty cycle</tt> value, or 0.5 when not available. */
	type dutyCycle() const noexcept
	{
		decltype(auto) o = dutyCycleObject();
		return o.valid() ? o.get() : (type)0.5;
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or NaN when not available. */
	type override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() const noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output value, or 0 when not available. */
	type output() const noexcept
	{
		decltype(auto) o = outputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() const noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable value, or \c true when not available. */
	bool enabled() const noexcept
	{
		decltype(auto) o = enableObject();
		return !o.valid() || o.get();
	}

	/*!
	 * \brief Enable (or disable) the pulse wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void enable(bool value = true) noexcept
	{
		decltype(auto) o = enableObject();
		if(o.valid())
			o = value;
	}

	/*!
	 * \brief Disable the pulse wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void disable() noexcept
	{
		enable(false);
	}

	/*!
	 * \brief Compute the pulse wave output.
	 */
	type operator()() noexcept
	{
		auto f = frequency();
		type period = f > 0 ? (type)1 / f : 0;

		type output = override_();

		if(likely(std::isnan(output))) {
			if(likely(enabled())) {
				type pulse = period * dutyCycle();

				type t = m_t;
				decltype(auto) po = phaseObject();
				if(po.valid())
					t = std::fmod(
						t
							+ phase() * ((type)1 / ((type)2 * pi<type>))
								  * period,
						period);

				if(t < pulse)
					output = amplitude();
				else
					output = 0;
			} else {
				output = 0;
			}
		}

		if(likely(period > 0)) {
			auto sf = sampleFrequency();
			if(likely(sf > 0)) {
				type dt = (type)(1.0f / sf);
				m_t = std::fmod(m_t + dt, period);
			}
		}

		decltype(auto) oo = outputObject();
		if(oo.valid())
			oo = output;

		return output;
	}

	/*!
	 * \brief Check numerical stability.
	 *
	 * This function checks if for every control interval (1 /
	 * #sampleFrequency()), the output is actually updated.
	 * Especially the period and phase values are checked if they
	 * are not too big.
	 *
	 * You may want to check (or assert on) this function once in a
	 * while, like once per second or after every run, to detect a
	 * stuck controller within reasonable time for your
	 * application.
	 */
	bool isHealthy() const noexcept
	{
		auto sf = sampleFrequency();
		if(sf <= 0)
			return true;

		auto f = frequency();
		if(f <= 0)
			return true;

		auto dt = 1.0f / sf;
		auto period = (type)1 / f;
		if(period + dt == period)
			return false;

		auto ph = phase();
		auto ph_test = (type)10 * f * dt;
		return ph_test + ph != ph;
	}

private:
	Bound m_o;
	type m_t{};
};



//////////////////////////////////////////////////////////
// LowPass
//////////////////////////////////////////////////////////

template <typename Container, typename T = float>
using FirstOrderFilterObjects = FreeObjectsList<
	FreeFunctions<float, Container, 's'>, FreeVariables<T, Container, 'I', 'c', 'F', 'O'>,
	FreeVariables<bool, Container, 'e', 'r'>>;

/*!
 * \brief First-order low- or high-pass filter, based on store variables.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (float) sample frequency (Hz)
 *     float input
 *     float cutoff frequency (Hz)
 *     bool=true enable
 *     bool reset
 *     float=nan override
 *     float output
 * } filter
 * \endcode
 *
 * Only <tt>sample frequency</tt> and <tt>cutoff frequency</tt> are
 * mandatory.  All variables of type \c float, except for <tt>sample
 * frequency</tt>, can be any other type, as long as it matches the
 * template parameter \p T.
 *
 * Then, instantiate the controller like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto filter_o = stored::LowPass<stored::YourStore>::objects("/filter/");
 *
 * // Instantiate the filter, tailored to the available fields in the store.
 * stored::LowPass<stored::YourStore, filter_o.flags()> filter{filter_o, yourStore};
 *
 * // ...or use HighPass instead of LowPass.
 * \endcode
 *
 * The cutoff frequency can be changed while running (by setting \c
 * reset to \c true).  It will applied smoothly; the output will
 * gradually take the new cutoff frequency into account.
 */
template <typename Container, bool LowPass, unsigned long long flags = 0, typename T = float>
class FirstOrderFilter {
public:
	using type = T;
	using Bound = typename FirstOrderFilterObjects<Container, type>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr FirstOrderFilter() noexcept = default;

	/*!
	 * \brief Initialize the filter, given a list of objects and a container.
	 */
	constexpr FirstOrderFilter(
		FirstOrderFilterObjects<Container, type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{
		static_assert(
			Bound::template valid<'s'>(), "'sample frequency' function is mandatory");
		static_assert(
			Bound::template valid<'c'>(), "'cutoff frequency' function is mandatory");
	}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return FirstOrderFilterObjects<Container, type>::template create<OnlyId...>(
			prefix, "sample frequency", "input", "cutoff frequency", "override",
			"output", "enable", "reset");
	}

	/*! \brief Return the <tt>sample frequency</tt> object. */
	decltype(auto) sampleFrequencyObject() const noexcept
	{
		return m_o.template get<'s'>();
	}

	/*! \brief Return the sample frequency. */
	float sampleFrequency() const noexcept
	{
		return sampleFrequencyObject()();
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() const noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c input value, or 0 when not available. */
	type input() const noexcept
	{
		decltype(auto) o = inputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the last input to the filter. */
	type lastInput() const noexcept
	{
		return m_prev_input;
	}

	/*! \brief Return the <tt>cutoff frequency</tt> object. */
	decltype(auto) cutoffFrequencyObject() const noexcept
	{
		return m_o.template get<'c'>();
	}

	/*! \brief Return the <tt>cutoff frequency</tt> object. */
	decltype(auto) cutoffFrequencyObject() noexcept
	{
		return m_o.template get<'c'>();
	}

	/*! \brief Return the <tt>cutoff frequency</tt> value. */
	type cutoffFrequency() const noexcept
	{
		return cutoffFrequencyObject().get();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or NaN when not available. */
	type override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() const noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output value, or 0 when not available. */
	type output() const noexcept
	{
		decltype(auto) o = outputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the last output of the filter. */
	type lastOutput() const noexcept
	{
		return m_prev_output;
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() const noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable value, or \c true when not available. */
	bool enabled() const noexcept
	{
		decltype(auto) o = enableObject();
		return !o.valid() || o.get();
	}

	/*!
	 * \brief Enable (or disable) the pulse wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void enable(bool value = true) noexcept
	{
		decltype(auto) o = enableObject();
		if(o.valid())
			o = value;
	}

	/*!
	 * \brief Disable the pulse wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void disable() noexcept
	{
		enable(false);
	}

	/*! \brief Return the \c reset object. */
	decltype(auto) resetObject() const noexcept
	{
		return m_o.template get<'r'>();
	}

	/*! \brief Return the \c reset object. */
	decltype(auto) resetObject() noexcept
	{
		return m_o.template get<'r'>();
	}

	/*! \brief Return the \c reset value, or \c false when not available. */
	bool reset() const noexcept
	{
		decltype(auto) o = resetObject();
		return o.valid() && o.get();
	}

	/*!
	 * \brief Compute filter output, given an \p input.
	 */
	type operator()(type input) noexcept
	{
		decltype(auto) o = inputObject();
		if(o.valid())
			o = input;

		return run(input);
	}

	/*!
	 * \brief Compute filter output, given the input stored in the store.
	 */
	type operator()() noexcept
	{
		return run(input());
	}

	/*!
	 * \brief Recompute alpha after changed filter parameters.
	 */
	void recomputeCoefficients()
	{
		type cutoff = cutoffFrequency();
		type rc = cutoff > 0 ? (type)1 / ((type)2 * pi<type> * cutoff) : 0;
		auto sf = sampleFrequency();
		type dt = sf > 0 ? (type)(1.0f / sampleFrequency()) : 0;

		if(LowPass)
			m_alpha = dt > 0 ? dt / (rc + dt) : 1;
		else
			m_alpha = rc > 0 ? rc / (rc + dt) : 1;
	}

protected:
	/*!
	 * \brief Compute filter output.
	 */
	type run(type input) noexcept
	{
		type output = override_();

		if(likely(std::isnan(output))) {
			if(!enabled()) {
				m_prev_output = output = input;
			} else {
				bool doReset = false;

				decltype(auto) ro = resetObject();
				if(unlikely(ro.valid() && ro.get())) {
					doReset = true;
					ro = false;
				}

				if(unlikely(std::isnan(m_alpha))) {
					doReset = true;
					m_prev_output = input;
				}

				if(unlikely(doReset)) {
					recomputeCoefficients();

					if(std::isnan(m_prev_output))
						m_prev_output = input;
				}

				if(LowPass) {
					output = m_alpha * input
						 + ((type)1 - m_alpha) * m_prev_output;
				} else {
					output = m_alpha * m_prev_output
						 + m_alpha * (input - m_prev_input);
				}

				m_prev_output = output;
			}
		} else {
			// Save current value, such that we resume smoothly
			// when the override is reset.
			m_prev_output = input;
		}

		m_prev_input = input;

		decltype(auto) oo = outputObject();
		if(oo.valid())
			oo = output;

		return output;
	}

private:
	Bound m_o;
	type m_alpha{std::numeric_limits<type>::quiet_NaN()};
	type m_prev_output{};
	type m_prev_input{};
};

template <typename Container, unsigned long long flags = 0, typename T = float>
using LowPass = FirstOrderFilter<Container, true, flags, T>;

template <typename Container, unsigned long long flags = 0, typename T = float>
using HighPass = FirstOrderFilter<Container, false, flags, T>;



//////////////////////////////////////////////////////////
// Ramp
//////////////////////////////////////////////////////////

template <typename Container, typename T = float>
using RampObjects = FreeObjectsList<
	FreeFunctions<float, Container, 's'>, FreeVariables<T, Container, 'I', 'v', 'a', 'F', 'O'>,
	FreeVariables<bool, Container, 'r', 'e'>>;

/*!
 * \brief Ramping setpoints, based on store variables.
 *
 * This is a quadratic path planner, that creates a smooth path from
 * the current output towards the provided input. The speed and
 * acceleration can be limited.
 *
 * To use this class, add a scope to your store, like:
 *
 * \code
 * {
 *     (float) sample frequency (Hz)
 *     float input
 *     float=inf speed limit
 *     float=inf acceleration limit
 *     bool reset
 *     bool=true enable
 *     float=nan override
 *     float output
 * } ramp
 * \endcode
 *
 * Only <tt>sample frequency</tt> is mandatory.  All variables of type
 * \c float, except for <tt>sample frequency</tt>, can be any other
 * type, as long as it matches the template parameter \p T.
 *
 * Then, instantiate the controller like this:
 *
 * \code
 * // Construct a compile-time object, which resolves all fields in your store.
 * constexpr auto ramp_o = stored::Ramp<stored::YourStore>::objects("/ramp/");
 *
 * // Instantiate the generator, tailored to the available fields in the store.
 * stored::Ramp<stored::YourStore, ramp_o.flags()> ramp{ramp_o, yourStore};
 * \endcode
 *
 * The parameters can be changed while running (when \c reset is set to
 * \c true).  The change will be applied smoothly to the path.
 */
template <typename Container, unsigned long long flags = 0, typename T = float>
class Ramp {
public:
	using type = T;
	using type_ = long;
	using Bound = typename RampObjects<Container, type>::template Bound<flags>;

	/*!
	 * \brief Default ctor.
	 *
	 * Use this when initialization is postponed. You can assign
	 * another instance later on.
	 */
	constexpr Ramp() noexcept = default;

	/*!
	 * \brief Initialize the ramp, given a list of objects and a container.
	 */
	constexpr Ramp(RampObjects<Container, type> const& o, Container& container)
		: m_o{Bound::create(o, container)}
	{
		static_assert(
			Bound::template valid<'s'>(), "'sample frequency' function is mandatory");
	}

	/*!
	 * \brief Create the list of objects in the store, used to compute the \p flags parameter.
	 */
	template <char... OnlyId, size_t N>
	static constexpr auto objects(char const (&prefix)[N]) noexcept
	{
		return RampObjects<Container, type>::template create<OnlyId...>(
			prefix, "sample frequency", "input", "speed limit", "acceleration limit",
			"override", "output", "reset", "enable");
	}

	/*! \brief Return the <tt>sample frequency</tt> object. */
	decltype(auto) sampleFrequencyObject() const noexcept
	{
		return m_o.template get<'s'>();
	}

	/*! \brief Return the sample frequency. */
	float sampleFrequency() const noexcept
	{
		return sampleFrequencyObject()();
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() const noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c input object. */
	decltype(auto) inputObject() noexcept
	{
		return m_o.template get<'I'>();
	}

	/*! \brief Return the \c input value, or 0 when not available. */
	type input() const noexcept
	{
		decltype(auto) o = inputObject();
		return o.valid() ? o.get() : type();
	}

	/*! \brief Return the <tt>speed limit</tt> object. */
	decltype(auto) speedLimitObject() const noexcept
	{
		return m_o.template get<'v'>();
	}

	/*! \brief Return the <tt>speed limit</tt> object. */
	decltype(auto) speedLimitObject() noexcept
	{
		return m_o.template get<'v'>();
	}

	/*! \brief Return the <tt>speed limit</tt> value, or inf when not available. */
	type speedLimit() const noexcept
	{
		decltype(auto) o = speedLimitObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the <tt>acceleration limit</tt> object. */
	decltype(auto) accelerationLimitObject() const noexcept
	{
		return m_o.template get<'a'>();
	}

	/*! \brief Return the <tt>acceleration limit</tt> object. */
	decltype(auto) accelerationLimitObject() noexcept
	{
		return m_o.template get<'a'>();
	}

	/*! \brief Return the <tt>acceleration limit</tt> value, or inf when not available. */
	type accelerationLimit() const noexcept
	{
		decltype(auto) o = accelerationLimitObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::infinity();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() const noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override object. */
	decltype(auto) overrideObject() noexcept
	{
		return m_o.template get<'F'>();
	}

	/*! \brief Return the \c override value, or NaN when not available. */
	type override_() const noexcept
	{
		decltype(auto) o = overrideObject();
		return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN();
	}

	/*! \brief Return the \c reset object. */
	decltype(auto) resetObject() const noexcept
	{
		return m_o.template get<'r'>();
	}

	/*! \brief Return the \c reset object. */
	decltype(auto) resetObject() noexcept
	{
		return m_o.template get<'r'>();
	}

	/*! \brief Return the \c reset value, or \c false when not available. */
	bool reset() const noexcept
	{
		decltype(auto) o = resetObject();
		return o.valid() && o.get();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() const noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable object. */
	decltype(auto) enableObject() noexcept
	{
		return m_o.template get<'e'>();
	}

	/*! \brief Return the \c enable value, or \c true when not available. */
	bool enabled() const noexcept
	{
		decltype(auto) o = enableObject();
		return !o.valid() || o.get();
	}

	/*!
	 * \brief Enable (or disable) the ramp.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void enable(bool value = true) noexcept
	{
		decltype(auto) o = enableObject();
		if(o.valid())
			o = value;
	}

	/*!
	 * \brief Disable the pulse wave.
	 *
	 * Ignored when the \c enable object is not available.
	 */
	void disable() noexcept
	{
		enable(false);
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() const noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output object. */
	decltype(auto) outputObject() noexcept
	{
		return m_o.template get<'O'>();
	}

	/*! \brief Return the \c output value, or 0 when not available. */
	type output() const noexcept
	{
		decltype(auto) o = outputObject();
		return o.valid() ? o.get() : type();
	}

	/*!
	 * \brief Compute the next ramp output, given an input.
	 */
	type operator()(type input) noexcept
	{
		decltype(auto) o = inputObject();
		if(o.valid())
			o = input;

		return run(input);
	}

	/*!
	 * \brief Compute the next ramp output, given the \c input in the store.
	 */
	type operator()() noexcept
	{
		return run(input());
	}

	/*!
	 * \brief Check numerical stability.
	 *
	 * The #Ramp is considered healthy when the configured
	 * acceleration and speed values are within the floating point
	 * precision.
	 *
	 * You may want to check (or assert on) this function once in a
	 * while, like once per second or after every run, to detect a
	 * stuck ramp within reasonable time for your application.
	 */
	bool isHealthy() const noexcept
	{
		if(std::isnan(m_adt))
			// No ramping configured.
			return true;

		if(!(accelerationLimit() > 0))
			// No limit set.
			return true;

		if(!(m_adt > 0))
			// That's not good. Numbers are probably already to far apart.
			return false;

		// m_adt is the smallest value that should be able to influence the position.
		if(m_x + m_adt == m_x)
			return false;

		if(m_start + m_adt == m_start)
			return false;

		return true;
	}

protected:
	/*!
	 * \brief Compute the output of the ramp.
	 *
	 * The implementation uses integers to determine the current speed.
	 * Acceleration is always +/- 1 (times m_adt) per tick.  So, the actual
	 * speed is <tt>m_v_ * m_adt</tt>, and the position is an discrete
	 * offset <tt>m_x_ * m_adt</tt> from \c m_start.
	 *
	 * By using this discrete approach, the distance to stop can be
	 * determined easily.
	 */
	type run(type input) noexcept
	{
		type output = override_();

		if(likely(std::isnan(output))) {
			decltype(auto) ro = resetObject();
			if(unlikely((ro.valid() && ro.get()) || std::isnan(m_adt))) {
				type v = m_adt > 0 ? (type)m_v_ * m_adt : 0;

				if(ro.valid())
					ro = false;

				float f = sampleFrequency();
				type dt = f > 0 ? (type)(1.0f / f) : 0;

				auto sl = speedLimit();
				if(std::isnan(sl) || sl < 0)
					sl = 0;

				auto a = accelerationLimit();
				if(std::isnan(a) || a < 0)
					a = 0;

				// Compute a as the acceleration per tick.
				a = std::min(sl, a * dt);

				if(a == std::numeric_limits<type>::infinity()) {
					// No speed and acceleration limit. Disable ramping.
					a = 0;
				} else if(a > 0) {
					auto v_steps = std::lround(sl / a);
					a = sl / (type)v_steps;
				}

				m_adt = a * dt;
				m_v_ = a > 0 ? (type_)std::lround(v / a) : 0;
				m_v_max_ =
					a > 0 ? std::max<type_>(1, (type_)std::lround(sl / a)) : 0;
				m_x_ = 0;
				m_x_stop_ = m_v_ * std::abs(m_v_) / 2;
				m_start = m_x;
			}

			if(unlikely(!(m_adt > 0))) {
				output = input;
			} else if(unlikely(!enabled())) {
				m_start = output = input;
				m_v_ = (type_)std::lround((output - m_x) / m_adt);
				m_x_ = 0;
				m_x_stop_ = m_v_ * std::abs(m_v_) / 2;
			} else {
				auto err = input - m_x;

				if(std::fabs(err) < m_adt && (m_v_ >= -1 && m_v_ <= 1)) {
					// Close enough. Stop.
					m_x_ = m_x_stop_ = m_v_ = 0;
					output = m_start = input;
				} else if(err > 0) {
					// Should be moving up towards target.
					auto x_stop_ = m_x_stop_;
					auto v_ = m_v_;

					if(v_ < m_v_max_) {
						// Speed up towards target.
						if(v_ >= 0)
							x_stop_ += v_++;
						else
							x_stop_ -= ++v_;
					}

					if(m_v_ > 0 && err < (type)(x_stop_ + v_ + 1) * m_adt) {
						if(err < (type)(m_x_stop_ + m_v_) * m_adt)
							// Break.
							m_x_stop_ -= --m_v_;
						// else hold speed.
					} else {
						m_x_stop_ = x_stop_;
						m_v_ = v_;
					}
				} else {
					// Should be moving down towards target.
					auto x_stop_ = m_x_stop_;
					auto v_ = m_v_;

					if(v_ > -m_v_max_) {
						// Speed up towards target.
						if(v_ <= 0)
							x_stop_ += v_--;
						else
							x_stop_ -= --v_;
					}

					if(m_v_ < 0 && err > (type)(x_stop_ + v_ - 1) * m_adt) {
						if(err > (type)(m_x_stop_ + m_v_) * m_adt)
							// Break.
							m_x_stop_ -= ++m_v_;
						// else hold speed.
					} else {
						m_x_stop_ = x_stop_;
						m_v_ = v_;
					}
				}

				m_x_ += m_v_;
				output = m_start + (type)m_x_ * m_adt;
			}

			m_x = output;
		} else {
			m_v_ = m_x_stop_ = 0;
		}

		decltype(auto) oo = outputObject();
		if(oo.valid())
			oo = output;

		return output;
	}

private:
	Bound m_o;
	type m_adt{std::numeric_limits<type>::quiet_NaN()};
	type_ m_v_{};
	type_ m_v_max_{};
	type_ m_x_{};
	type_ m_x_stop_{};
	type m_start{};
	type m_x{};
};

} // namespace stored

#endif // C++14
#endif // LIBSTORED_COMPONENTS_H
