#ifndef __LIBSTORED_DEBUGGER_H
#define __LIBSTORED_DEBUGGER_H

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/types.h>
#include <libstored/util.h>
#include <libstored/spm.h>

#include <new>
#include <utility>
#include <map>
#include <vector>

namespace stored {

	// Container-template-type-invariant base class.
	class DebugVariantBase {
	public:
		virtual ~DebugVariantBase()
#if __cplusplus >= 201103L
			= default;
#else
			{}
#endif

		virtual size_t get(void* dst, size_t len = 0) const = 0;
		virtual size_t set(void const* src, size_t len = 0) = 0;
		virtual Type::type type() const = 0;
		virtual size_t size() const = 0;
		virtual bool valid() const = 0;
	};

	// Container-specific subclass of DebugVariantBase.
	template <typename Container = void>
	class DebugVariantTyped : public DebugVariantBase {
	public:
		DebugVariantTyped(Variant<Container> const& variant)
			: m_variant(variant)
		{}

		DebugVariantTyped() {}

		virtual ~DebugVariantTyped() override
#if __cplusplus >= 201103L
			= default;
#else
			{}
#endif
		
		size_t get(void* dst, size_t len = 0) const override final {
			return variant().get(dst, len); }
		size_t set(void const* src, size_t len = 0) override final {
			return variant().set(src, len); }
		Type::type type() const override final {
			return variant().type(); }
		size_t size() const override final {
			return variant().size(); }
		bool valid() const override final {
			return variant().valid(); }

		Variant<Container> const& variant() const { return m_variant; }
		Variant<Container>& variant() { return m_variant; }

	private:
		Variant<Container> m_variant;
	};

	// Template-type-independent container for a DebugVariantTyped.
	class DebugVariant : public DebugVariantBase {
	public:
		DebugVariant() {
			new(m_buffer) DebugVariantTyped<>();
		}

		template <typename Container>
		DebugVariant(Variant<Container> const& variant) {
			// Check if the cast of variant() is valid.
			static_assert(sizeof(DebugVariantTyped<Container>) == sizeof(DebugVariantTyped<>), "");

			// Check if our default copy constructor works properly.
#if __cplusplus >= 201103L
			static_assert(std::is_trivially_copyable<Variant<Container>>::value, "");
#elif defined(__GCC__)
			static_assert(__has_trivial_copy(Variant<Container>), "");
#endif

			new(m_buffer) DebugVariantTyped<Container>(variant);
			// Check if the cast of variant() works properly.
			stored_assert(static_cast<DebugVariantBase*>(reinterpret_cast<DebugVariantTyped<Container>*>(m_buffer)) == &this->variant());
		}

		~DebugVariant() override {
			variant().~DebugVariantBase();
		}

		size_t get(void* dst, size_t len = 0) const override final {
			return variant().get(dst, len); }
		size_t set(void const* src, size_t len = 0) override final {
			return variant().set(src, len); }
		Type::type type() const override final {
			return variant().type(); }
		size_t size() const override final {
			return variant().size(); }
		bool valid() const override final {
			return variant().valid(); }

		DebugVariantBase& variant() {
			return *static_cast<DebugVariantBase*>(reinterpret_cast<DebugVariantTyped<>*>(m_buffer));
		}

		DebugVariantBase const& variant() const {
			return *static_cast<DebugVariantBase const*>(reinterpret_cast<DebugVariantTyped<> const*>(m_buffer));
		}

	private:
		char m_buffer[sizeof(DebugVariantTyped<>)];
	};

	class DebugStoreBase {
	public:
		virtual ~DebugStoreBase()
#if __cplusplus >= 201103L
			= default;
#else
			{}
#endif

		virtual char const* name() const = 0;

		virtual DebugVariant find(char const* name, size_t len = std::numeric_limits<size_t>::max()) = 0;

		typedef void(ListCallback)(char const*, DebugVariant&);
		virtual void list(ListCallback* f) const = 0;

		typedef void(ListCallbackArg)(char const*, DebugVariant&, void*);
		virtual void list(ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr) const = 0;

#if __cplusplus >= 201103L
		template <typename F>
		SFINAE_IS_FUNCTION(F, ListCallback, void)
		list(F& f) const {
			auto cb = [](char const* name, DebugVariant& variant, void* f) {
				(*(F*)f)(name, variant);
			};
			list(static_cast<ListCallbackArg*>(cb), &f);
		}
#endif
	};

	template <typename Store>
	class DebugStore : public DebugStoreBase {
	public:
		DebugStore(Store& store)
			: m_store(store)
		{}

		virtual ~DebugStore() override 
#if __cplusplus >= 201103L
			= default;
#else
			{}
#endif
		
		char const* name() const override { return store().name(); }

		DebugVariant find(char const* name, size_t len = std::numeric_limits<size_t>::max()) override {
			return store().find(name, len);
		}
	
		virtual void list(DebugStoreBase::ListCallbackArg* f, void* arg = nullptr, char const* prefix = nullptr) const override {
			void* cb[] = {(void*)f, arg};
			store().list(&listCallback, cb, prefix);
		}

		virtual void list(DebugStoreBase::ListCallback* f) const override {
			void* cb[] = {(void*)f, nullptr};
			store().list(&listCallback, cb);
		}

		typename Store::Implementation& store() const { return m_store.implementation(); }

	private:
		static void listCallback(void* container, char const* name, Type::type type, void* buffer, size_t len, void* cb) {
			DebugVariant variant(Variant<typename Store::Implementation>(*(typename Store::Implementation*)container, type, buffer, len));
			(*(ListCallbackArg*)((void**)cb)[0])(name, variant, ((void**)cb)[1]);
		}

	private:
		Store& m_store;
	};

	class ProtocolLayer {
	public:
		explicit ProtocolLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
			: m_up(up), m_down(down)
		{}

		virtual ~ProtocolLayer();

		void setUp(ProtocolLayer* up) { m_up = up; }
		void setDown(ProtocolLayer* down) { m_down = down; }

		ProtocolLayer* up() const { return m_up; }
		ProtocolLayer* down() const { return m_down; }

		virtual void decode(void* buffer, size_t len) {
			if(up())
				up()->decode(buffer, len);
		}

		void encode() {
			encode((void const*)nullptr, 0, true);
		}

		virtual void encode(void* buffer, size_t len, bool last = true) {
			if(down())
				down()->encode(buffer, len, last);
		}

		virtual void encode(void const* buffer, size_t len, bool last = true) {
			if(down())
				down()->encode(buffer, len, last);
		}
	private:
		ProtocolLayer* m_up;
		ProtocolLayer* m_down;
	};

	class Debugger : public ProtocolLayer {
	public:
		struct StorePrefixComparator {
			bool operator()(char const* lhs, char const* rhs) const {
				stored_assert(lhs && rhs);
				return strcmp(lhs, rhs) < 0;
			}
		};

		typedef std::map<char const*, DebugStoreBase*, StorePrefixComparator> StoreMap;

		Debugger();
		virtual ~Debugger();

		template <typename Store>
		void map(Store& store, char const* name = nullptr) {
			map(new DebugStore<Store>(store), name);
		}

		void unmap(char const* name);
		StoreMap const& stores() const;

		DebugVariant find(char const* name, size_t len = std::numeric_limits<size_t>::max()) const;

		typedef DebugStoreBase::ListCallbackArg ListCallbackArg;
		typedef DebugStoreBase::ListCallback ListCallback;

		void list(ListCallbackArg* f, void* arg = nullptr) const;
		void list(ListCallback* f) const;
#if __cplusplus >= 201103L
		template <typename F>
		void list(F& f) const {
			auto cb = [](char const* name, DebugVariant& variant, void* arg) {
				(*(F*)arg)(name, variant);
			};
			list((ListCallbackArg*)cb, &f);
		}
#endif

		static char const CmdCapabilities = '?';
		static char const CmdRead = 'r';
		static char const CmdWrite = 'w';
		static char const CmdEcho = 'e';
		static char const CmdList = 'l';
		static char const CmdAlias = 'a';
		static char const CmdMacro = 'm';
		static char const Ack = '!';
		static char const Nack = '?';

	public:
		virtual void capabilities(char*& list, size_t& len, size_t reserve = 0);
		virtual void process(void const* frame, size_t len, ProtocolLayer& response);

		virtual void decode(void* buffer, size_t len) override;

	protected:
		typedef std::map<char, DebugVariant> AliasMap;
		AliasMap& aliases();
		AliasMap const& aliases() const;
		
		typedef std::map<char, std::string> MacroMap;
		MacroMap& macros();
		MacroMap const& macros() const;
		virtual bool runMacro(char m, ProtocolLayer& response);

	private:
		static void listCmdCallback(char const* name, DebugVariant& variant, void* arg);

	protected:
		void map(DebugStoreBase* store, char const* name);

		template <typename T, typename B>
		void encodeHex(T value, B*& buf, size_t& len, bool shortest = true) {
			void* data = (void*)&value;
			len = sizeof(T);
			encodeHex(toType<T>::type, data, len, shortest);
			buf = (B*)data;
		}
		void encodeHex(Type::type type, void*& data, size_t& len, bool shortest = true);
		bool decodeHex(Type::type type, void*& data, size_t& len);
		ScratchPad& spm();

	private:
		StoreMap m_map;
		ScratchPad m_scratchpad;

		AliasMap m_aliases;

		MacroMap m_macros;
		size_t m_macroSize;
	};

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_DEBUGGER_H
