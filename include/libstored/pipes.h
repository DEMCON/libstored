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

#	include <array>
#	include <cstdio>
#	include <functional>
#	include <string>
#	include <utility>
#	include <limits>

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
	using type_out = decltype(type_out_helper(&S::inject));
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

	type_out inject(type_in x)
	{
		return inject_<S>(x);
	}

	type_out operator()(type_in x)
	{
		return inject(x);
	}

	friend type_out operator<<(Segment& s, type_in x)
	{
		return s.inject(x);
	}

	friend type_out operator>>(type_in x, Segment& s)
	{
		return s.inject(x);
	}

	type_out extract()
	{
		return extract_<S>();
	}

	friend auto operator<<(type_out& x, Segment& s)
	{
		return x = s.extract();
	}

	friend auto operator>>(Segment& s, type_out& x)
	{
		return x = s.extract();
	}

	auto entry_cast(type_out x) const
	{
		return entry_cast_<S>(x);
	}

	auto exit_cast(type_in x) const
	{
		return exit_cast_<S>(x);
	}

private:
	template <typename S_, std::enable_if_t<traits::template has_inject_<S_>(), int> = 0>
	type_out inject_(type_in x)
	{
		return S_::inject(x);
	}

	template <typename S_, std::enable_if_t<!traits::template has_inject_<S_>(), int> = 0>
	type_out inject_(type_in x)
	{
		UNUSED(x)
		return extract_<S_>();
	}

	template <typename S_, std::enable_if_t<traits::template has_extract_<S_>(), int> = 0>
	type_out extract_()
	{
		return S_::extract();
	}

	template <typename S_, std::enable_if_t<!traits::template has_extract_<S_>(), int> = 0>
	type_out extract_()
	{
		return type_out{};
	}

	template <typename S_, std::enable_if_t<traits::template has_entry_cast_<S_>(), int> = 0>
	auto entry_cast_(type_out x) const
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
		return static_cast<std::decay_t<type_in>>(x);
	}

	template <typename S_, std::enable_if_t<traits::template has_exit_cast_<S_>(), int> = 0>
	auto exit_cast_(type_in x) const
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
	using type = Out;
};

template <typename S0, typename S1, typename... S>
class Segments<S0, S1, S...> {
	static_assert(
		std::is_convertible<
			typename segment_traits<S0>::type_out,
			typename segment_traits<S1>::type_in>::value,
		"Incompatible segment types");
	static_assert(
		std::is_same<
			std::decay_t<typename segment_traits<S0>::type_out>,
			std::decay_t<typename segment_traits<S1>::type_in>>::value,
		"Different pipe connection types");

public:
	using Init = typename segments_type<S0, S1, S...>::init;
	using Last = Segment<typename segments_type<S0, S1, S...>::last>;

	using type_in = typename segment_traits<Init>::type_in;
	using type_out = typename segments_type_out<
		decltype(std::declval<Last>().exit_cast(
			std::declval<typename segments_traits<Init>::type_out>())),
		typename segment_traits<Last>::type_out>::type;

protected:
	template <typename... S_, typename SN_>
	constexpr explicit Segments(Segments<S_...>&& init, SN_&& last)
		: m_init{std::move(init)}
		, m_last{std::forward<SN_>(last)}
	{}

public:
	type_out inject(type_in x)
	{
		return m_last(m_init.inject(x));
	}

	type_out extract()
	{
		if(m_last.has_extract)
			return m_last.extract();

		return m_last.exit_cast(m_init.extract());
	}

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
	Init m_init;
	Last m_last;
};

template <typename S0>
class Segments<S0> : public Segment<S0> {
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
		return Segments<Segment<S0>, std::decay_t<S_>>{
			std::move(*this), std::forward<S_>(s)};
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
		p.inject(x);
	}

	friend void operator<<(PipeEntry& p, type_in const& x)
	{
		p.inject(x);
	}

private:
	virtual void justInject(type_in const& x) = 0;
};

template <typename Out>
class PipeExit {
	STORED_CLASS_DEFAULT_COPY_MOVE(PipeExit)
public:
	using type_out = Out;

protected:
	constexpr PipeExit() = default;

public:
	virtual ~PipeExit() = default;

	virtual type_out extract() = 0;

	friend type_out& operator>>(PipeExit& p, type_out& x)
	{
		return x = p.extract();
	}

	friend type_out& operator<<(type_out& x, PipeExit& p)
	{
		return x = p.extract();
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
};

template <typename In, typename Out>
class Pipe : public PipeEntry<In>, public PipeExit<Out> {
	STORED_CLASS_DEFAULT_COPY_MOVE(Pipe)
public:
	using typename PipeEntry<In>::type_in;
	using typename PipeExit<Out>::type_out;

protected:
	constexpr Pipe() = default;

public:
	virtual ~Pipe() override = default;

	virtual type_out inject(type_in const& x) = 0;

	type_out operator()(type_in const& x)
	{
		return inject(x);
	}

	friend type_out operator>>(type_in const& x, Pipe& p)
	{
		p.inject(x);
	}

	friend type_out operator<<(Pipe& p, type_in const& x)
	{
		p.inject(x);
	}

private:
	void justInject(type_in const& x) final
	{
		inject(x);
	}
};

// Concrete implementation of Pipe, given a segment/pipe.
template <typename S>
class SpecificCappedPipe
	: public Pipe<typename segment_traits<S>::type_in, typename segment_traits<S>::type_out>,
	  public S {
	STORED_CLASS_DEFAULT_COPY_MOVE(SpecificCappedPipe)
public:
	using segments_type = S;
	using type_in = typename segment_traits<segments_type>::type_in;
	using type_out = typename segment_traits<segments_type>::type_out;
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

	virtual type_out inject(type_in const& x) override
	{
		type_out y = segments_type::inject(x);
		return y;
	}

	virtual type_out extract() override
	{
		return segments_type::extract();
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

	virtual type_out inject(type_in const& x) override
	{
		type_out y = base::inject(x);
		forward(y);
		return y;
	}

	virtual void connect(PipeEntry<type_out>& p) final
	{
		m_forward = &p;
		forward(this->extract());
	}

	virtual void disconnect() noexcept final
	{
		m_forward = nullptr;
	}

	template <typename... S_>
	friend constexpr auto operator>>(Segments<S_...>&& s, Exit&& e);

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

template <typename T>
class Identity {
public:
	T inject(T x)
	{
		return x;
	}
};

template <typename T>
auto operator>>(Entry<T>&& entry, Exit&& e)
{
	return std::move(entry) >> Identity<T>{} >> std::move(e);
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
	Out inject(In x)
	{
		return exit_cast(x);
	}

	Out exit_cast(In x) const
	{
		return static_cast<Out>(x);
	}

	In entry_cast(Out x) const
	{
		return static_cast<In>(x);
	}
};

template <typename T>
class Cast<T, T, false> : public Identity<T> {};

template <typename T>
class Cast<T, T, true> : public Identity<T> {};

} // namespace impl

template <typename In, typename Out>
using Cast = impl::Cast<In, Out>;

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

	type_out inject(type_in x)
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

template <typename T, size_t N>
class Tee {
public:
	template <typename... P, std::enable_if_t<sizeof...(P) + 1 == N, int> = 0>
	constexpr explicit Tee(PipeEntry<T>& p0, P&... p)
		: m_p{p0, p...}
	{}

	T inject(T x)
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

template <typename T>
class Log {
public:
	template <typename F>
	Log(std::string name, F&& logger)
		: m_name{std::move(name)}
		, m_logger{std::forward<F>(logger)}
	{}

	Log(std::string name)
		: m_name{std::move(name)}
		, m_logger{&Log::template print<T>}
	{}

	T inject(T x)
	{
		if(m_logger)
			m_logger(m_name, x);

		return x;
	}

protected:
	template <typename T_, std::enable_if_t<std::is_constructible<double, T_>::value, int> = 0>
	static void print(std::string const& name, T x)
	{
		printf("%s = %g\n", name.c_str(), static_cast<double>(x));
	}

	template <typename T_, std::enable_if_t<!std::is_constructible<double, T_>::value, int> = 0>
	static void print(std::string const& name, T x)
	{
		printf("%s injected\n", name.c_str());
	}

private:
	std::string m_name;
	std::function<void(std::string const&, T)> m_logger;
};

} // namespace pipes
} // namespace stored

#endif // __cplusplus
#endif // LIBSTORED_PIPES_H
