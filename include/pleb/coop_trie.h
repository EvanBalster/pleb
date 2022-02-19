#pragma once


#include <string>
#include <string_view>
#include <mutex>
#include <cstdint>

#include "coop_pool.h"
#include "locking_weak_table.h" // Hope to replace with lockfree table later


/*
	Cooperative atomic trie structures.
		Tries are owned by their immedate members and by subtries.
		Like other cooperatives, outside users are also free to share ownership.

	These containers are controlled by std::shared_ptr and are
		designed to outlive all contained values by giving those
		values shared ownership over the container.

	The primary use case is messaging patterns:  Objects register
		themselves in a collection, under a name, for as long as
		they exist, disappearing from the container upon destruction.
*/


namespace coop
{
	/*
		Base class for cooperative tries.
			Sub-tries share ownership in their parents using std::shared_ptr.
			Trie values are conferred by the base class, which is also responsible
			for ensuring values hold ownership over the trie.
	*/
	template<typename Coop_Base>
	class trie_ : public Coop_Base
	{
	public:
		using coop_type = Coop_Base;

		using hash_type = std::hash<std::string>;

		
	public:
		static std::shared_ptr<trie_> create()    {return std::make_shared<constructor>();}

		~trie_() {}


		/*
			Access the parent node.
		*/
		const std::shared_ptr<trie_> &parent() noexcept    {return _parent;}

		/*
			Access an immediate child by its identifier at this leve in the trie.
				try_child may fail, returning null.
				get_child will create a subtrie if it does not exist.
		*/
		[[nodiscard]] std::shared_ptr<trie_> try_child(std::string_view id) noexcept    {return _children.get(id);}
		[[nodiscard]] std::shared_ptr<trie_> get_child(std::string_view id) noexcept    {return _children.template acquire<constructor>(id, *this);}
		//[[nodiscard]] std::shared_ptr<trie_> operator[](std::string_view id) noexcept    {return get_child(id);}


		/*
			Access direct or indirect children (or this trie node) by providing a path.
				Path should be an iterable range of string_view compatible path elements.

			acquire     -- create child nodes as needed to complete the path.
			find_prefix -- find the trie_node corresponding to the longest matching prefix of the path.
		*/
		template<typename Path> [[nodiscard]]
		std::shared_ptr<trie_> find   (Path path) noexcept    {return _search<Path,0>(std::forward<Path>(path));}

		template<typename Path> [[nodiscard]]
		std::shared_ptr<trie_> nearest(Path path) noexcept    {return _search<Path,1>(std::forward<Path>(path));}

		template<typename Path> [[nodiscard]]
		std::shared_ptr<trie_> get    (Path path) noexcept
		{
			std::shared_ptr<trie_> node = shared_from_this();
			auto i = path.begin(), e = path.end();
			for (std::string_view id : path)
			{
				if (!id.length()) continue;
				node = node->get_child(id);
			}
			return node;
		}


		// The base class must provide shared_from_this.
		std::shared_ptr<      trie_> shared_from_this()          {return std::shared_ptr<      trie_>(Coop_Base::shared_from_this(), this);}
		std::shared_ptr<const trie_> shared_from_this() const    {return std::shared_ptr<const trie_>(Coop_Base::shared_from_this(), this);}
		

	protected:
		trie_()                                  {}
		trie_(std::shared_ptr<trie_> _parent)    : _parent(std::move(_parent)) {}
		trie_(trie_ &_parent)                    : _parent(_parent.shared_from_this()) {}

		class constructor : public trie_ {public: constructor() {} constructor(trie_&t) : trie_(t) {}};

		template<typename Path, bool Nearest = false> [[nodiscard]]
			std::shared_ptr<trie_> _search(Path path) noexcept
		{
			std::shared_ptr<trie_> node = shared_from_this();
			auto i = path.begin(), e = path.end();
			for (std::string_view id : path)
			{
				if (!id.length()) continue;
				auto next = node->try_child(id);
				if (Nearest)    {if (!next) break; node.swap(next);}
				else            {node.swap(next); if (!node) break;}
			}
			return node;
		}

	private:
		std::shared_ptr<trie_>    _parent;
		locking_weak_table<trie_> _children; // Hope to replace with atomic table later
	};


	// A small type to inject support for enable_shared_from_this into a parent type.
	template<typename T> class add_shared_from_this : public T, public std::enable_shared_from_this<T> {public: using T::T;};

	/*
		Definitions of unmanaged [see pool.h] and managed tries and multitries.

		Tries combine the interfaces of basic_trie and slot.
		Multitries combine basic_trie and pool.
	*/
	namespace unmanaged
	{
		template<typename T> using trie      = trie_<add_shared_from_this<unmanaged::slot<T>>>;
		template<typename T> using multitrie = trie_<add_shared_from_this<unmanaged::pool<T>>>;
	}

	template<typename T> using trie      = trie_<coop::slot<T>>;
	template<typename T> using multitrie = trie_<coop::pool<T>>;

#if 0
	/*
		Atomic trie where each node may have any number of values.
			Storage of values is based on pool.
	*/
	template<typename Value>
	class multitrie :
		public pool<Value>,
		public trie_base<multitrie<Value>>
	{
	public:
		using value_type = Value;

		using pool_type = pool<Value>;

		struct element :
			public pool_type::element
		{
			// Access the trie held by this value.
			std::shared_ptr<node_type> node() const    {return std::static_pointer_cast<node_type, pool_type>(this->pool);}

		private:
			element() = delete; // This type can't be instantiated, only reinterpreted from pool_type::element
		};


	public:
		/*
			trie_node must always be created under control of a shared_ptr.
		*/
		static std::shared_ptr<multitrie> create()    {return std::make_shared<constructor>();}

		~multitrie() {}


		/*
			Add a value to the trie node.
				The trie node will not be destroyed until all values have expired.
				add_element provides access to the trie node.

			Values are removed when these shared pointers expire.
		*/
		[[nodiscard]] std::shared_ptr<value_type> add        (value_type &&value)    {return pool_type::alloc(std::move(value));}
		[[nodiscard]] std::shared_ptr<element>    add_element(value_type &&value)    {return std::static_pointer_cast<element>(pool_type::alloc_element(std::move(value)));}


		/*
			Lock-free iteration over values at this node,
				or ascending to the root of the trie.
		*/
		template<class F> void for_each       (F func) const    {for (auto &value : (pool_type&) *this) func(value);}
		template<class F> void for_each_ascend(F func) const    {for_each(func); if (parent) parent->for_each_ascend(func);}


	protected:
		using _base_t = trie_base<multitrie<Value>>;
		friend class _base_t;

		multitrie()                                            {}
		multitrie(std::shared_ptr<const multitrie> _parent)    : _base_t(std::move(_parent)) {}

		class constructor : public multitrie
		{
		public:
			constructor()                             {}
			constructor(std::shared_ptr<const node_type> _parent)    : multitrie(std::move(_parent)) {}
		};
	};


	/*
		Atomic trie where each node has one value or no value.
	*/
	template<typename Value>
	class trie :
		public std::enable_shared_from_this<trie>,
		public trie_base<trie<Value>>
	{
	public:
		using value_type = Value;

		struct element
		{
			value_type                              value;
			const std::shared_ptr<trie> node;

			element(value_type &&_value, std::shared_ptr<trie> _node)    : value(std::move(_value)), node(std::move(_node)) {}
		};


	public:
		/*
			trie_node must always be created under control of a shared_ptr.
		*/
		static std::shared_ptr<trie> create()    {return std::make_shared<constructor>();}

		~trie() {}

		/*
			Set the value for the trie node.
				Fails, returning a null pointer, if some value is already set.
				The trie node will not be destroyed until all values have expired.
				add_element provides access to the trie node.

			Values are removed when these shared pointers expire.
		*/
		[[nodiscard]] std::shared_ptr<value_type> set        (value_type &&value)    {auto ptr = set_element(std::move(value)); return ptr ? std::shared_ptr<value_type>(std::move(ptr), &ptr->value) : nullptr;}
		[[nodiscard]] std::shared_ptr<element>    set_element(value_type &&value)    {return _element.try_emplace(std::move(value));}

		std::shared_ptr<value_type> get()         const noexcept    {auto ptr = get_element(); return ptr ? std::shared_ptr<value_type>(std::move(ptr), &ptr->value) : nullptr;}
		std::shared_ptr<element>    get_element() const noexcept    {return _element.lock();}


	protected:
		using _base_t = trie_base<trie<Value>>;
		friend class _base_t;

		trie()                                                                     {_creating.clear();}
		trie(std::shared_ptr<const trie> _parent)    : _base_t(std::move(_parent)) {_creating.clear();}

		unmanaged::slot<element> _element;
	};
#endif
}
