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
			using value_type     = T;
			class node;
			using node_slot = unmanaged::slot<node>;

			using next_ptr = std::atomic<node_slot*>;

			class iterator;

			class node
			{
			private:
				T value;

				friend class forward_list;
				std::atomic<node_slot*>               next;
				std::atomic<std::atomic<node_slot*>*> prev;

			public:
				// Construct a tail node with no value.
				node(std::atomic<node_slot*> &head) :
					next(nullptr), prev(&head) {}

				// Construct and insert a node with a value.
				template<typename ... Args>
				node(node_slot *slot, const iterator &following, Args&& ... args);

				~node();
			};

			class iterator
			{
			protected:
				friend class node;
				node_slot            *_pos;
				std::shared_ptr<node> _node;

			public:
				iterator()                                    noexcept    : _pos(nullptr) {}
				iterator(const std::atomic<node_slot*> &head) noexcept    : _pos(nullptr) {acquire(head);}

				iterator &operator++() noexcept    {acquire(_node->next); return *this;}

				bool operator==(const iterator &o) const noexcept    {return _pos == o._pos;}
				bool operator!=(const iterator &o) const noexcept    {return _pos != o._pos;}

				// Insert a node before this iterator's position.
				//void insert(node_slot *new_slot) const    {_node->insert(new_slot);}

			
			protected:
				// Without changing _prev, set _pos and _element to the next item in the list.
				//  This function assumes the node holding _prev is retained and will not expire.
				bool acquire(const std::atomic<node_slot*> &next_ptr) noexcept
				{
					while (true)
					{
						// Acquire the next element, unless this is the end of the list...
						_pos = next_ptr.load(std::memory_order_acquire);
						if (!_pos) {_node.reset(); break;}

						// Attempt to acquire a shared_ptr. Could fail if the node just expired.
						//  On failure, we retry until we find a valid next node or the end of the list.
						if (auto pos_node = _pos->lock()) {_node = std::move(pos_node); return true;}

						// TODO tail node check?
					}
					return false; // End of list
				}
			};

		public:
			forward_list()    : _head(&_tail) {_tail.try_emplace(_head);}

			// Iterate over elements.
			iterator begin()    {return iterator(_head);}
			iterator end  ()    {return iterator();}

			// Insert an element before the given iterator's position.
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace(const iterator &pos, Args&& ... args)
			{
				node *new_node = alloc_node();
				auto result = node->try_emplace(std::forward<Args>(args)...);
				pos.insert(new_node, _head);
				return result;
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
			std::atomic<node_slot*> _head;
			node_slot               _tail;

			node *alloc_node();
		};
	}


	// Construct and insert a node with a value.
	template<typename T> template<typename ... Args>
	unmanaged::forward_list<T>::node::node(
		node_slot *slot, const iterator &following, Args&& ... args)
		:
		value(std::forward<Args>(args...)), next(following._pos), prev(nullptr)
	{
		// Lock the following node by swapping its prev pointer to null.
		std::atomic<node_slot*> *prevptr;
		while (true)
		{
			prevptr = following._node->prev.load(std::memory_order_acquire);
			if (!prevptr) continue;
			if (prev.compare_exchange_weak(prevptr, nullptr)) break;
		}

		// Splice this very node into the "next" chain.
		prevptr->store            (slot, std::memory_order_release);

		// Build the reverse chain, re-enabling insertion & expiration
		prev.store                (prevptr, std::memory_order_release);
		following._node->prev.store(&next,   std::memory_order_release);
	}

	template<typename T>
	unmanaged::forward_list<T>::node::~node()
	{
		// Temporarily retain the following node and lock its prevptr.
		std::shared_ptr<node> next_node;
		node_slot *next_slot;
		while (true)
		{
			next_slot = next.load(std::memory_order_acquire);
			if (!next_slot) break;
			next_node = next_slot->lock();
			if (!next_node) continue;

			auto expect = &next;
			if (next_node->prev.compare_exchange_weak(expect, nullptr)) break;
		}

		// Our prevptr should never ever be locked or otherwise null unless we were never added to a list.
		auto prevptr = prev.load(std::memory_order_acquire);

		// Route the chain of next pointers around this dying node.
		if (prevptr) prevptr->store(next_slot);

		// Give the next node our prev ptr, unlocking it for other insertions/deletions.
		if (next_node) next_node->prev.store(prevptr);
	}
}
