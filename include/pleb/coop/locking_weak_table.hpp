#pragma once

#include <type_traits>


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
			static const std::string_view view(const std::string_view &t) noexcept    {return t;}
#else
			static std::string view(std::string_view t) noexcept    {return std::string(t);}
#endif
		};

#if __cplusplus >= 202000 || _MSVC_LANG >= 202000
		template<typename T>
		struct table_hash : public std::hash<T> {};

		template<>
		struct table_hash<std::string> : public std::hash<std::string_view>
		{
			using is_transparent = void;
			
			[[nodiscard]] size_t operator()(const char      *v) const noexcept    {return hash::operator()(v);}
			[[nodiscard]] size_t operator()(std::string_view v) const noexcept    {return hash::operator()(v);}
			[[nodiscard]] auto operator()(const std::string &v) const noexcept    {return hash::operator()(v);}
			//[[nodiscard]] auto operator()(const std::string_view &v) const noexcept    {return hash::operator()(v);}
		};
#else
		template<typename T> using table_hash = std::hash<T>;
#endif
	}



	template<typename Key, typename Value, typename Hash = detail::table_hash<Key>>
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
		std::shared_ptr<value_type> find(key_reference key) const noexcept
		{
			shared_lock lock(_mtx);

			auto pos = _find(key);
			if (pos == _map.end()) return {};
			return pos->second.lock();
		}

		template<typename ConstructorType, typename... Args>
		[[nodiscard]]
		std::shared_ptr<value_type> find_or_create(key_reference key, Args && ... args) noexcept(noexcept(std::make_shared<ConstructorType>(std::forward<Args>(args) ...)))
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
				auto ins = _map.emplace(key, make);
				if (!ins.second) ins.first->second = make;
				return make;
			}
		}

		// Try to insert a shared_ptr.
		bool try_insert(key_reference key, std::shared_ptr<value_type> ptr)
		{
			unique_lock lock(_mtx);
			auto i = _map.find(detail::key_view<key_type>::view(key));
			if (i==_map.end()) {_map.emplace(key, ptr); return true;}
			else if (!i->second.expired())             {return false;}
			else           {i->second = std::move(ptr); return true;}
		}


		// Visit each item in the table via a function taking a key and weak pointer.
		template<typename Callback,                   std::enable_if_t<std::is_invocable_v<Callback, const Key&, const std::weak_ptr<Value>&>, int> Dummy=0>
		void visit(const Callback &callback) const    noexcept(noexcept(std::declval<Callback>()(std::declval<Key>(),std::weak_ptr<Value>())))
		{
			shared_lock lock(_mtx);
			for (auto &pair : _map) callback(pair.first, pair.second);
		}

		// Visit each item in the table via a function taking a key and shared pointer.
		template<typename Callback,                   std::enable_if_t<std::is_invocable_v<Callback, const Key&, std::shared_ptr<Value>>, int> Dummy=0>
		void visit(const Callback &callback) const    noexcept(noexcept(std::declval<Callback>()(std::declval<Key>(),std::shared_ptr<Value>())))
		{
			shared_lock lock(_mtx);
			for (auto &pair : _map)
				if (auto p = pair.second.lock())
					callback(pair.first, std::move(p));
		}


	private:
		using _map_t = std::unordered_map<key_type, std::weak_ptr<value_type>, Hash, std::equal_to<>>;
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
