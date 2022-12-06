#pragma once


/*
	This class defines a concurrent hashtable protected by a read-write mutex.
	It is a provisional solution for PLEB pending a wait-free hashtable implementation
	based on split ordered lists or some comparable technique.
*/


// Allow shared_mutex to be replaced with another implementation
#ifdef PLEB_REPLACEMENT_SHARED_MUTEX_HEADER
	#include PLEB_REPLACEMENT_SHARED_MUTEX_HEADER
#else
	#include <shared_mutex>
#endif
#include <memory>
#include <unordered_map>


namespace coop
{
#ifdef PLEB_REPLACEMENT_SHARED_MUTEX
	using std_shared_mutex = PLEB_REPLACEMENT_SHARED_MUTEX;
#else
	using std_shared_mutex = std::shared_mutex;
#endif
#ifdef PLEB_REPLACEMENT_UNIQUE_LOCK
	template<class T> using std_unique_lock = PLEB_REPLACEMENT_UNIQUE_LOCK<T>;
#else
	template<class T> using std_unique_lock = std::unique_lock<T>;
#endif
#ifdef PLEB_REPLACEMENT_SHARED_LOCK
	template<class T> using std_shared_lock = PLEB_REPLACEMENT_SHARED_LOCK<T>;
#else
	template<class T> using std_shared_lock = std::shared_lock<T>;
#endif


	namespace detail // Workarounds for pseudo-heterogeneous lookup
	{
		template<typename T>
		struct key_view
		{
			using type = const T&;
			static type view(const T &t) noexcept {return t;}
		};

		template<>
		struct key_view<std::string>
		{
			using type = std::string_view;
#if __cplusplus >= 202000 || _MSVC_LANG >= 202000
			static const std::string_view& view(const std::string_view &t) noexcept    {return t;}
#else
			static std::string view(std::string_view t) noexcept    {return std::string(t);}
#endif
		};
	}



	template<typename Key, typename Value, typename Hash = std::hash<Key>>
	class locking_weak_table
	{
	public:
		using key_type      = Key;
		using key_reference = typename detail::key_view<key_type>::type;
		using value_type    = Value;

	public:
		bool set(key_reference key, std::weak_ptr<value_type> value) noexcept
		{
			unique_lock lock(_mtx);

			auto pos = _find(key);
			if (pos != _map.end()) _map.erase(pos);
			return _map.emplace(key, std::move(value)).second;
		}

		bool remove(key_reference key) noexcept
		{
			unique_lock lock(_mtx);

			auto pos = _find(key);
			if (pos != _map.end()) {_map.erase(pos); return true;}
			return false;
		}

		void clear() noexcept    {std::unique_lock lock(_mtx); _map.clear();}

		[[nodiscard]]
		std::shared_ptr<value_type> get(key_reference key) const noexcept
		{
			shared_lock lock(_mtx);

			auto pos = _find(key);
			if (pos == _map.end()) return {};
			return pos->second.lock();
		}

		template<typename ConstructorType, typename... Args>
		[[nodiscard]]
		std::shared_ptr<value_type> find_or_create(key_reference key, Args && ... args) noexcept
		{
			{
				shared_lock lock(_mtx);
				auto pos = _find(key);
				if (pos != _map.end())
				{
					auto result = pos->second.lock();
					if (result) return result;
				}
			}
			{
				unique_lock lock(_mtx);
				auto make = std::make_shared<ConstructorType>(std::forward<Args>(args) ...);
				_map[detail::key_view<key_type>::view(key)] = make;
				return make;
			}
		}

		// Try to insert a shared_ptr.
		bool try_insert(key_reference key, std::shared_ptr<value_type> ptr)
		{
			unique_lock lock(_mtx);
			auto &elem = _map[detail::key_view<key_type>::view(key)];
			if (elem.expired()) {elem = std::move(ptr); return true;}
			else                                        return false;
		}


	private:
		using _map_t = std::unordered_map<key_type, std::weak_ptr<value_type>, Hash>;
		mutable std_shared_mutex _mtx;
		_map_t                   _map;

		using unique_lock = std_unique_lock<std_shared_mutex>;
		using shared_lock = std_shared_lock<std_shared_mutex>;

		// No heterogeneous lookup before C++20...
		typename _map_t::const_iterator _find(key_reference key) const noexcept
		{
			return _map.find(detail::key_view<key_type>::view(key));
		}
	};
}