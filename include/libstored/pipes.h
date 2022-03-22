#ifndef LIBSTORED_PIPES_H
#define LIBSTORED_PIPES_H
/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libstored/macros.h>

#if defined(__cplusplus) && STORED_cplusplus >= 201402L && defined(STORED_DRAFT_API)

#	include <libstored/util.h>
#	include <libstored/types.h>

#	include <array>
#	include <cstdio>
#	include <functional>
#	include <limits>
#	include <new>
#	include <string>
#	include <utility>

#	include <cassert>

namespace stored {
namespace pipes {

// Forward declares.
template <typename S>
class Segment;

class Exit {};
class Cap {};

template <typename In, typename Out>
class Pipe;

template <typename S>
class SpecificCappedPipe;

template <typename S>
class SpecificOpenPipe;



namespace impl {

//////////////////////////////////
// traits
//

template <class U, class T>
struct is_convertible
	: std::integral_constant<
		  bool, (std::is_constructible<T, U>::value || std::is_convertible<U, T>::value)> {
};

#	define STORED_PIPE_TRAITS_HAS_F(type, name)                                              \
	protected:                                                                                \
		template <                                                                        \
			typename type##_,                                                         \
			std::enable_if_t<                                                         \
				std::is_member_function_pointer<decltype(&type##_::name)>::value, \
				int> = 0>                                                         \
		static constexpr bool has_##name##_() noexcept                                    \
		{                                                                                 \
			return true;                                                              \
		}                                                                                 \
                                                                                                  \
		using segment_traits_base::has_##name##_;                                         \
                                                                                                  \
	public:                                                                                   \
		static constexpr bool has_##name = has_##name##_<type>();

struct segment_traits_base {
protected:
	template <typename S_>
	static constexpr bool has_inject_() noexcept
	{
		return false;
	}

	template <typename S_>
	static constexpr bool has_extract_() noexcept
	{
		return false;
	}

	template <typename S_>
	static constexpr bool has_entry_cast_() noexcept
	{
		return false;
	}

	template <typename S_>
	static constexpr bool has_exit_cast_() noexcept
	{
		return false;
	}
};

} // namespace impl

template <typename S>
struct segment_traits : public impl::segment_traits_base {
private:
	template <typename In, typename Out, typename S_>
	static In type_in_helper(Out (S_::*)(In));

	template <typename In, typename Out, typename S_>
	static In type_in_helper(Out (S_::*)(In) const);

	template <typename In, typename Out, typename S_>
	static Out type_out_helper(Out (S_::*)(In));

	template <typename In, typename Out, typename S_>
	static Out type_out_helper(Out (S_::*)(In) const);

#	if STORED_cplusplus >= 201703L
	template <typename In, typename Out, typename S_>
	static In type_in_helper(Out (S_::*)(In) noexcept);

	template <typename In, typename Out, typename S_>
	static In type_in_helper(Out (S_::*)(In) const noexcept);

	template <typename In, typename Out, typename S_>
	static Out type_out_helper(Out (S_::*)(In) noexcept);

	template <typename In, typename Out, typename S_>
	static Out type_out_helper(Out (S_::*)(In) const noexcept);
#	endif // >= C++17

public:
	STORED_PIPE_TRAITS_HAS_F(S, inject)
	static_assert(has_inject, "Needs inject() to be a segment");

	STORED_PIPE_TRAITS_HAS_F(S, extract)
	STORED_PIPE_TRAITS_HAS_F(S, entry_cast)
	STORED_PIPE_TRAITS_HAS_F(S, exit_cast)

	using type_in = decltype(type_in_helper(&S::inject));
	static_assert(
		std::is_default_constructible<std::decay_t<type_in>>::value,
		"Segment's type in must be default-constructible");

	using type_out = decltype(type_out_helper(&S::inject));
	static_assert(
		std::is_default_constructible<std::decay_t<type_out>>::value,
		"Segment's type out must be default-constructible");
};

template <>
struct segment_traits<Exit> : impl::segment_traits_base {
	using type_in = void;
	using type_out = void;
};

template <>
struct segment_traits<Cap> : impl::segment_traits_base {
	using type_in = void;
	using type_out = void;
};

template <typename S>
struct segment_traits<Segment<S>> : segment_traits<S> {};

template <typename... S>
struct segments_traits {};

template <typename S0, typename S1, typename... S>
struct segments_traits<S0, S1, S...> {
	using type_in = typename segment_traits<S0>::type_in;
	using type_out = typename segments_traits<S1, S...>::type_out;
};

template <typename S0>
struct segments_traits<S0> : public segment_traits<S0> {};



//////////////////////////////////
// Segment
//

// Converts a class into a full-blown Segment, adding default implementations
// for all optional functions.
template <typename S>
class Segment : protected S, public segment_traits<S> {
	STORED_CLASS_DEFAULT_COPY_MOVE(Segment)
public:
	using traits = segment_traits<S>;
	using typename traits::type_in;
	using typename traits::type_out;

	template <typename S_>
	constexpr explicit Segment(S_&& s)
		: S{std::forward<S_>(s)}
	{}

	~Segment() = default;

	decltype(auto) inject(type_in x)
	{
		return inject_<S>(x);
	}

	decltype(auto) operator()(type_in x)
	{
		return inject(x);
	}

	decltype(auto) extract()
	{
		return extract_<S>();
	}

	decltype(auto) entry_cast(type_out x) const
	{
		return entry_cast_<S>(x);
	}

	decltype(auto) exit_cast(type_in x) const
	{
		return exit_cast_<S>(x);
	}

private:
	template <typename S_, std::enable_if_t<traits::template has_inject_<S_>(), int> = 0>
	decltype(auto) inject_(type_in x)
	{
		return S_::inject(x);
	}

	template <typename S_, std::enable_if_t<!traits::template has_inject_<S_>(), int> = 0>
	decltype(auto) inject_(type_in x)
	{
		UNUSED(x)
		return extract_<S_>();
	}

	template <typename S_, std::enable_if_t<traits::template has_extract_<S_>(), int> = 0>
	decltype(auto) extract_()
	{
		return S_::extract();
	}

	template <typename S_, std::enable_if_t<!traits::template has_extract_<S_>(), int> = 0>
	auto extract_()
	{
		return type_out{};
	}

	template <typename S_, std::enable_if_t<traits::template has_entry_cast_<S_>(), int> = 0>
	decltype(auto) entry_cast_(type_out x) const
	{
		return S_::entry_cast(x);
	}

	template <
		typename S_, std::enable_if_t<
				     !traits::template has_entry_cast_<S_>()
					     && std::is_same<type_in, type_out>::value,
				     int> = 0>
	type_in entry_cast_(type_out x) const
	{
		return x;
	}

	template <
		typename S_, std::enable_if_t<
				     !traits::template has_entry_cast_<S_>()
					     && !std::is_same<type_in, type_out>::value,
				     int> = 0>
	auto entry_cast_(type_out x) const
	{
		static_assert(
			impl::is_convertible<type_out, std::decay_t<type_in>>::value,
			"Provide entry_cast() or support static_cast<type_in>(type_out)");
		return static_cast<std::decay_t<type_in>>(x);
	}

	template <typename S_, std::enable_if_t<traits::template has_exit_cast_<S_>(), int> = 0>
	decltype(auto) exit_cast_(type_in x) const
	{
		return S_::exit_cast(x);
	}

	template <
		typename S_, std::enable_if_t<
				     !traits::template has_exit_cast_<S_>()
					     && std::is_same<type_in, type_out>::value,
				     int> = 0>
	type_out exit_cast_(type_in x) const
	{
		return x;
	}

	template <
		typename S_, std::enable_if_t<
				     !traits::template has_exit_cast_<S_>()
					     && !std::is_same<type_in, type_out>::value,
				     int> = 0>
	auto exit_cast_(type_in x) const
	{
		static_assert(
			impl::is_convertible<type_in, std::decay_t<type_out>>::value,
			"Provide exit_cast() or support static_cast<type_out>(type_in)");
		return static_cast<std::decay_t<type_out>>(x);
	}
};



//////////////////////////////////
// Segments
//

// Composition of segments (recursive).
template <typename... S>
class Segments {};

template <typename A, typename B>
struct segments_type_join {};

template <typename... A, typename... B>
struct segments_type_join<Segments<A...>, Segments<B...>> {
	using type = Segments<A..., B...>;
};

template <typename... S>
struct segments_type {};

template <typename S0, typename S1, typename... S>
struct segments_type<S0, S1, S...> {
	using init = typename segments_type_join<
		Segments<S0>, typename segments_type<S1, S...>::init>::type;
	using last = typename segments_type<S1, S...>::last;
};

template <typename S0>
struct segments_type<S0> {
	using init = Segments<>;
	using last = S0;
};

template <typename PrevOut, typename Out>
struct segments_type_out {};

template <typename Out>
struct segments_type_out<Out, Out> {
	using type = Out;
};

template <typename Out>
struct segments_type_out<Out, Out const&> {
	using type = Out;
};

template <typename Out>
struct segments_type_out<Out const&, Out> {
	using type = Out;
};

template <typename Out>
struct segments_type_out<Out const&, Out const&> {
	using type = Out const&;
};

namespace impl {
/*
 * Last wraps a single Segment to be used as a Segments base class.  Consider
 * the class Segments<A,A>. In this case, Segments inheritance is:
 *
 *      A
 *      ^
 *      |
 * Segment<A>     A
 *      ^         ^
 *      |         |
 * Segments<A>   Segment<A>
 *      ^         ^
 *     Init      Last
 *      |         |
 *     Segments<A,A>
 *
 * As Segment<A> is a direct base of Segments<A,A>, and a base via Init, so a
 * cast to Segment<A> is ambiguous. When Last = Segment<A> is wrapped in
 * another type, this ambiguity is solved. For this, we use impl::Last.
 */
template <typename... S>
struct Last : public Segment<typename segments_type<S...>::last> {
	using base = Segment<typename segments_type<S...>::last>;
	template <typename S_>
	constexpr explicit Last(S_&& s)
		: base{std::forward<S_>(s)}
	{}
};
} // namespace impl

/*
 * Segments<S...> is a recursive type, containing the given sequence of
 * segments S.  It has two base classes:
 * - Init: a Segments<...>, having all S... types, except for the last one.
 * - Last: a Segment<>, with the last S type.
 *
 * Calls like inject() and extract() recursively process Init, and specifically
 * handle Last.
 *
 * All S types are plain/simple Segment-compatible types (like Identity), where
 * optional functions may be missing.  These types are wrapped in a Segment,
 * such that the interface is completed.
 */
template <typename S0, typename S1, typename... S>
class Segments<S0, S1, S...> : private segments_type<S0, S1, S...>::init,
			       private impl::Last<S0, S1, S...> {
	static_assert(
		std::is_convertible<
			typename segment_traits<S0>::type_out,
			typename segment_traits<S1>::type_in>::value,
		"Incompatible segment interface types");

	static_assert(
		std::is_same<
			std::decay_t<typename segment_traits<S0>::type_out>,
			std::decay_t<typename segment_traits<S1>::type_in>>::value,
		"Different segment types");

public:
	using Init = typename segments_type<S0, S1, S...>::init;
	using Last = impl::Last<S0, S1, S...>;

	using type_in = typename segment_traits<Init>::type_in;
	using type_out = typename segments_type_out<
		decltype(std::declval<Last>().exit_cast(
			std::declval<typename segments_traits<Init>::type_out>())),
		typename segment_traits<Last>::type_out>::type;

protected:
	template <typename... S_, typename SN_>
	constexpr explicit Segments(Segments<S_...>&& init, SN_&& last)
		: Init{std::move(init)}
		, Last{std::forward<SN_>(last)}
	{}

public:
	decltype(auto) inject(type_in x)
	{
		return Last::inject(Init::inject(x));
	}

	static constexpr bool has_inject = Init::has_inject || Last::has_inject;

	decltype(auto) extract()
	{
		return extract_helper<Last>();
	}

	static constexpr bool has_extract = Init::has_extract || Last::has_extract;

	decltype(auto) exit_cast(type_in x) const
	{
		return Last::exit_cast(Init::exit_cast(x));
	}

	static constexpr bool has_exit_cast = Init::has_exit_cast || Last::has_exit_cast;

	decltype(auto) entry_cast(type_out x) const
	{
		return Init::entry_cast(Last::entry_cast(x));
	}

	static constexpr bool has_entry_cast = Init::has_entry_cast || Last::has_entry_cast;

	template <
		typename S_,
		std::enable_if_t<
			std::is_convertible<type_out, typename segment_traits<S_>::type_in>::value
				&& !std::is_lvalue_reference<S_>::value,
			int> = 0>
	constexpr auto operator>>(S_&& s) &&
	{
		return Segments<S0, S1, S..., std::decay_t<S_>>{
			std::move(*this), std::forward<S_>(s)};
	}

	template <typename... S_>
	friend class Segments;

private:
	template <typename Last_, std::enable_if_t<Last_::has_extract, int> = 0>
	decltype(auto) extract_helper()
	{
		return Last_::extract();
	}

	template <typename Last_, std::enable_if_t<!Last_::has_extract, int> = 0>
	decltype(auto) extract_helper()
	{
		return Last_::exit_cast(Init::extract());
	}
};

template <typename S0>
class Segments<S0> : public Segment<S0> {
	STORED_CLASS_DEFAULT_COPY_MOVE(Segments)
public:
	using typename Segment<S0>::type_in;
	using typename Segment<S0>::type_out;

protected:
	template <typename S0_>
	constexpr explicit Segments(S0_&& segment)
		: Segment<S0>{std::forward<S0_>(segment)}
	{}

	template <typename T>
	friend class Entry;

	template <typename... S>
	friend class Segments;

public:
	template <
		typename S_,
		std::enable_if_t<
			std::is_convertible<type_out, typename segment_traits<S_>::type_in>::value
				&& !std::is_lvalue_reference<S_>::value,
			int> = 0>
	constexpr auto operator>>(S_&& s) &&
	{
		return Segments<S0, std::decay_t<S_>>{std::move(*this), std::forward<S_>(s)};
	}
};



//////////////////////////////////
// Pipe
//

// Start of a pipe. Finish with either Exit or Cap.
template <typename T>
class Entry {
public:
	template <
		typename S_,
		std::enable_if_t<
			std::is_convertible<T, typename segment_traits<S_>::type_in>::value, int> =
			0>
	constexpr auto operator>>(S_&& s) &&
	{
		return Segments<std::decay_t<S_>>{std::forward<S_>(s)};
	}
};

// Virtual base class for any segment/pipe with specific in/out types.
template <typename In>
class PipeEntry {
	STORED_CLASS_DEFAULT_COPY_MOVE(PipeEntry)
public:
	using type_in = In;

protected:
	constexpr PipeEntry() = default;

public:
	virtual ~PipeEntry() = default;

	void inject(type_in const& x)
	{
		justInject(x);
	}

	void operator()(type_in const& x)
	{
		inject(x);
	}

	friend void operator>>(type_in const& x, PipeEntry& p)
	{
		return p.inject(x);
	}

	friend void operator<<(PipeEntry& p, type_in const& x)
	{
		return p.inject(x);
	}

private:
	virtual void justInject(type_in const& x) = 0;
};

template <typename T>
class ExitValue {
public:
	using type = T;

protected:
	explicit ExitValue(type const& v)
		: m_p{&v}
	{}

	explicit ExitValue(type&& v)
		: m_p{}
	{
		new(m_v) type{std::move(v)};
	}

	template <typename S>
	friend class SpecificCappedPipe;

public:
	ExitValue(ExitValue const&) = delete;
	void operator=(ExitValue const&) = delete;

	ExitValue(ExitValue&& v) noexcept
		: m_p{v.m_p}
	{
		if(m_p)
			new(m_v) type{std::move(v.value())};
	}

	ExitValue& operator=(ExitValue&& v) = delete;

	~ExitValue()
	{
		if(!m_p)
			value().~type();
	}

	T const& get() const
	{
		return m_p ? *m_p : value();
	}

	operator type const &() const
	{
		return get();
	}

	type& move(type& dst) &&
	{
		if(m_p)
			dst = *m_p;
		else
			dst = std::move(value());

		return dst;
	}

#	define EXITVALUE_COMPARE_OP(op)                                         \
		friend bool operator op(ExitValue<type> const& v, type const& x) \
		{                                                                \
			return v.get() op x;                                     \
		}                                                                \
		friend bool operator op(type const& x, ExitValue<type> const& v) \
		{                                                                \
			return v.get() op x;                                     \
		}

	EXITVALUE_COMPARE_OP(==)
	EXITVALUE_COMPARE_OP(!=)
	EXITVALUE_COMPARE_OP(>)
	EXITVALUE_COMPARE_OP(<)
	EXITVALUE_COMPARE_OP(>=)
	EXITVALUE_COMPARE_OP(<=)

#	undef EXITVALUE_COMPARE_OP

protected:
	type& value()
	{
		return *reinterpret_cast<type*>(&m_v[0]);
	}

	type const& value() const
	{
		return *reinterpret_cast<type const*>(&m_v[0]);
	}

private:
	alignas(type) char m_v[sizeof(type)]; // only valid when m_p == nullptr
	type const* m_p;
};


template <typename Out>
class PipeExit {
	STORED_CLASS_DEFAULT_COPY_MOVE(PipeExit)
public:
	using type_out = Out;
	using type_out_wrapper = ExitValue<Out>;

protected:
	constexpr PipeExit() = default;

public:
	virtual ~PipeExit() = default;

	virtual type_out_wrapper extract() = 0;

	friend auto& operator>>(PipeExit& p, std::decay_t<type_out>& x)
	{
		return p.extract().move(x);
	}

	friend auto& operator<<(std::decay_t<type_out>& x, PipeExit& p)
	{
		return p.extract().move(x);
	}

	virtual void connect(PipeEntry<Out>& p)
	{
		UNUSED(p)
		// Do not connect a capped pipe.
		std::unexpected();
	}

	template <typename Out_>
	Pipe<Out, Out_>& operator>>(Pipe<Out, Out_>& p) &
	{
		connect(p);
		return p;
	}

	virtual void disconnect() noexcept {}

	virtual PipeEntry<Out>* connection() const noexcept
	{
		return nullptr;
	}

	bool connected() const noexcept
	{
		return connection();
	}

	void extend(Pipe<Out, Out>& p)
	{
		auto* c = connection();
		if(c)
			p.connect(*c);
		else
			p.disconnect();

		connect(p);
	}
};

template <typename In, typename Out>
class Pipe : public PipeEntry<In>, public PipeExit<Out> {
	STORED_CLASS_DEFAULT_COPY_MOVE(Pipe)
public:
	using typename PipeEntry<In>::type_in;
	using typename PipeExit<Out>::type_out;
	using typename PipeExit<Out>::type_out_wrapper;

protected:
	constexpr Pipe() = default;

public:
	virtual ~Pipe() override = default;

	virtual type_out_wrapper inject(type_in const& x) = 0;

	type_out_wrapper operator()(type_in const& x)
	{
		return inject(x);
	}

	friend type_out_wrapper operator>>(type_in const& x, Pipe& p)
	{
		return p.inject(x);
	}

	friend type_out_wrapper operator<<(Pipe& p, type_in const& x)
	{
		return p.inject(x);
	}

private:
	void justInject(type_in const& x) final
	{
		inject(x);
	}
};

// Concrete implementation of Pipe, given a segment/pipe.
template <typename S>
class SpecificCappedPipe : public Pipe<
				   std::decay_t<typename segment_traits<S>::type_in>,
				   std::decay_t<typename segment_traits<S>::type_out>>,
			   public S {
	STORED_CLASS_DEFAULT_COPY_MOVE(SpecificCappedPipe)
public:
	using segments_type = S;
	using type_in = std::decay_t<typename segment_traits<segments_type>::type_in>;
	using type_out = std::decay_t<typename segment_traits<segments_type>::type_out>;
	using type_out_wrapper = ExitValue<type_out>;
	using Pipe_type = Pipe<type_in, type_out>;

protected:
	template <
		typename S_,
		std::enable_if_t<std::is_constructible<segments_type, S_>::value, int> = 0>
	explicit SpecificCappedPipe(S_&& s)
		: segments_type{std::forward<S_>(s)}
	{}

	template <typename... S_>
	friend class Segments;

public:
	virtual ~SpecificCappedPipe() override = default;

	virtual type_out_wrapper inject(type_in const& x) override
	{
		return type_out_wrapper{segments_type::inject(x)};
	}

	virtual type_out_wrapper extract() override
	{
		return type_out_wrapper{segments_type::extract()};
	}

	using Pipe_type::operator();
	using Pipe_type::operator>>;

	template <typename... S_>
	friend constexpr auto operator>>(Segments<S_...>&& s, Cap&& e);
};

template <typename... S_>
constexpr auto operator>>(Segments<S_...>&& s, Cap&& e)
{
	UNUSED(e)
	return SpecificCappedPipe<Segments<S_...>>{std::move(s)};
}

template <typename S>
class SpecificOpenPipe : public SpecificCappedPipe<S> {
	STORED_CLASS_DEFAULT_COPY_MOVE(SpecificOpenPipe)
	using base = SpecificCappedPipe<S>;

public:
	using segments_type = S;
	using type_in = typename base::type_in;
	using type_out = typename base::type_out;
	using type_out_wrapper = typename base::type_out_wrapper;
	using Pipe_type = typename base::Pipe_type;

protected:
	template <
		typename S_,
		std::enable_if_t<std::is_constructible<segments_type, S_>::value, int> = 0>
	explicit SpecificOpenPipe(S_&& s)
		: base{std::forward<S_>(s)}
	{}

	template <typename... S_>
	friend class Segments;

public:
	virtual ~SpecificOpenPipe() override = default;

	virtual type_out_wrapper inject(type_in const& x) override
	{
		type_out_wrapper y = base::inject(x);
		forward(y);
		return y;
	}

	virtual void connect(PipeEntry<type_out>& p) final
	{
		m_forward = &p;
		if(this->has_extract)
			forward(this->extract());
	}

	virtual void disconnect() noexcept final
	{
		m_forward = nullptr;
	}

	template <typename... S_>
	friend constexpr auto operator>>(Segments<S_...>&& s, Exit&& e);

	virtual PipeEntry<type_out>* connection() const noexcept final
	{
		return m_forward;
	}

private:
	void forward(type_out const& x)
	{
		if(m_forward)
			m_forward->inject(x);
	}

private:
	PipeEntry<type_out>* m_forward = nullptr;
};

template <typename... S_>
constexpr auto operator>>(Segments<S_...>&& s, Exit&& e)
{
	UNUSED(e)
	return SpecificOpenPipe<Segments<S_...>>{std::move(s)};
}


//////////////////////////////////
// Common segments
//

/*!
 * \brief Pipe segment without side effects.
 */
template <typename T>
class Identity {
public:
	T const& inject(T const& x)
	{
		return x;
	}
};

template <typename T>
auto operator>>(Entry<T>&& entry, Exit&& exit)
{
	return std::move(entry) >> Identity<T>{} >> std::move(exit);
}

template <typename T>
auto operator>>(Entry<T>&& entry, Cap&& cap)
{
	return std::move(entry) >> Identity<T>{} >> std::move(cap);
}

namespace impl {

template <
	typename In, typename Out,
	bool is_number =
		std::numeric_limits<In>::is_specialized&& std::numeric_limits<Out>::is_specialized>
class Cast {};

// This is a cast between numbers. Use saturated_cast<>.
template <typename In, typename Out>
class Cast<In, Out, true> {
public:
	Out inject(In x)
	{
		return exit_cast(x);
	}

	Out exit_cast(In x) const
	{
		return saturated_cast<Out>(x);
	}

	In entry_cast(Out x) const
	{
		return saturated_cast<In>(x);
	}
};

// This is a cast between other types than numbers. Use static_cast<>.
template <typename In, typename Out>
class Cast<In, Out, false> {
public:
	Out inject(In const& x)
	{
		return exit_cast(x);
	}

	Out exit_cast(In const& x) const
	{
		return static_cast<Out>(x);
	}

	In entry_cast(Out const& x) const
	{
		return static_cast<In>(x);
	}
};

template <typename T, bool is_number>
class Cast<T, T, is_number> : public Identity<T> {};

} // namespace impl

/*!
 * \brief A pipe segment that casts between types.
 *
 * Numeric types are cast using \c saturated_cast. For other types, \c
 * static_cast is used.
 */
template <typename In, typename Out>
using Cast = impl::Cast<In, Out>;

/*!
 * \brief Memory element for injected values.
 *
 * Extracted data is returned from this memory.
 */
template <typename T>
class Buffer {
public:
	using type_in = T;
	using type = std::decay_t<type_in>;
	using type_out = type const&;

	template <
		typename type_,
		std::enable_if_t<std::is_constructible<type, type_>::value, int> = 0>
	constexpr explicit Buffer(type_&& x)
		: m_x{std::forward<type_>(x)}
	{}

	constexpr Buffer() = default;

	type_out inject(type_in const& x)
	{
		m_x = x;
		return extract();
	}

	type_out extract()
	{
		return m_x;
	}

private:
	type m_x{};
};

/*!
 * \brief Forwards injected data into a fixed list of other pipes.
 */
template <typename T, size_t N>
class Tee {
public:
	template <typename... P, std::enable_if_t<sizeof...(P) + 1 == N, int> = 0>
	constexpr explicit Tee(PipeEntry<T>& p0, P&... p)
		: m_p{p0, p...}
	{}

	T const& inject(T const& x)
	{
		for(auto& p : m_p)
			p.get().inject(x);

		return x;
	}

private:
	std::array<std::reference_wrapper<PipeEntry<T>>, N> m_p;
};

#	if STORED_cplusplus >= 201703L
template <typename T, typename... P>
Tee(PipeEntry<T>&, P&...) -> Tee<T, sizeof...(P) + 1>;
#	endif // >= C++17

/*!
 * \brief Invokes a logger function for injected values.
 *
 * By default, the value is printed to stdout. If a custom logger function is
 * provided, it must accept a name (std::string const&) and value (T).
 */
template <typename T>
class Log {
public:
	using type = T;
	using logger_type = void(std::string const&, type const&);

	template <
		typename F,
		std::enable_if_t<std::is_assignable<std::function<logger_type>, F>::value, int> = 0>
	Log(std::string name, F&& logger)
		: m_name{std::move(name)}
		, m_logger{std::forward<F>(logger)}
	{}

	explicit Log(std::string name)
		: m_name{std::move(name)}
		, m_logger{&Log::template print<type>}
	{}

	type const& inject(type const& x)
	{
		if(m_logger)
			m_logger(m_name, x);

		return x;
	}

protected:
	template <typename T_, std::enable_if_t<std::is_constructible<double, T_>::value, int> = 0>
	static void print(std::string const& name, T_ const& x)
	{
		printf("%s = %g\n", name.c_str(), static_cast<double>(x));
	}

	template <typename T_, std::enable_if_t<!std::is_constructible<double, T_>::value, int> = 0>
	static void print(std::string const& name, T_ const& x)
	{
		printf("%s injected\n", name.c_str());
	}

private:
	std::string m_name;
	std::function<void(std::string const&, type const&)> m_logger;
};

/*!
 * \brief Pass either the value or a default-constructed value through the
 *	pipe, depending on a gate pipe.
 *
 * The gate pipe value is extracted upon every inject.
 */
template <typename T, bool invert = false, typename Gate = bool>
class Transistor {
public:
	explicit Transistor(PipeExit<Gate>& gate)
		: m_gate{&gate}
	{}

	T inject(T const& x)
	{
		if(invert)
			return m_gate->extract() ? T{} : x;
		else
			return m_gate->extract() ? x : T{};
	}

private:
	PipeExit<Gate>* m_gate;
};

/*!
 * \brief Invoke a function for every inject.
 *
 * The function prototype can be:
 *
 * - void(T)
 * - void(T const&)
 * - void(T&)
 * - T(T)
 *
 * The first two cases allow a function to observe the injected value.  The
 * last two cases allow modifying it; the modified/returned value is passed
 * downstream.
 *
 * T and F are auto-deducted in C++17.
 */
template <typename T, typename F = void(T)>
class Call {
public:
	using function_type = F;

	template <
		typename F_,
		std::enable_if_t<
			std::is_constructible<std::function<function_type>, F_>::value, int> = 0>
	explicit Call(F_&& f)
		: m_f{std::forward<F_>(f)}
	{}

	T const& inject(T const& x)
	{
		m_f(x);
		return x;
	}

private:
	std::function<function_type> m_f;
};

template <typename T>
class Call<T, void(T&)> {
public:
	using function_type = void(T&);

	template <
		typename F_,
		std::enable_if_t<
			std::is_constructible<std::function<function_type>, F_>::value, int> = 0>
	explicit Call(F_&& f)
		: m_f{std::forward<F_>(f)}
	{}

	T inject(T x)
	{
		m_f(x);
		return x;
	}

private:
	std::function<function_type> m_f;
};

template <typename T>
class Call<T, T(T)> {
public:
	using function_type = T(T);

	template <
		typename F_,
		std::enable_if_t<
			std::is_constructible<std::function<function_type>, F_>::value, int> = 0>
	explicit Call(F_&& f)
		: m_f{std::forward<F_>(f)}
	{}

	T inject(T x)
	{
		return m_f(std::move(x));
	}

private:
	std::function<function_type> m_f;
};

#	if STORED_cplusplus >= 201703L
namespace impl {
template <typename F>
struct call_type_in {};

template <typename R, typename A>
struct call_type_in<R(A)> {
	using type = std::decay_t<A>;
};

template <typename F>
struct call_type_in<std::function<F>> : public call_type_in<F> {};

template <typename F>
struct call_f_type {};

template <typename F>
struct call_f_type<std::function<F>> {
	using type = F;
};
} // namespace impl

template <typename F_>
Call(F_ &&)
	-> Call<typename impl::call_type_in<decltype(std::function{std::declval<F_>()})>::type,
		typename impl::call_f_type<decltype(std::function{std::declval<F_>()})>::type>;
#	endif // >= C++17

/*!
 * \brief Extract a store object's value upon every extract/inject.
 *
 * The value passed through the pipe entry is ignored.
 *
 * V can be a stored::Variant, and any reference to a fixed-type store object.
 * Usually, use C++17 auto-deduction to determine both T and V.
 */
template <typename T, typename V>
class Get {};

template <typename T, typename Container>
class Get<T, stored::Variant<Container>> {
public:
	using Variant_type = stored::Variant<Container>;

	explicit Get(Variant_type v)
		: m_v{v}
	{}

	T inject(bool x)
	{
		UNUSED(x)
		return extract();
	}

	T extract()
	{
		return m_v.valid() ? m_v.template get<T>() : T{};
	}

	bool entry_cast(T x) const
	{
		UNUSED(x)
		return false;
	}

	T exit_cast(bool x) const
	{
		UNUSED(x)
		return T{};
	}

private:
	Variant_type m_v;
};

template <typename T, typename Object>
class Get<T, Object&> {
public:
	explicit Get(Object& o)
		: m_o{o}
	{}

	T inject(bool x)
	{
		UNUSED(x)
		return extract();
	}

	T extract()
	{
		return m_o.get().get();
	}

	bool entry_cast(T x) const
	{
		UNUSED(x)
		return false;
	}

	T exit_cast(bool x) const
	{
		UNUSED(x)
		return T{};
	}

private:
	std::reference_wrapper<Object> m_o;
};

#	if STORED_cplusplus >= 201703L
namespace impl {
template <typename V>
struct object_data_type {};

template <typename Store, typename Implementation, typename T, size_t offset, size_t size_>
struct object_data_type<stored::impl::StoreVariable<Store, Implementation, T, offset, size_>> {
	using type = T;
};

template <
	typename Store, typename Implementation,
	template <typename, unsigned int> class FunctionMap, unsigned int F>
struct object_data_type<stored::impl::StoreFunction<Store, Implementation, FunctionMap, F>> {
	using type =
		typename stored::impl::StoreFunction<Store, Implementation, FunctionMap, F>::type;
};

template <typename Store, typename Implementation, Type::type type_, size_t offset, size_t size_>
struct object_data_type<stored::impl::StoreVariantV<Store, Implementation, type_, offset, size_>> {
	static_assert(type_ & stored::Type::FlagFixed, "Only fixed types are supported");
	using type = typename stored::fromType<type_>::type;
};

template <typename Store, typename Implementation, Type::type type_, unsigned int F, size_t size_>
struct object_data_type<stored::impl::StoreVariantF<Store, Implementation, type_, F, size_>> {
	static_assert(type_ & stored::Type::FlagFixed, "Only fixed types are supported");
	using type = typename stored::fromType<type_>::type;
};

template <typename V>
struct object_type {};

template <typename Store, typename Implementation, typename T, size_t offset, size_t size_>
struct object_type<stored::impl::StoreVariable<Store, Implementation, T, offset, size_>> {
	using type = stored::impl::StoreVariable<Store, Implementation, T, offset, size_>&;
};

template <
	typename Store, typename Implementation,
	template <typename, unsigned int> class FunctionMap, unsigned int F>
struct object_type<stored::impl::StoreFunction<Store, Implementation, FunctionMap, F>> {
	using type = stored::impl::StoreFunction<Store, Implementation, FunctionMap, F>&;
};

template <typename Store, typename Implementation, Type::type type_, size_t offset, size_t size_>
struct object_type<stored::impl::StoreVariantV<Store, Implementation, type_, offset, size_>> {
	using type = typename stored::impl::StoreVariantV<
		Store, Implementation, type_, offset, size_>::Variant_type;
};

template <typename Store, typename Implementation, Type::type type_, unsigned int F, size_t size_>
struct object_type<stored::impl::StoreVariantF<Store, Implementation, type_, F, size_>> {
	using type = typename stored::impl::StoreVariantF<
		Store, Implementation, type_, F, size_>::Variant_type;
};
} // namespace impl

// It can only deduct StoreVariable/StoreFunction. Variant does not include the
// type in the template, so T would be unknown, although it is perfectly
// supported by Get. StoreVariant* is supported, as long as it holds fixed-size
// data (which is usually not what it is used for...)
template <typename V>
Get(V &&)
	-> Get<typename impl::object_data_type<std::decay_t<V>>::type,
	       typename impl::object_type<std::decay_t<V>>::type>;
#	endif // C++17

/*!
 * \brief Write a value that is injected in the pipe to a store object.
 * \see Get
 */
template <typename T, typename V>
class Set {};

template <typename T, typename Container>
class Set<T, stored::Variant<Container>> {
public:
	using Variant_type = stored::Variant<Container>;

	explicit Set(Variant_type v)
		: m_v{v}
	{}

	T const& inject(T const& x)
	{
		if(m_v.valid())
			m_v.template set<T>(x);

		return x;
	}

	T extract()
	{
		return m_v.valid() ? m_v.template get<T>() : T{};
	}

private:
	Variant_type m_v;
};

template <typename T, typename Object>
class Set<T, Object&> {
public:
	explicit Set(Object& o)
		: m_o{o}
	{}

	T const& inject(T const& x)
	{
		m_o.get().set(x);
		return x;
	}

	T extract()
	{
		return m_o.get().get();
	}

private:
	std::reference_wrapper<Object> m_o;
};

#	if STORED_cplusplus >= 201703L
template <typename V>
Set(V &&)
	-> Set<typename impl::object_data_type<std::decay_t<V>>::type,
	       typename impl::object_type<std::decay_t<V>>::type>;
#	endif // C++17

/*!
 * \brief Multiplex pipes, given the injected index value.
 *
 * Pipes are saved as references. Given the injected value, the corresponding
 * pipe is extracted when required.
 */
template <typename T, size_t N>
class Mux {
public:
	template <typename... P, std::enable_if_t<sizeof...(P) + 1 == N, int> = 0>
	constexpr explicit Mux(PipeExit<T>& p0, P&... p)
		: m_p{p0, p...}
	{}

	T inject(size_t i)
	{
		m_i = i;
		return extract();
	}

	T extract()
	{
		return m_i < N ? m_p[m_i].get().extract() : T{};
	}

	size_t entry_cast(T x) const
	{
		UNUSED(x)
		return 0;
	}

	T exit_cast(size_t x) const
	{
		UNUSED(x)
		return T{};
	}

private:
	std::array<std::reference_wrapper<PipeExit<T>>, N> m_p;
	size_t m_i = 0;
};

template <typename T>
class Mux<T, 1> {
public:
	constexpr explicit Mux(PipeExit<T>& p)
		: m_p{p}
	{}

	T inject(size_t i)
	{
		UNUSED(i)
		return extract();
	}

	decltype(auto) extract()
	{
		return m_p.get().extract();
	}

	size_t entry_cast(T x) const
	{
		UNUSED(x)
		return 0;
	}

	T exit_cast(size_t x) const
	{
		UNUSED(x)
		return T{};
	}

private:
	std::reference_wrapper<PipeExit<T>> m_p;
};

#	if STORED_cplusplus >= 201703L
template <typename T, typename... P>
Mux(PipeExit<T>&, P&...) -> Mux<T, sizeof...(P) + 1>;
#	endif // >= C++17

} // namespace pipes
} // namespace stored

#endif // __cplusplus
#endif // LIBSTORED_PIPES_H
