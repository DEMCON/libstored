#ifndef __LIBSTORED_STORE_H_ExampleSync
#define __LIBSTORED_STORE_H_ExampleSync

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>
#include <libstored/types.h>
#include <libstored/directory.h>

namespace stored {

	/*!
	 * \brief Data storage of ExampleSyncBase.
	 */
#ifdef STORED_COMPILER_MSVC
	__declspec(align(8))
#endif
	struct ExampleSyncData {
		ExampleSyncData();

		/*! \brief Data buffer for all variables. */
		char buffer[12];

		static uint8_t const* shortDirectory();
		static uint8_t const* longDirectory();
	}
#ifndef STORED_COMPILER_MSVC
	__attribute__((aligned(sizeof(double))))
#endif
	;

	/*!
	 * \brief Base class with default interface of all ExampleSync implementations.
	 * 
	 * Although there are no virtual functions in the base class, subclasses
	 * can override them.  The (lowest) subclass must pass the \p
	 * Implementation_ template paramater to its base, such that all calls from
	 * the base class can be directed to the proper overridden implementation.
	 *
	 * The base class cannot be instantiated. If a default implementation is
	 * required, which does not have side effects to functions, instantiate
	 * #stored::ExampleSync.  This class contains all data of all variables,
	 * so it can be large.  So, be aware when instantiating it on the stack.
	 * Heap is fine. Static allocations is fine too, as the constructor and
	 * destructor are trivial.
	 * 
	 * \see #stored::ExampleSync
	 * \see #stored::ExampleSyncData
	 * \ingroup libstored_stores
	 */
	template <typename Implementation_>
	class ExampleSyncBase {
		CLASS_NOCOPY(ExampleSyncBase)
	protected:
		/*! \brief Default constructor. */
		ExampleSyncBase() is_default;

	public:
		/*! \brief Type of the actual implementation, which is the (lowest) subclass. */
		typedef Implementation_ Implementation;
		/*! \brief Returns the name of store, which can be used as prefix for #stored::Debugger. */
		char const* name() const { return "/ExampleSync"; }

		/*! \brief Returns the reference to the implementation.*/
		Implementation const& implementation() const { return *static_cast<Implementation const*>(this); }
		/*! \copydoc stored::ExampleSyncBase::implementation() const */
		Implementation& implementation() { return *static_cast<Implementation*>(this); }

	private:
		/*! \brief The store's data. */
		ExampleSyncData m_data;
		/*! \brief Returns the buffer with all variable data. */
		char const* buffer() const { return m_data.buffer; }
		/*! \copydoc buffer() const */
		char* buffer() { return m_data.buffer; }

		// Accessor generators.

		/*!
		 * \brief Returns a typed Variable object, given the offset in the buffer.
		 *
		 * This only works for fixed-length types.
		 * For other types, use #variantv(Type::type, size_t, size_t).
		 */
		template <typename T> Variable<T,Implementation> variable(size_t offset) {
			stored_assert(offset + sizeof(T) <= sizeof(m_data.buffer));
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			return Variable<T,Implementation>(implementation(), *reinterpret_cast<T*>(&buffer()[offset]));
		}

		/*!
		 * \brief Returns a typed Function object, given the function identifier.
		 *
		 * This only works for fixed-length types.
		 * For other types, use #variantf(Type::type, unsigned int).
		 */
		template <typename T> Function<T,Implementation> function(unsigned int f) {
			return Function<T,Implementation>(implementation(), f);
		}
        
		/*!
		 * \brief Returns the Variant for a variable.
		 */
		Variant<Implementation> variantv(Type::type type, size_t offset, size_t len) {
			stored_assert(offset + len < sizeof(m_data.buffer));
			stored_assert(!Type::isFunction(type));
			return Variant<Implementation>(implementation(), type, &buffer()[offset], len);
		}
		
		/*!
		 * \brief Returns the Variant for a function.
		 */
		Variant<Implementation> variantf(Type::type type, unsigned int f, size_t len) {
			stored_assert(Type::isFunction(type));
			return Variant<Implementation>(implementation(), type, f, len);
		}
		
		// Variable/Function/Variant must be able to call callback() and the hooks.
		// However, these functions should normally not be used from outside the class.
		// Therefore, they are protected, and make these classes a friend.
		template <typename T, typename I, bool H> friend class Variable;
		template <typename T, typename I> friend class Function;
		friend class Variant<Implementation>;

	protected:
		/*!
		 * \brief Function callback resolver.
		 *
		 * This is the callback for a Function and a Variant, which converts a
		 * call to the function identifier to an actual function call within
		 * the Implementation.
		 */
		size_t callback(bool UNUSED_PAR(set), void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len), unsigned int UNUSED_PAR(f)) {
			switch(f) {
			case 1: // function
				stored_assert(len == sizeof(int32_t));
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				implementation().__function(set, *reinterpret_cast<int32_t*>(buffer));
				return sizeof(int32_t);
			default:
				return 0;
			}
		}

		// Default function callback. Override in subclass.

		/*! \brief Callback for function */
		void __function(bool set, int32_t& value);

	public:
		/*!
		 * \brief Type of a key.
		 * \see #bufferToKey()
		 */
		typedef uintptr_t Key;

		/*!
		 * \brief Converts a variable's buffer to a key.
		 *
		 * A key is unique for all variables of the same store, but identical
		 * for the same variables across different instances of the same store
		 * class. Therefore, the key can be used to synchronize between
		 * instances of the same store.  A key does not contain meta data, such
		 * as type or length. It is up to the synchronization library to make
		 * sure that these properties are handled well.
		 *
		 * For synchronization, when #hookEntryX() or #hookEntryRO() is
		 * invoked, one can compute the key of the object that is accessed. The
		 * key can be used, for example, in a key-to-Variant map. When data
		 * arrives from another party, the key can be used to find the proper
		 * Variant in the map.
		 *
		 * This way, data exchange is type-safe, as the Variant can check if
		 * the data format matches the expected type. However, one cannot
		 * process data if the key is not set yet in the map.
		 */
		Key bufferToKey(void const* buffer) const {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
			stored_assert((uintptr_t)buffer >= (uintptr_t)this->buffer() && (uintptr_t)buffer < (uintptr_t)this->buffer() + sizeof(m_data.buffer));
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
			return (uintptr_t)buffer - (uintptr_t)this->buffer();
		}

	protected:
		/*!
		 * \brief Hook when exclusive access to a given variable is to be acquired.
		 * \details Must be followed by #hookExitX().
		 */
		void hookEntryX(Type::type type, void* buffer, size_t len) {
			// This function is called by Variants, and alike, but the implementation
			// has to friend all of them if it overrides this function.
			// To ease integration give a default implementation that forwards the hook.
			implementation().__hookEntryX(type, buffer, len); }

		/*!
		 * \brief Hook when exclusive access to a given variable is released.
		 * \details Must be preceded by #hookEntryX().
		 */
		void hookExitX(Type::type type, void* buffer, size_t len, bool changed) {
			implementation().__hookExitX(type, buffer, len, changed); }

		/*!
		 * \brief Hook when read-only access to a given variable is to be acquired.
		 * \details Must be followed by #hookExitRO().
		 */
		void hookEntryRO(Type::type type, void* buffer, size_t len) {
			implementation().__hookEntryRO(type, buffer, len); }

		/*!
		 * \brief Hook when read-only access to a given variable is released.
		 * \details Must be preceded by #hookEntryRO().
		 */
		void hookExitRO(Type::type type, void* buffer, size_t len) {
			implementation().__hookExitRO(type, buffer, len); }

		/*!
		 * \copydoc hookEntryX()
		 * \details Default implementation does nothing. Override subclass.
		 */
		void __hookEntryX(Type::type UNUSED_PAR(type), void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len)) {}

		/*!
		 * \copydoc hookExitX()
		 * \details Default implementation does nothing. Override subclass.
		 */
		void __hookExitX(Type::type UNUSED_PAR(type), void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len), bool UNUSED_PAR(changed)) {}

		/*!
		 * \copydoc hookEntryRO()
		 * \details Default implementation does nothing. Override subclass.
		 */
		void __hookEntryRO(Type::type UNUSED_PAR(type), void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len)) {}

		/*!
		 * \copydoc hookExitRO()
		 * \details Default implementation does nothing. Override subclass.
		 */
		void __hookExitRO(Type::type UNUSED_PAR(type), void* UNUSED_PAR(buffer), size_t UNUSED_PAR(len)) {}

	public:
		// Type-specific object accessors.

		/*! \brief variable 1 */
		Variable<int32_t,Implementation> variable_1() {
			stored_assert(4u == sizeof(int32_t)); // NOLINT(hicpp-static-assert,misc-static-assert)
			return variable<int32_t>(8u); }
		/*! \brief variable 2 */
		Variable<int32_t,Implementation> variable_2() {
			stored_assert(4u == sizeof(int32_t)); // NOLINT(hicpp-static-assert,misc-static-assert)
			return variable<int32_t>(0u); }
		/*! \brief function */
		Function<int32_t,Implementation> function() {
			return function<int32_t>(1u); }

	public:
		/*!
		 * \copydoc stored::ExampleSyncData::shortDirectory()
		 */
		uint8_t const* shortDirectory() const { return ExampleSyncData::shortDirectory(); }
		/*!
		 * \copydoc stored::ExampleSyncData::longDirectory()
		 */
		uint8_t const* longDirectory() const { return ExampleSyncData::longDirectory(); }

		/*!
		 * \brief Finds an object with the given name.
		 * \return the object, or an invalid #stored::Variant if not found.
		 */
		Variant<Implementation> find(char const* name, size_t len = std::numeric_limits<size_t>::max()) {
			return stored::find(implementation(), buffer(), shortDirectory(), name, len);
		}

		/*!
		 * \brief Calls a callback for every object in the #longDirectory().
		 * \see stored::list()
		 */
		template <typename F>
		void list(F f, void* arg, char const* prefix = nullptr) { stored::list(&implementation(), buffer(), longDirectory(), f, arg, prefix); }

#if __cplusplus >= 201103L
		/*!
		 * \brief Calls a callback for every object in the #longDirectory().
		 * \see stored::list()
		 */
		template <typename F>
		void list(F& f) { stored::list<Implementation,F>(&implementation(), buffer(), longDirectory(), f); }
#endif
	};
	
	/*!
	 * \brief Default ExampleSyncBase implementation.
	 * \ingroup libstored_stores
	 */
	class ExampleSync : public ExampleSyncBase<ExampleSync> {
		CLASS_NOCOPY(ExampleSync)
	public:
		typedef ExampleSyncBase<ExampleSync> base;
		friend class ExampleSyncBase<ExampleSync>;
		/*! \copydoc stored::ExampleSyncBase::ExampleSyncBase() */
		ExampleSync() is_default;
	protected:
		void __function(bool set, int32_t& value) { if(!set) value = (int32_t)0; }
	};

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_STORE_H_ExampleSync