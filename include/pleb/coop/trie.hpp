#pragma once


#include <string>
#include <string_view>
#include <mutex>
#include <cstdint>

#include "pool.hpp"
#include "locking_weak_table.hpp" // Hope to replace with lockfree table later


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
		/*
			Create a trie with the given identifier.
				This is typically used to create a root trie.
		*/
		static std::shared_ptr<trie_> create(std::string_view id, char separator = '/')    {return std::make_shared<constructor>(id, separator);}

		~trie_() {}

		/*
			Get this trie's identifier or complete path.
				The path is a list of ancestors, not including the root.
		*/
		std::string_view id  () const noexcept    {return {_path.data()+_path_id_pos, _path.length()-_path_id_pos};}
		std::string_view path() const noexcept    {return _path;}


		/*
			Access the parent node.
		*/
		const std::shared_ptr<trie_> &parent() const noexcept    {return _parent;}


		/*
			Map a child of this trie to an existing trie.
				This is comparable to a symlink, and requires the name to be unused.
		*/
		bool make_link(std::string_view id, std::shared_ptr<trie_> destination)    {return _children.try_insert(id, std::move(destination));}


		/*
			Access an immediate child by its identifier at this leve in the trie.
				try_child may fail, returning null.
				get_child will create a subtrie if it does not exist.
		*/
		[[nodiscard]] std::shared_ptr<trie_> try_child(std::string_view id) noexcept    {return _children.find(id);}
		[[nodiscard]] std::shared_ptr<trie_> get_child(std::string_view id)             {return _children.template find_or_create<constructor>(id, id, *this);}
		//[[nodiscard]] std::shared_ptr<trie_> operator[](std::string_view id) noexcept    {return get_child(id);}


		/*
			Access direct or indirect children (or this trie node) by providing a path.
				Path should be an iterable range of string_view compatible path elements.

			find -- find a descendant if it exists.
			get  -- create child nodes as needed to complete the path.
			find -- find the trie_node corresponding to the longest matching prefix of the path.
		*/
		template<typename Path> [[nodiscard]]
		std::shared_ptr<trie_> find   (Path path) noexcept    {return _search<Path,0>(std::forward<Path>(path));}

		template<typename Path> [[nodiscard]]
		std::shared_ptr<trie_> nearest(Path path) noexcept    {return _search<Path,1>(std::forward<Path>(path));}

		template<typename Path> [[nodiscard]]
		std::shared_ptr<trie_> get    (Path path)
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


		/*
			Visit child tries via callback.
				Pending implementation of a lock-free hashmap this operation blocks any
				mutation of the trie; incautious use may result in deadlock.
		*/
		template<typename Callback>
		void visit_children(const Callback &callback) const noexcept(noexcept(_children.visit(callback)))
			{_children.visit(callback);}


		// The base class must provide shared_from_this.
		std::shared_ptr<      trie_> shared_from_this()          {return std::shared_ptr<      trie_>(Coop_Base::shared_from_this(), this);}
		std::shared_ptr<const trie_> shared_from_this() const    {return std::shared_ptr<const trie_>(Coop_Base::shared_from_this(), this);}
		

	protected:
		trie_(std::string_view id, char separator)    : _path(id), _path_id_pos(0), _separator(separator) {}
		trie_(std::string_view id, std::shared_ptr<trie_> parent)
			:
			_parent(std::move(parent)),
			_path(_concat(_parent->path(), _parent->_separator, id)), _path_id_pos((unsigned short)(_path.length()-id.length())),
			_separator(_parent->_separator) {}

		class constructor : public trie_
		{
		public:
			constructor(std::string_view id, char  sep)    : trie_(id, sep) {}
			constructor(std::string_view id, trie_&par)    : trie_(id, par.shared_from_this()) {}
		};

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

		static std::string _concat(std::string_view base, char separator, std::string_view id)
		{
			std::string result;
			if (base.length())
			{
				result.reserve(base.length()+1+id.length());
				result.assign(base.data(), base.length());
				result.push_back(separator);
			}
			else result.reserve(id.length());
			result.append(id.data(), id.length());
			return result;
		}

	private:
		std::shared_ptr<trie_>                 _parent;
		const std::string                      _path;
		const unsigned short                   _path_id_pos;
		const char                             _separator;
		locking_weak_table<std::string, trie_> _children; // Hope to replace with atomic table later
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
}
