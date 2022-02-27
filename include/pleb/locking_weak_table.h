#pragma once


#include <shared_mutex>
#include <memory>
#include <unordered_map>


namespace coop
{
	template<typename T>
	class locking_weak_table
	{
	public:
		using value_type = T;

	public:
		bool set(std::string_view name, std::weak_ptr<T> value) noexcept
		{
			std::unique_lock lock(_mtx);

			auto pos = _find(name);
			if (pos != _map.end()) _map.erase(pos);
			return _map.emplace(name, std::move(value)).second;
		}

		bool remove(std::string_view name) noexcept
		{
			std::unique_lock lock(_mtx);

			auto pos = _find(name);
			if (pos != _map.end()) {_map.erase(pos); return true;}
			return false;
		}

		void clear() noexcept    {std::unique_lock lock(_mtx); _map.clear();}

		[[nodiscard]]
		std::shared_ptr<T> get(std::string_view name) const noexcept
		{
			std::shared_lock lock(_mtx);

			auto pos = _find(name);
			if (pos == _map.end()) return {};
			return pos->second.lock();
		}

		template<typename ConstructorType, typename... Args>
		[[nodiscard]]
		std::shared_ptr<T> find_or_create(std::string_view name, Args && ... args) noexcept
		{
			{
				std::shared_lock lock(_mtx);
				auto pos = _find(name);
				if (pos != _map.end())
				{
					auto result = pos->second.lock();
					if (result) return result;
				}
			}
			{
				std::unique_lock lock(_mtx);
				auto make = std::make_shared<ConstructorType>(std::forward<Args>(args) ...);
				_map[std::string(name)] = make;
				return make;
			}
		}

		// Try to insert a shared_ptr.
		bool try_insert(std::string_view name, std::shared_ptr<T> ptr)
		{
			std::shared_lock lock(_mtx);
			auto &elem = _map[std::string(name)];
			if (elem.expired()) {elem = std::move(ptr); return true;}
			else                                        return false;
		}


	private:
		using _map_t = std::unordered_map<std::string, std::weak_ptr<T>>;
		mutable std::shared_mutex _mtx;
		_map_t                    _map;

		// No heterogeneous lookup before C++20...
		typename _map_t::const_iterator _find(const std::string_view name) const noexcept    {return _map.find(std::string(name));}
	};
}