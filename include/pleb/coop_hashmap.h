#pragma once


#include "coop_list.h"



namespace coop
{
	namespace unmanaged
	{
		template<
			class Key,
			class Value,
			class Hash     = std::hash<Key>,
			class KeyEqual = std::equal_to<Key>>
		class hashmap :
			protected Hash
		{
		public:
			using key_type   = Key;
			using value_type = Value;
			using hasher     = Hash;
			using hash_type  = size_t;

			static const size_t HASH_BITS = sizeof(hash_type)*8;

		protected:
			using list_t = forward_list<T>;
			using node_t = typename list_t::node;

		protected:
			forward_list _list;
		};
	}

	/*
		A concurrent wait-free atomic hashmap based on the split-ordered list technique,
			with some differences:  most significant bits are used rather than bit reversal...

	*/
	template<
		class Key,
		class Value,
		class Hash     = std::hash<Key>,
		class KeyEqual = std::equal_to<Key>>
	class wait_free_map :
		protected Hash
	{
	public:
		using key_type   = Key;
		using value_type = Value;
		using hasher     = Hash;
		using hash_type  = size_t;

		static const size_t HASH_BITS = sizeof(hash_type)*8;


	public:
		wait_free_map()
		{
		}
		~wait_free_map()
		{
		}

		bool get(const key_type &key, value_type &result) const;
		{
			table *table = _table.load(std::memory_order_acquire);
			auto   hash  = Hash::operator()(key) & hash_type(1);
			auto   index = hash >> (HASH_BITS - table->index_bits);
			node  *node  = table->nodes[index].load(std::memory_order_acquire);
			while (node && node->hash <= hash)
			{
				// TODO need to retain nodes during read
				if (node->hash == hash)
				{
					result = node->value();
					return true;
				}
				node = node->next.load(std::memory_order_acquire);
			}
			if (!node) return false;
		}


	private:
		// Nodes may be dummies (1 per bucket) or values.
		struct node
		{
			hash_type          hash;
			std::atomic<node*> next;

			bool is_value() const    {return hash & 1;}
			value&  value()          {return static_cast<valnode*>(this)->value();}


			bool insert(node &new_node)
			{
				node *node = this;
				while (true)
				{
					node *next = node->next.load(std::memory_order_acquire);
					if (next->hash >= new_node.hash)
					{

					}
				}
			}
			void erase(const key_type &key, hash_type key_hash)
			{

			}
		};
		struct valnode : public node
		{
			key_type   key;
			value_type value;
		};

		struct table
		{
			size_t             index_bits;
			std::atomic<node*> nodes[1];
		};

		std::atomic<table*> _table;
	};
}
