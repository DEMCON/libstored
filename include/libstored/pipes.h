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

#	include <cassert>

namespace stored {
namespace pipes {

// Forward declares.
template <typename S>
class Segment;


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

template <typename S>
struct segment_traits<Segment<S>> : segment_traits<S> {
	using typename segment_traits<S>::type_in;
	using typename segment_traits<S>::type_out;
};

template <typename... S>
struct segments_traits {};

template <typename S0, typename S1, typename... S>
struct segments_traits<S0, S1, S...> {
	using type_in = typename segment_traits<S0>::type_in;
	using type_out = typename segments_traits<S1, S...>::type_out;
};

template <typename S0>
struct segments_traits<S0> : public segment_traits<S0> {};

template <>
struct segments_traits<> {};



//////////////////////////////////
// Segment
//

// Converts a class into a full-blown Segment, adding default implementations
// for all optional functions.
template <typename S>
class Segment : protected S, public segment_traits<S> {
public:
	using traits = segment_traits<S>;
	using typename traits::type_in;
	using typename traits::type_out;

	template <typename S_>
	constexpr explicit Segment(S_&& s)
		: S{std::forward<S_>(s)}
	{}

	Segment(Segment const&) = default;
	Segment(Segment&&) = default;
	Segment& operator=(Segment const&) = default;
	Segment& operator=(Segment&&) = default;
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
		(void)x;
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

class Exit {
public:
	// Seems to be required for segment_traits to work.
	int inject(int);
};

template <typename S>
class SpecificPipe;

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
			std::is_convertible<type_out, typename segment_traits<S_>::type_in>::value,
			int> = 0>
	constexpr auto operator>>(S_&& s) &&
	{
		return Segments<S0, S1, S..., std::decay_t<S_>>{
			std::move(*this), std::forward<S_>(s)};
	}

	SpecificPipe<Segments> operator>>(Exit&& e) &&;

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
			std::is_convertible<type_out, typename segment_traits<S_>::type_in>::value,
			int> = 0>
	constexpr auto operator>>(S_&& s) &&
	{
		return Segments<Segment<S0>, std::decay_t<S_>>{
			std::move(*this), std::forward<S_>(s)};
	}

	SpecificPipe<Segments> operator>>(Exit&& e) &&;
};



//////////////////////////////////
// Pipe
//

template <typename T>
class Entry {
public:
	template <
		typename S,
		std::enable_if_t<
			std::is_convertible<T, typename segment_traits<S>::type_in>::value, int> =
			0>
	constexpr auto operator>>(S&& s) &&
	{
		return Segments<std::decay_t<S>>{std::forward<S>(s)};
	}
};

// Virtual base class for any segment/pipe with specific in/out types.
template <typename In>
class PipeEntry {
public:
	using type_in = In;

protected:
	constexpr explicit PipeEntry() = default;

public:
	PipeEntry(PipeEntry const&) = default;
	PipeEntry(PipeEntry&&) = default;
	PipeEntry& operator=(PipeEntry const&) = default;
	PipeEntry& operator=(PipeEntry&&) = default;

	virtual ~PipeEntry() = default;

	void inject(type_in x)
	{
		justInject(x);
	}

	void operator()(type_in x)
	{
		inject(x);
	}

	friend void operator>>(type_in x, PipeEntry& p)
	{
		p.inject(x);
	}

	friend void operator<<(PipeEntry& p, type_in x)
	{
		p.inject(x);
	}

private:
	virtual void justInject(type_in x) = 0;
};

template <typename Out>
class PipeExit {
public:
	using type_out = Out;

protected:
	constexpr explicit PipeExit() = default;

public:
	PipeExit(PipeExit const&) = default;
	PipeExit(PipeExit&&) = default;
	PipeExit& operator=(PipeExit const&) = default;
	PipeExit& operator=(PipeExit&&) = default;

	virtual ~PipeExit() = default;

	virtual type_out extract() = 0;

	friend auto operator>>(PipeExit& p, type_out& x)
	{
		return x = p.extract();
	}

	friend auto operator<<(type_out& x, PipeExit& p)
	{
		return x = p.extract();
	}
};

template <typename In, typename Out>
class Pipe : public PipeEntry<In>, public PipeExit<Out> {
public:
	using typename PipeEntry<In>::type_in;
	using typename PipeExit<Out>::type_out;

protected:
	constexpr explicit Pipe() = default;

public:
	Pipe(Pipe const&) = default;
	Pipe(Pipe&&) = default;
	Pipe& operator=(Pipe const&) = default;
	Pipe& operator=(Pipe&&) = default;

	virtual ~Pipe() override = default;

	virtual type_out inject(type_in x) = 0;

	type_out operator()(type_in x)
	{
		return inject(x);
	}

	friend type_out operator>>(type_in x, Pipe& p)
	{
		p.inject(x);
	}

	friend type_out operator<<(Pipe& p, type_in x)
	{
		p.inject(x);
	}

private:
	void justInject(type_in x) final
	{
		inject(x);
	}
};

// Concrete implementation of Pipe, given a segment/pipe.
template <typename S>
class SpecificPipe
	: public Pipe<typename segment_traits<S>::type_in, typename segment_traits<S>::type_out>,
	  public S {
public:
	using segments_type = S;
	using type_in = typename segment_traits<segments_type>::type_in;
	using type_out = typename segment_traits<segments_type>::type_out;
	using Pipe_type = Pipe<type_in, type_out>;

protected:
	template <
		typename S_,
		std::enable_if_t<std::is_constructible<segments_type, S_>::value, int> = 0>
	explicit SpecificPipe(S_&& s)
		: segments_type{std::forward<S_>(s)}
	{}

	template <typename... S_>
	friend class Segments;

public:
	SpecificPipe(SpecificPipe const&) = default;
	SpecificPipe(SpecificPipe&&) = default;
	SpecificPipe& operator=(SpecificPipe const&) = default;
	SpecificPipe& operator=(SpecificPipe&&) = default;

	virtual ~SpecificPipe() override = default;

	virtual type_out inject(type_in x) override
	{
		return segments_type::inject(x);
	}

	virtual type_out extract() override
	{
		return segments_type::extract();
	}

	using Pipe_type::operator();
};

template <typename S0, typename S1, typename... S>
SpecificPipe<Segments<S0, S1, S...>> Segments<S0, S1, S...>::operator>>(Exit&& e) &&
{
	UNUSED(e)
	return SpecificPipe<Segments>{std::move(*this)};
}

template <typename S0>
SpecificPipe<Segments<S0>> Segments<S0>::operator>>(Exit&& e) &&
{
	UNUSED(e)
	return SpecificPipe<Segments>{std::move(*this)};
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

template <typename In, typename Out>
class Cast {
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
