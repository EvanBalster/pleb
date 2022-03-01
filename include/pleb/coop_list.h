#pragma once

#include <cstddef>
#include "coop_pool.h"


/*
	This header defines a cooperative wait-free forward list,
		which may also be used as a stack.

	In addition to values, lists may contain "bookmark" nodes.
		Bookmarks may be used as starting points for iteration,
		but will be skipped over when iterating.
		Bookmarks CANNOT BE DELETED and must outlive the list.
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

			class iterator;

			class node
			{
			public:
				node() : _next(nullptr) {}

				// No copying!!
				node(const node&) = delete;
				void operator=(const node&) = delete;

			private:
				friend class forward_list;
				friend class iterator;
				std::atomic<node*> _next;

				// Add this node to a list, making the argument point to it.
				//   Behavior is undefined if this node is already in a list.
				void _injectAfter(node &previous)
				{
					node *after = previous._next.load();
					do this->_next.store(after);
					while (!previous._next.compare_exchange_weak(after, this));
				}
			};

			/*
				Bookmarks are dummy nodes skipped during iteration.
			*/
			class bookmark : public node
			{
			protected:
				friend class iterator;
				mutable read_write_gate _rw;
			};

			class data_node : public node
			{
			public:
				slot slot;
			};

			class iterator
			{
			protected:
				forward_list      *_list;
				data_node         *_pos;
				std::shared_ptr<T> _item;

			public:
				iterator()                            noexcept    : _list(nullptr), _pos(nullptr) {}
				iterator(forward_list &l)             noexcept    : _list(&l), _pos(nullptr) {acquire(&l._head);}
				iterator(forward_list &l, node &from) noexcept    : _list(&l), _pos(nullptr) {acquire(&from);}

				bool not_end() const noexcept    {return  _pos;}
				bool is_end () const noexcept    {return !_pos;}

				iterator &operator++() noexcept    {acquire(_pos); return *this;}

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
				// Without changing _prev, set _pos and _element to the next item in the list.
				//  This function assumes the node holding _prev is retained and will not expire.
				bool acquire(node *from) noexcept
				{
				start:
					// Acquire the next data node, unless this is the end of the list...
					while (true)
					{
						node *next = from->_next.load(std::memory_order_acquire);
						if (!next) {_pos = nullptr; _item.reset(); return false;}

						if (ptrdiff_t(next) & 1)
					}
					while (true)
					{
						// Attempt to acquire the item at this new node.
						if (auto new_item = _pos->slot.lock())
						{
							// Double check that the node has not suddenly been reallocated
							if (prev_node._next.load(std::memory_order_acquire) != _pos) goto start;

							_item = std::move(new_item);
							return true;
							
						}
						else // Remove the following, now-expired node
						{
							node *after = _pos->_next.load();
							if (prev_node._next.compare_exchange_weak(_pos, after))
								_list->_free(_pos);
						}
					}
				}
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
			void insert_after(const iterator &pos, bookmark &node)    {node._injectAfter(*pos._pos);}


		protected:
			bookmark            _head, _recycled;
			std::atomic<size_t> _size;

			friend class iterator;

			template<typename... Args> [[nodiscard]]
			std::shared_ptr<value_type> _emplace(node &previous, Args&& ... args)
			{
				data_node *node = _alloc();
				auto ref = node->slot.try_emplace(std::forward<Args>(args)...);
				if (ref) node->_injectAfter(previous);
				else     _free(node);
				return std::move(ref);
			}

			void  _free(data_node *node) noexcept    {node->_inject(_recycled);}
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
