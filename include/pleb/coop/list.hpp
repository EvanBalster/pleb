#pragma once

#include <cstddef>
#include "pool.hpp"


/*
	This header defines a cooperative wait-free forward list.
		This may in turn be used to implement stacks, hashmaps
		and other wait-free data structures.

	The list may contain data nodes and "bookmark nodes".
		Normal iterators skip over bookmarks and expired data.
		Bookmarks are typically used as starting points for iterators.
		At the moment, bookmarks CANNOT BE DELETED and must outlive the list.
*/


namespace coop
{
	namespace unmanaged
	{
		template<typename T>
		class forward_list
		{
		public:
			using value_type = T;
			using slot = unmanaged::slot<T>;

			class node;
			class value_node;
			class bookmark_node;

			class iterator;

			static const uintptr_t
				node_data_flag    = 1,
				sentinel_out_of_list = 0,
				sentinel_end_of_list = 1,
				node_ptr_mask = ~(node_data_flag);

			// A tagged pointer indicating the next node and its type (data or bookmark)
			//    Also contains a flag for whether the HOLDER of this pointer was released.
			struct node_ptr
			{
				uintptr_t raw;

				bool is_null()    const noexcept    {return !(raw&node_ptr_mask);}
				bool is_node()    const noexcept    {return  (raw&node_ptr_mask);}
				bool is_data()    const noexcept    {return raw&node_data_flag;}
				bool is_in_list() const noexcept    {return raw != sentinel_out_of_list;}

				explicit operator bool() const noexcept    {return bool(raw&node_ptr_mask);}

				node *get()      const    {return reinterpret_cast<node*>(raw&node_ptr_mask);}
				operator node*() const    {return get();}
			};

			struct atomic_node_ptr
			{
				std::atomic<uintptr_t> raw;

				atomic_node_ptr() : raw(sentinel_out_of_list) {}

				void set_end_of_list() noexcept    {raw.store(sentinel_end_of_list, std::memory_order_relaxed);}
				void set_out_of_list() noexcept    {raw.store(sentinel_out_of_list, std::memory_order_relaxed);}

				node_ptr load(               std::memory_order order = std::memory_order_seq_cst) const    {return {raw.load(order)};}
				void store(const node_ptr v, std::memory_order order = std::memory_order_seq_cst)          {raw.store(v.raw,order);}

				bool compare_exchange_weak(node_ptr &expected, node_ptr desired) noexcept    {return raw.compare_exchange_weak(expected.raw, desired.raw);}
			};

			class node
			{
			public:
				node() {}
				
				// Check if this node is in a list.

				// No copying!!
				node(const node&) = delete;
				void operator=(const node&) = delete;

			protected:
				friend class forward_list;
				friend class iterator;
				atomic_node_ptr _next;

				// Insert a node after this one, resulting in sequence:
				//   this > new_next > former referent of _next
				//   Behavior is undefined if the added node is already in a list.
				void _insertNext(node_ptr new_next)
				{
					node *new_next_p = new_next.get();
					node_ptr after = _next.load();
					do new_next_p->_next.store(after);
					while (!_next.compare_exchange_weak(after, new_next));
				}
			};
			
			/*
				A slim node used as the beginning of a list.
			*/
			class start_node : public node
			{
			public:
				start_node()    {_next.set_end_of_list();}
			};

			/*
				Bookmarks are dummy nodes skipped by regular iterators.
					They are typically inserted in order to act as
					shortcuts to segments of the list (eg, in hashmaps).
			*/
			class bookmark_node : public node
			{
			public:
				node_ptr node_ptr() const noexcept    {return {uintptr_t(this)};}

				void mark_for_removal()    {_readers.close();}

			protected:
				friend class node_iterator;
				mutable visitor_guard _readers;
			};

			/*
				Data nodes contain... data
			*/
			class value_node : public node
			{
			public:
				node_ptr node_ptr() const noexcept    {return {uintptr_t(this)&node_data_flag};}

				slot slot;
			};

			/*
				An iterator that traverses both data and bookmark nodes.
			*/
			class node_iterator
			{
			protected:
				friend class forward_list;
				forward_list      *_list;
				node              *_pos;
				std::shared_ptr<T> _item;

			public:
				node_iterator()                            noexcept    : _list(0), _pos(0) {}
				node_iterator(forward_list &l)             noexcept    : _list(&l), _pos(0) {_follow(l._head);}
				node_iterator(forward_list &l, node &from) noexcept    : _list(&l), _pos(0) {_follow(from);}

				bool is_data() const noexcept    {return bool(_item);}
				bool not_end() const noexcept    {return  _pos;}
				bool is_end () const noexcept    {return !_pos;}

				node_iterator &operator++() noexcept    {_follow(*_pos); return *this;}

				bool operator==(const iterator &o) const noexcept    {return _pos == o._pos;}
				bool operator!=(const iterator &o) const noexcept    {return _pos != o._pos;}

				// Access the value (data nodes only)
				T *operator->() const noexcept    {return  _item.get();}
				T &operator* () const noexcept    {return *_item.get();}

				operator std::shared_ptr<T>() const noexcept    {return value();}

				const std::shared_ptr<T> &value() const noexcept    {return _item;}

				// Get a shared pointer to the value and set this iterator to "end of list".  Does not alter the list.
				std::shared_ptr<T> release() noexcept    {_pos = nullptr; return std::move(_item);}

				// Insert a node before this iterator's position.
				//void insert(node_slot *new_slot) const    {_node->insert(new_slot);}

			
			protected:
				void _moveTo(node *pos, std::shared_ptr<T> item = {}) noexcept
				{
					if (_pos && !_item)
						static_cast<bookmark_node*>(_pos)->_readers.leave();
					_pos = pos;
					_item = std::move(item);
				}
			
				// Move to the first un-expired element in the list after "from".
				//  This function assumes the node holding _prev is retained and will not expire.
				void _follow(node &from) noexcept
				{
					node_ptr next;

				reload:
					next = from._next.load(std::memory_order_acquire);
					
					while (true)
					{
						node *node = next.get();
						if (!node) return _moveTo(nullptr);
						
						if (next.is_data())
						{
							// Attempt to acquire the item at this new node.
							auto v_node = static_cast<value_node*>(node);
							if (auto value_ref = v_node->slot.lock())
							{
								// Double check that the node was not recycled before being locked
								if (from._next.load(std::memory_order_acquire) != _pos) goto reload;
								return _moveTo(v_node, std::move(value_ref));
							}
						}
						else
						{
							// Attempt to acquire the bookmark, or excise it if expired.
							//   DEFECT: this operation may spinlock if another thread has locked the node.
							//   REMEDY: attempt to skip over locked node.
							auto m_node = static_cast<bookmark_node*>(node);
							if      (m_node->_readers.join())      _moveTo(m_node);
							else if (!m_node->_readers.try_lock()) goto reload;
						}

						// The next node seems to have expired.  Remove it.
					excise_node:
						node_ptr after = node->_next.load();
						if (from._next.compare_exchange_weak(next, after))
						{
							node->_next.set_out_of_list();
							if (next.is_data()) _list->_free(static_cast<value_node*>(node));
							else                static_cast<bookmark_node*>(node)->_readers.unlock();
						}
					}
				}
			};

			/*
				An iterator that skips bookmark nodes.
			*/
			class iterator : public node_iterator
			{
			public:
				iterator()                            noexcept    : node_iterator()       {}
				iterator(forward_list &l)             noexcept    : node_iterator(l)      {_skip_non_data();}
				iterator(forward_list &l, node &from) noexcept    : node_iterator(l,from) {_skip_non_data();}

				iterator &operator++() noexcept    {node_iterator::operator++(); _skip_non_data(); return *this;}

			protected:
				void _skip_non_data() noexcept    {while (_pos && !_item) ++*this;}
			};

		public:
			forward_list()    : _size(0) {}

			~forward_list()    {TODO "delete node storage"}

			/*
				Iterate through values in the list.
					after(n) yields an iterator following the given node.
			*/
			iterator begin()           noexcept    {return iterator(*this);}
			iterator end  ()           noexcept    {return iterator();}
			iterator after(node &node) noexcept    {return iterator(*this, node);}

			/*
				Iterate through nodes in the list.
					This type of iterator stops at values and bookmarks.
			*/
			node_iterator node_begin()           noexcept    {return node_iterator(*this);}
			node_iterator node_end  ()           noexcept    {return node_iterator();}
			node_iterator node_after(node &node) noexcept    {return node_iterator(*this, node);}

			// Access the item at the front of the list.
			std::shared_ptr<value_type> front()    {auto i = begin(); return i.release();}


			// Insert an element after an iterator's position.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_after(const node_iterator &pos, Args&& ... args)    {if (!pos._pos) return nullptr; return _emplace(pos._pos, std::forward<Args>(args)...);}

			// Insert an element at the head of the list.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_front(Args&& ... args)    {return _emplace(_head, std::forward<Args>(args)...);}


			// Emplace after the given node.  Be careful with this function; don't mix nodes from different lists.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_after(node &node, Args&& ... args)    {return _emplace(node, std::forward<Args>(args)...);}


			/*
				Insert nodes manually.
					This is the only way to insert bookmark nodes.
			*/
			void insert_after(const node_iterator &pos, bookmark_node &node)    {_throw_if_in_list(node); pos._pos->_insertNext(node.node_ptr());}
			void insert_after(const node_iterator &pos, value_node    &node)    {_throw_if_in_list(node); pos._pos->_insertNext(node.node_ptr());}


		protected:
			start_node          _head, _recycled;
			std::atomic<size_t> _size;

			friend class iterator;
			
			void _throw_if_in_list(const node &node)
			{
				if (node._next.load(std::memory_order_relaxed).is_in_list())
					throw std::logic_error("Node is currently part of a list.");
			}

			template<typename... Args> [[nodiscard]]
			std::shared_ptr<value_type> _emplace(node &previous, Args&& ... args)
			{
				value_node *node = _alloc();
				auto ref = node->slot.try_emplace(std::forward<Args>(args) ...);
				if (ref) previous._insertNext(node->node_ptr());
				else     _free(node); // shouldn't fail but handle it anyway
				return ref;
			}

			void  _free(value_node *node) noexcept    {_throw_if_in_list(*node); _recycled._insertNext(node->node_ptr());}
			value_node* _alloc()
			{
				node_ptr next = _recycled._next.load(), after;
				do after = next.get()->_next.load();
				while (!_recycled._next.compare_exchange_weak(next, after));
				if (!next)
				{
					TODO "allocate node storage"
					TODO "push new nodes onto freelist"
				}
				return static_cast<value_node*>(next.get());
			}
		};
	}
}
