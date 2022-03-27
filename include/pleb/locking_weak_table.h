#pragma once


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


	template<typename T>
	class locking_weak_table
	{
	public:
		using value_type = T;

	public:
		bool set(std::string_view name, std::weak_ptr<T> value) noexcept
		{
			unique_lock lock(_mtx);

			auto pos = _find(name);
			if (pos != _map.end()) _map.erase(pos);
			return _map.emplace(name, std::move(value)).second;
		}

		bool remove(std::string_view name) noexcept
		{
			unique_lock lock(_mtx);

			auto pos = _find(name);
			if (pos != _map.end()) {_map.erase(pos); return true;}
			return false;
		}

		void clear() noexcept    {std::unique_lock lock(_mtx); _map.clear();}

		[[nodiscard]]
		std::shared_ptr<T> get(std::string_view name) const noexcept
		{
			shared_lock lock(_mtx);

			auto pos = _find(name);
			if (pos == _map.end()) return {};
			return pos->second.lock();
		}

		template<typename ConstructorType, typename... Args>
		[[nodiscard]]
		std::shared_ptr<T> find_or_create(std::string_view name, Args && ... args) noexcept
		{
			{
				shared_lock lock(_mtx);
				auto pos = _find(name);
				if (pos != _map.end())
				{
					auto result = pos->second.lock();
					if (result) return result;
				}
			}
			{
				unique_lock lock(_mtx);
				auto make = std::make_shared<ConstructorType>(std::forward<Args>(args) ...);
				_map[std::string(name)] = make;
				return make;
			}
		}

		// Try to insert a shared_ptr.
		bool try_insert(std::string_view name, std::shared_ptr<T> ptr)
		{
			unique_lock lock(_mtx);
			auto &elem = _map[std::string(name)];
			if (elem.expired()) {elem = std::move(ptr); return true;}
			else                                        return false;
		}


	private:
		using _map_t = std::unordered_map<std::string, std::weak_ptr<T>>;
		mutable std_shared_mutex _mtx;
		_map_t                   _map;

		using unique_lock = std_unique_lock<std_shared_mutex>;
		using shared_lock = std_shared_lock<std_shared_mutex>;

		// No heterogeneous lookup before C++20...
		typename _map_t::const_iterator _find(const std::string_view name) const noexcept    {return _map.find(std::string(name));}
	};
}