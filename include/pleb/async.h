#pragma once


namespace pleb
{
	/*

	*/
	enum class timescale
	{
		none      = -128,

		// <100 ns
		nano      = -9,

		// 100 ns - 100 us
		micro     = -6,

		// 100 us - 100 ms
		milli     = -3,

		// 100 ms and up
		macro     =  0,

		unbounded = 127
	};

	struct caller_config
	{
		timescale sync_limit = timescale::none;
	};

	template<typename Parameter>
	class callee
	{
		using function_type = std::function<void(Parameter)>;

		function_type function;
		timescale     timescale = timescale::milli;

		std::future<void> operator()(Parameter &&parameter, const caller_config &config) const
		{
			//if (config.sync_limit < timescale)
			return std::async(std::launch::async, function, std::move<Parameter>(parameter));
		}
	};



	/*
		Simple interface to publish / subscribe
	*/
	template<typename T> using subscription = topic<T>::subscription;

	template<typename T>
	static std::shared_ptr<subscription<T>> subscribe(
		std::string_view         topic,
		std::function<void(T)> &&function,
		timescale                defer = timescale::automatic) noexcept
	{
		return pleb::topic<T>::root().subscribe(topic, std::move(function), defer);
	}

	template<typename T>
	static void publish(
		std::string_view topic,
		T              &&item,
		timescale        defer = timescale::automatic) noexcept
	{
		return pleb::topic<T>::root().publish(topic, std::move(item), defer);
	}


	template<typename T> using service = resource<T>::service;

	template<typename T>
	static std::shared_ptr<service<T>> serve(
		std::string_view           path,
		std::function<void(T&&)> &&function,
		deferment                  defer = deferment::automatic) noexcept
	{
		return pleb::resource<T>::root().subscribe(path, std::move(function), defer);
	}

	template<typename T>
	static void request(
		std::string_view path,
		T              &&item,
		deferment        defer = deferment::automatic) noexcept
	{
		return pleb::resource<T>::root().request(topic, std::move(item), defer);
	}
}
