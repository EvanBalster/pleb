#pragma once


#include "coop_list.h"



namespace coop
{
	namespace unmanaged
	{

		static const size_t LIST_SIZE = sizeof(forward_list<int>);

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

			struct entry
			{
				const hash_type hash;
				const key_type  key;
				value_type      value;
			};

			using list_t = forward_list<entry>;
			using node_t = typename list_t::node;
			using bookmark_t = typename list_t::bookmark_node;
			using iterator = typename list_t::iterator;


			static const size_t HASH_BITS = sizeof(hash_type)*8;

			constexpr static size_t _tierBits (size_t tier)    {return 4+2*tier;}
			constexpr static size_t _tierSize (size_t tier)    {return 1<<_tierBits(tier);}
			constexpr static size_t _tierShift(size_t tier)    {return HASH_BITS-_tierBits(tier);}


		public:
			hashmap()    : _table{_table_tier0}, _tier(0) {}

			iterator begin() noexcept    {return _list.begin();}
			iterator end  () noexcept    {return _list.end();}

			iterator insert(const key_type &key, value_type &&value)
			{
				std::atomic<bookmark_t*> _table[15];
				std::atomic<size_t>      _tier;
			}

			iterator find(const key_type &key)
			{
				auto hash = Hash::operator()(key);
				auto tier = _tier.load(std::memory_order_acquire);
				auto *table = _table[tier].load(); // Synchronization?
				auto tableSize = _tierSize(tier);
				
				auto &bucket = table[hash >> _tierShift(tier)];
				
				iterator i = _list.after(bucket);
				for (; i.not_end(); ++i)
				{
					if (i-> hash > hash) {i = _list.end(); break;}
					if (i->hash == hash && KeyEqual()(i->key, key)) break;	
				}
				return i;
			}

		protected:
			forward_list _list;

			// Table sizes 16,64,256 ... max 2^32
			std::atomic<bookmark_t*> _table[15];
			std::atomic<size_t>      _tier;
			bookmark_t               _table_tier0[_tierSize(0)];
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
