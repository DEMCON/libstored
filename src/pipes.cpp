// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/macros.h>
#include <libstored/pipes.h>

#ifdef STORED_HAVE_PIPES

#	include <utility>

namespace stored {
namespace pipes {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Group gc;

// NOLINTNEXTLINE
struct GC_ final {
	~GC_()
	{
		gc.destroy();
	}
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GC_ gc_;



void Group::add(PipeBase& p)
{
	m_pipes.insert(&p);
}

void Group::add(std::initializer_list<std::reference_wrapper<PipeBase>> il)
{
	for(auto const& p : il)
		m_pipes.insert(&p.get());
}

void Group::remove(PipeBase& p)
{
	m_pipes.erase(&p);
}

void Group::remove(std::initializer_list<std::reference_wrapper<PipeBase>> il)
{
	for(auto const& p : il)
		m_pipes.erase(&p.get());
}

void Group::clear()
{
	m_pipes.clear();
}

void Group::trigger(bool* triggered) const
{
	bool triggered_ = false;

	for(auto* p : m_pipes) {
		bool t = false;
		p->trigger(&t);
		if(t)
			triggered_ = true;
	}

	if(triggered)
		*triggered = triggered_;
}

void Group::destroy(PipeBase& p)
{
	remove(p);

	// As both real instances of PipeBase have the delete operator
	// overloaded, this will call stored::deallocate() properly.
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	delete &p;
}

void Group::destroy()
{
	for(auto* p : m_pipes)
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete p;

	m_pipes.clear();
}

Group::set_type const& Group::pipes() const noexcept
{
	return m_pipes;
}

size_t Group::size() const noexcept
{
	return m_pipes.size();
}

Group::set_type::const_iterator Group::begin() const noexcept
{
	return m_pipes.cbegin();
}

Group::set_type::const_iterator Group::end() const noexcept
{
	return m_pipes.cend();
}

} // namespace pipes
} // namespace stored
#else  // !STORED_HAVE_PIPES
char dummy_char_to_make_pipes_cpp_non_empty; // NOLINT
#endif // STORED_HAVE_PIPES
