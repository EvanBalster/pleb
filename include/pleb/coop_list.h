#pragma once

#include <cstddef>
#include "coop_pool.h"


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
			class data_node;
			class bookmark_node;

			class iterator;

			static const uintptr_t
				node_data_flag    = 1,
				node_release_flag = 2, // Flag bookmarks for removal
				sentinel_end_of_list = 0,
				sentinel_out_of_list = 1,
				node_ptr_mask = ~(node_data_flag & node_release_flag);

			// A tagged pointer indicating the next node and its type (data or bookmark)
			//    Also contains a flag for whether the HOLDER of this pointer was released.
			struct node_ptr
			{
				uintptr_t raw;

				bool is_data()     const noexcept    {return raw&node_data_flag;}
				bool prev_expire() const noexcept    {return raw&node_release_flag;}

				bool is_excised() const noexcept    {return raw == sentinel_out_of_list;}

				explicit operator bool() const noexcept    {return bool(raw&node_ptr_mask);}

				node *get()      const    {return reinterpret_cast<node*>(raw&node_ptr_mask);}
				operator node*() const    {return get();}
			};

			struct atomic_node_ptr
			{
				std::atomic<uintptr_t> raw;

				atomic_node_ptr() : raw(sentinel_end_of_list) {}

				void set_prev_expire(std::memory_order order = std::memory_order_seq_cst) noexcept    {raw.fetch_or(node_release_flag);}
				void set_prev_excised() noexcept                                                      {raw.store(sentinel_out_of_list);}

				node_ptr load(               std::memory_order order = std::memory_order_seq_cst) const    {return {raw.load(order)};}
				void store(const node_ptr v, std::memory_order order = std::memory_order_seq_cst)          {raw.store(v.raw,order);}

				bool compare_exchange_weak(node_ptr &expected, node_ptr desired) noexcept    {return raw.compare_exchange_weak(expected.raw, desired.raw);}
			};

			class node
			{
			public:
				node() {}

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
				Bookmarks are dummy nodes skipped during iteration.
			*/
			class bookmark_node : public node
			{
			public:
				node_ptr node_ptr() const noexcept    {return {uintptr_t(this)};}

				void mark_for_removal()    {_next.set_prev_expire();}

			protected:
				friend class iterator;
				mutable shared_trylock _rw;
			};

			/*
				Data nodes contain... data
			*/
			class data_node : public node
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
				forward_list      *_list;
				node              *_pos;
				std::shared_ptr<T> _item;

			public:
				node_iterator()                            noexcept    : _list(0), _pos(0) {}
				node_iterator(forward_list &l)             noexcept    : _list(&l), _pos(0) {_follow(l._head);}
				node_iterator(forward_list &l, node &from) noexcept    : _list(&l), _pos(0) {_follow(from);}

				bool not_end() const noexcept    {return  _pos;}
				bool is_end () const noexcept    {return !_pos;}

				node_iterator &operator++() noexcept    {_follow(*_pos); return *this;}

				bool operator==(const iterator &o) const noexcept    {return _pos == o._pos;}
				bool operator!=(const iterator &o) const noexcept    {return _pos != o._pos;}

				T *operator->() const noexcept    {return  _item.get();}
				T &operator* () const noexcept    {return *_item.get();}

				operator std::shared_ptr<T>() const noexcept    {return value();}

				std::shared_ptr<T> value  ()  const noexcept    {return _item;}

				// Get a shared pointer to the value and set this iterator to "end of list".  Does not alter the list.
				std::shared_ptr<T> release()        noexcept    {_pos = nullptr; return std::move(_item);}

				// Insert a node before this iterator's position.
				//void insert(node_slot *new_slot) const    {_node->insert(new_slot);}

			
			protected:
				// Move to the first un-expired element in the list after "from".
				//  This function assumes the node holding _prev is retained and will not expire.
				bool _follow(node &from) noexcept
				{
					node_ptr pos;

				reload:
					pos = from._next.load(std::memory_order_acquire);
					
					while (true)
					{
						_pos = pos.get();
						if (pos.is_data())
						{
							// Attempt to acquire the item at this new node.
							auto node = static_cast<data_node*>(_pos);
							if (auto data_ptr = node->slot.lock())
							{
								// Double check that the node has not suddenly been reallocated
								if (from._next.load(std::memory_order_acquire) != _pos) goto reload;

								_item = std::move(data_ptr);
								return true;

							}
						}
						else
						{
							auto node = static_cast<bookmark_node*>(_pos);
							auto status = node->_next.load(std::memory_order_acquire); // relaxed?
							if (status.prev_expire())
							{
								// This node is marked for removal; attempt to excise it...
								if (!)
							}
						}

						// The next node is expired.  Remove it.
						node_ptr after = _pos->_next.load();
						if (start._next.compare_exchange_weak(pos, after))
						{
							// Excised this node; clean up.
							if (pos.is_data()) _list->_free(_pos);
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
				void _skip_non_data() noexcept    {while (_pos && !_value) ++*this;}
			};

		public:
			forward_list()    : _size(0) {}

			~forward_list()    {TODO "delete node storage"}

			/*
				Iterate over values in this list.
			*/
			iterator begin() noexcept    {return iterator(*this);}
			iterator end  () noexcept    {return iterator();}

			/*
				Get an iterator starting just after the given node.
			*/
			iterator after(node &node) noexcept    {return iterator(*this, node);}

			// Access the item at the front of the list.
			std::shared_ptr<value_type> front()    {auto i = begin(); return i.release();}


			// Insert an element after the given iterator's position.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_after(const iterator &pos, Args&& ... args)    {if (!pos._pos) return nullptr; return _emplace(pos._pos, std::forward<Args>(args)...);}

			// Insert an element at the head of the list.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_front(Args&& ... args)    {return _emplace(_head, std::forward<Args>(args)...);}


			// Emplace after the given node.  Be careful with this function; don't mix nodes from different lists.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_after(node &node, Args&& ... args)    {return _emplace(node, std::forward<Args>(args)...);}


			/*
				Insert nodes manually.
					These can be used as starting
			*/
			void insert_after(const iterator &pos, bookmark_node &node)    {pos._pos->_insertNext(node.node_ptr());}


		protected:
			node                _head, _recycled;
			std::atomic<size_t> _size;

			friend class iterator;

			template<typename... Args> [[nodiscard]]
			std::shared_ptr<value_type> _emplace(node &previous, Args&& ... args)
			{
				data_node *node = _alloc();
				auto ref = node->slot.try_emplace(std::forward<Args>(args) ...);
				if (ref) previous._insertNext(node->node_ptr());
				else     _free(node);
				return std::move(ref);
			}

			void  _free(data_node *node) noexcept    {_recycled._insertNext(node->node_ptr());}
			data_node* _alloc()
			{
				node *next = _recycled.load(), *after;
				do after = next->_next.load();
				while (!_recycled.compare_exchange_weak(next, after));
				if (!next)
				{
					TODO "allocate node storage"
					TODO "push new nodes onto freelist"
				}
				return static_cast<data_node*>(next);
			}
		};
	}
}
