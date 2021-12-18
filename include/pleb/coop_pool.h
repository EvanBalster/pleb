#pragma once


#include <cassert>
#include <utility>    // std::forward for emplace
#include <memory>     // std::shared_ptr and weak_ptr
#include <atomic>
#include <stdexcept>  // std::logic_error


/*
	This header defines wait-free "co-operative" allocation pools.
		These are collections of vacant and occupied residences for objects.
		Emplacing, removing and iterating over valid elements are wait-free operations.
		Priority is placed on iterator performance.

	"Co-operative sets" are owned by their members, unlike traditional collections.
		The existence of the collective container is supported by item within;
		hence, it shall not cease to exist until after its last member does.
		For brevity, we use the term "coop" in code.
*/


namespace coop
{
	/*
		The 'unmanaged' namespace defines coops which do not ensure member ownership.
			Users of the unmanaged containers must guarantee that members hold some
			direct or indirect ownership 
	*/
	namespace unmanaged
	{
		/*
			A coop with space for only a single object.
				Used as a building block for more complex coops.

			This class is 'unmanaged' [see warning above].
		*/
		template<typename T>
		class slot
		{
		public:
			using value_type = T;


		public:
			slot() : _access(0)  {}
			~slot() {assert(empty());}

			/*
				Access the slot like a weak_ptr.
			*/
			[[nodiscard]]
			std::shared_ptr<T> lock() const noexcept    {std::shared_ptr<T> r; if (_open()) {r = _weak_t.lock();     } _close(); return r;}
			std::weak_ptr  <T> weak() const noexcept    {std::weak_ptr  <T> r; if (_open()) {r = _weak_t;            } _close(); return r;}
			long use_count()          const noexcept    {long r=0;             if (_open()) {r = _weak_t.use_count();} _close(); return r;}
			bool expired()            const noexcept    {bool r=1;             if (_open()) {r = _weak_t.expired  ();} _close(); return r;}
			bool empty()              const noexcept    {return expired();}

			/*
				Try to create a value in the slot, returning shared_ptr which is empty on failure.
					May fail despite an empty slot if a read is in progress in another thread.
			*/
			template<typename ... Args>
			[[nodiscard]] std::shared_ptr<T> try_emplace(Args && ... args)
			{
				// Lock this slot if it is not being read, then attempt to fill it if it is empty.
				//   The initial check for !expired is a double-checking optimization.
				std::shared_ptr<T> new_t = nullptr;
				if (empty())
					if (_write_begin())
				{
					if (_weak_t.expired())
						_weak_t = new_t = std::shared_ptr<T>(new (_buf) T(std::forward<Args>(args) ...), deleter{});
					_write_finish();
				}
				return new_t;
			}

		private:
			struct deleter {void operator()(T *t) const noexcept    {t->~T();}};

			bool _open () const noexcept    {return _access.fetch_add(1)>=0;}
			void _close() const noexcept    {       _access.fetch_sub(1);}

			bool _write_begin(size_t spins = 20) const noexcept    {do {int r = 0; if (_access.compare_exchange_strong(r, -0x10000)) return true;} while (spins--); return false;}
			void _write_finish()                 const noexcept    {_access.fetch_add(0x10000);}

			// TODO need a lock-free atomic weak pointer
			alignas(T) char          _buf[sizeof(T)];
			std::weak_ptr<T>         _weak_t;   // TODO half the weak pointer is wasted memory because it's pointing to _buf.
			mutable std::atomic<int> _access;
		};

		/*
			An iterator over an array of slots which selects only
				slots containing an item.  Items indicated by the
				iterator will be retained by an internal shared_ptr.
		*/
		template<typename T, typename Container>
		class slot_iterator
		{
		public:
			using container  = Container;
			using slot_type  = unmanaged::slot<T>;
			using value_type = T;

		public:
			slot_iterator()                               noexcept    : _buff(nullptr), _slot(nullptr) {}
			slot_iterator(const container *buffer)        noexcept    : _buff(buffer), _slot(_buff->slot_begin()) {advance();}

			slot_iterator& operator++()                   noexcept    {++_slot; _element = nullptr; advance(); return *this;}
			slot_iterator  operator++(int)                noexcept    {slot_iterator prev = *this; ++*this; return prev;}

			bool operator==(const slot_iterator &o) const noexcept    {return _slot==o._slot;}
			bool operator!=(const slot_iterator &o) const noexcept    {return _slot!=o._slot;}

			value_type &operator*()                 const noexcept    {return *_element;}
			operator std::shared_ptr<value_type>()  const noexcept    {return _element;}


		protected:
			const container            *_buff;
			const slot_type            *_slot;
			std::shared_ptr<value_type> _element;

			bool advance() noexcept
			{
				if (_slot) while (_slot < _buff->slot_end())
				{
					if ((_element = _slot->lock())) return true;
					++_slot;
				}
				_slot = 0; return false;
			}
		};

		/*
			A fixed-size atomic array coop of non-atomic objects.

			This class is 'unmanaged' [see warning above].
		*/
		template<typename T, size_t StaticCapacity = 8>
		class buffer
		{
		public:
			using value_type     = T;
			using unmanaged_slot = unmanaged::slot<T>;
			
			using iterator = slot_iterator<T, buffer<T>>;


		public:
			/*
				Instantiate the buffer.
					Capacity is allowed to be larger than the class itself, if allocated accordingly.
			*/
			buffer(size_t capacity = StaticCapacity) noexcept    :
				_end(_slots+capacity)
			{
				// Initialize slots beyond static capacity
				for (auto *el = _slots+StaticCapacity; el < _end; ++el) new (el) unmanaged_slot;
			}
			~buffer() noexcept
			{
				// Finalize slots beyond static capacity
				for (auto *el = _slots+StaticCapacity; el < _end; ++el) el->~unmanaged_slot();
			}

			/*
				Get the run-time capacity of the buffer.
			*/
			size_t capacity() const noexcept    {return _end-_slots;}

			/*
				Iterate over non-empty array slots.
					Iterators keep the referenced alement alive temporarily.
			*/
			iterator begin() const noexcept   {return iterator(this);}
			iterator end()   const noexcept   {return iterator();}
			
			/*
				Iterate over all slots.
			*/
			unmanaged_slot *slot_begin() noexcept    {return _slots;}
			unmanaged_slot *slot_end  () noexcept    {return _end;}
			const unmanaged_slot *slot_begin() const noexcept    {return _slots;}
			const unmanaged_slot *slot_end  () const noexcept    {return _end;}


		private:
			unmanaged_slot *_end; // TODO not standard layout
			unmanaged_slot  _slots[StaticCapacity];
		};

		/*
			An atomic container for an unordered list of non-atomic objects

			This class is 'unmanaged' [see warning above].
		*/
		template<typename T>
		class pool
		{
		public:
			enum {basic_capacity = 8};

			using value_type = T;
			
			//using iterator = slot_iterator<T, pool<T>>;

		private:
			using slot = unmanaged::slot<T>;
			
			using buffer = unmanaged::buffer<T, basic_capacity>;

			class iterator;

			class buffer_chain
			{
			public:
				buffer_chain(size_t capacity = basic_capacity)     : _buffer(capacity), _next(nullptr) {}

				~buffer_chain() noexcept    {if (auto next = _next.load(std::memory_order_acquire)) _free(next);}

				buffer_chain *more(size_t expand_size = 0)
				{
					if (auto next = _next.load(std::memory_order_acquire)) return next;

					if (!expand_size) expand_size = 2*_buffer.capacity();
					if (expand_size < basic_capacity) expand_size = basic_capacity;

					buffer_chain *existed = nullptr, *created = _alloc(expand_size);
					if (_next.compare_exchange_strong(existed, created)) return created;
					else /* if someone beat us to it */ {_free(created); return existed;}
				}

				slot *slot_begin() noexcept    {return _buffer.slot_begin();}
				slot *slot_end  () noexcept    {return _buffer.slot_end();}
				const slot *slot_begin() const noexcept    {return _buffer.slot_begin();}
				const slot *slot_end  () const noexcept    {return _buffer.slot_end();}

			private:
				static buffer_chain *_alloc(size_t capacity)
				{
					return new
						(new char[sizeof(buffer_chain) + sizeof(slot) * (capacity - basic_capacity)])
						buffer_chain(capacity);
				}
				static void          _free (buffer_chain *ch) noexcept    {ch->~buffer_chain(); delete[] (char*) ch;}
			
				friend class pool::iterator;
				std::atomic<buffer_chain*> _next = nullptr;
				buffer                     _buffer;
			};

		public:
			class iterator :
				public slot_iterator<T, buffer_chain>
			{
				using _super = slot_iterator<T, buffer_chain>;
				
			public:
				iterator()                 noexcept    : _super() {}
				iterator(const pool *pool) noexcept    : _super(&pool->_first) {if (!_slot) {_slot = _buff->slot_end();} if (!_element) advance();}

				iterator& operator++()    noexcept    {++_slot; _element = nullptr; advance(); return *this;}
				iterator  operator++(int) noexcept    {iterator prev = *this; ++*this; return prev;}

				using _super::operator==;
				using _super::operator!=;
				using _super::operator*;


			protected:
				using _super::_buff;
				using _super::_slot;
				using _super::_element;
				
				bool advance()
				{
					while (_slot)
					{
						if (_slot >= _buff->slot_end())
						{
							if (!(_buff = _buff->_next.load(std::memory_order_acquire)))
								{_slot = nullptr; break;}
							else _slot = _buff->slot_begin();
						}
						if ((_element = _slot->lock())) return true;
						else ++_slot;
					}
					return false;
				}
			};

		public:
			pool()           {}
			~pool() noexcept {}

			/*
				Iterate over elements in the pool.
					Iterators keep the referenced alement alive temporarily.
			*/
			iterator begin() const   {return iterator(this);}
			iterator end()   const   {return iterator();}

			/*
				Allocate a value in the pool.
					Always succeeds unless an eception is thrown.
			*/
			template<typename ... Args> [[nodiscard]]
			std::shared_ptr<value_type> emplace(Args&& ... args)
			{
				struct pool_deleter {void operator()(value_type *v) const noexcept {v->~value_type();}};

				for (buffer_chain *buf = &this->_first; buf; buf = buf->more())
					for (auto i = buf->slot_begin(), e = buf->slot_end(); i != e; ++i)
						if (auto ptr = i->try_emplace(std::forward<Args>(args) ...))
							return ptr;

				// Should be unreachable in practice (std::bad_alloc is the fail condition)
				return nullptr;
			}

		protected:
			pool(const pool&) = delete;
			pool(pool&&) = delete;
			void operator=(const pool&) = delete;
			void operator=(pool&&) = delete;

		private:
			friend class iterator;
			buffer_chain _first;
		};
	}


	/*
		This wrapper adds reverse ownership to an object.
	*/
	template<typename Value, typename Coop>
	class membership
	{
	public:
		Value                       value;
		const std::shared_ptr<Coop> container;

		template<typename ... Args>
		membership(std::shared_ptr<Coop> _container, Args&& ... args)
			:
			value    (std::forward<Args>(args)...),
			container(std::move(_container)) {}
	};


	/*
		An atomic single-item container, guaranteed to outlive the contained item.
			Behaves like std::weak_ptr but with the ability to replace an expired item.
	*/
	template<typename T>
	class slot :
		public std::enable_shared_from_this<slot<T>>
	{
	public:
		using value_type     = T;
		using element_type   = membership<T, slot<T>>;
		using unmanaged_slot = unmanaged::slot<element_type>;


	public:
		// This class must be created via shared_ptr.
		static std::shared_ptr<slot> create()    {return std::make_shared<constructor>();}
		~slot() noexcept {}

		// Access the slot like a weak_ptr.
		std::shared_ptr<T> lock() const noexcept    {auto p=_slot.lock(); if (!p) return 0; return std::shared_ptr<T>(p, &p->value);}
		long use_count()          const noexcept    {return _slot.use_count();}
		bool expired()            const noexcept    {return _slot.expired();}
		bool empty()              const noexcept    {return _slot.empty();}

		/*
			Try to create a value in the slot, returning shared_ptr which is empty on failure.
				May fail despite an empty slot if a read is in progress in another thread.
		*/
		template<typename ... Args> [[nodiscard]]
		std::shared_ptr<T> try_emplace(Args && ... args)
		{
			auto result = _slot.try_emplace(std::forward<Args>(args)...);
			if (!result) return 0;
			return std::shared_ptr<T>(std::move(result), &result->value);
		}


	protected:
		slot() {}
		struct constructor : public slot {};

		slot(const slot&) = delete;
		slot(slot&&) = delete;
		void operator=(const slot&) = delete;
		void operator=(slot&&) = delete;


	private:
		unmanaged_slot _slot;
	};

	
	/*
		An atomic pool of values, guaranteed to outlive the final item.
			Items are managed by shared_ptr<Value> but allocated from contiguous buffers.
			Each item is augmented with a wrapper that holds a shared_ptr to the pool.
	*/
	template<typename T>
	class pool :
		public std::enable_shared_from_this<pool<T>>
	{
	public:
		using value_type     = T;
		using element_type   = membership<T, pool<T>>;
		using unmanaged_pool = unmanaged::pool<element_type>;

		class iterator : public unmanaged_pool::iterator
		{
		public:
			using unmanaged_pool::iterator::iterator;

			value_type                   &operator*() const noexcept    {return this->_element->value;}
			std::shared_ptr<element_type> element()   const noexcept    {return this->_element;}
		};


	public:
		/*
			This class must always be owned by shared pointer.
		*/
		static std::shared_ptr<pool> create()    {return std::make_shared<constructor>();}
		~pool() noexcept {}

		/*
			Iterate over elements in the pool.
				Iterators keep the referenced alement alive temporarily.
		*/
		iterator begin() const   {return iterator(&_pool);}
		iterator end()   const   {return iterator();}

		/*
			Allocate a value in the pool.
				Always succeeds unless an eception is thrown.
				The resulting shared_ptr keeps the value and the pool alive.
		*/
		template<typename ... Args> [[nodiscard]]
		std::shared_ptr<value_type> emplace(Args&& ... args)
		{
			auto elem_ptr = emplace_element(std::forward<Args>(args) ...);
			return std::shared_ptr<value_type>(std::move(elem_ptr), &elem_ptr->value);
		}
		template<typename ... Args> [[nodiscard]]
		std::shared_ptr<element_type> emplace_element(Args&& ... args)
		{
			return _pool.emplace(this->shared_from_this(), std::forward<Args>(args) ...);
		}


	protected:
		pool() {}
		struct constructor : public pool {};

		pool(const pool&) = delete;
		pool(pool&&) = delete;
		void operator=(const pool&) = delete;
		void operator=(pool&&) = delete;


	private:
		unmanaged_pool _pool;
	};
	
}
