#pragma once

#include <cstddef>
#include "coop_pool.h"


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
			using next_ptr = std::atomic<node*>;

			class iterator;

			class node
			{
			private:
				friend class forward_list;

				slot               slot;

			public:
				node() : _next(nullptr) {}

			private:
				std::atomic<node*> _next;

				// Add this node to a list, making the argument point to it.
				//   Behavior is undefined if this node is already in a list.
				void _inject(std::atomic<node*> &nextptr)
				{
					node *after = nextptr.load();
					do this->_next.store(after);
					while (!nextptr.compare_exchange_weak(after, this));
				}
			};

			class iterator
			{
			protected:
				friend class node;
				forward_list      *_list;
				node              *_pos;
				std::shared_ptr<T> _item;

			public:
				iterator()                noexcept    : _list(nullptr), _pos(nullptr) {}
				iterator(forward_list &l) noexcept    : _list(&l), _pos(nullptr) {acquire(l._head);}

				iterator &operator++() noexcept    {acquire(_pos->_next); return *this;}

				bool operator==(const iterator &o) const noexcept    {return _pos == o._pos;}
				bool operator!=(const iterator &o) const noexcept    {return _pos != o._pos;}

				// Insert a node before this iterator's position.
				//void insert(node_slot *new_slot) const    {_node->insert(new_slot);}

			
			protected:
				// Without changing _prev, set _pos and _element to the next item in the list.
				//  This function assumes the node holding _prev is retained and will not expire.
				bool acquire(std::atomic<node*> &next_ptr) noexcept
				{
				start:
					// Acquire the next element, unless this is the end of the list...
					_pos = next_ptr.load(std::memory_order_acquire);
					if (!_pos) {_item.reset(); return false;}
					while (true)
					{
						// Attempt to acquire the item at this new node.
						if (auto new_item = _pos->slot.lock())
						{
							// Double check that the node has not suddenly been reallocated
							if (next_ptr.load(std::memory_order_acquire) != _pos) goto start;

							_item = std::move(new_item);
							return true;
							
						}
						else // Remove the following, now-expired node
						{
							node *after = _pos->_next.load();
							if (next_ptr.compare_exchange_weak(_pos, after))
								_list->_free(_pos);
						}
					}
				}
			};

		public:
			forward_list()    : _head(nullptr), _recycled(nullptr) {}

			~forward_list()    {TODO "delete node storage"}

			// Iterate over elements.
			iterator begin()    {return iterator(*this);}
			iterator end  ()    {return iterator();}

			// Insert an element before the given iterator's position.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace(const iterator &pos, Args&& ... args)
			{
				auto node_ptr = _pool.emplace(pos, std::forward<Args>(args)...);
				return std::shared_ptr<T>(node_ptr, &node_ptr->value);
			}

			// Insert an element at the head of the list.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace_front(Args&& ... args)
			{
				node *new_node = alloc_node();
				auto result = node->try_emplace(std::forward<Args>(args)...);
				while (true)
				{
					auto first = _head.load(std::memory_order_acquire);
					new_node->next.store(first, std::memory_order_release);
					if (_head.compare_exchange_weak(first, new_node)) break;
				}
				return result;
			}


		protected:
			std::atomic<node*> _head, _recycled;

			friend class iterator;
			void  _free(node *node) noexcept    {node->_inject(_recycled);}
			node* _alloc()
			{
				node *next = _recycled.load(), *after;
				do after = next->_next.load();
				while (!_recycled.compare_exchange_weak(next, after));
				if (!next)
				{
					TODO "allocate node storage"
					TODO "push new nodes onto freelist"
				}
				return next;
			}
		};
	}
}
