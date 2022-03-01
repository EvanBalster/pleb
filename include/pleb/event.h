#pragma once


#include "pleb_base.h"
#include "conversion.h"
#include "status.h"


/*
	PLEB facilitates publishing events to topics,
		whereupon they are handled by subscribers to those topics.
		See resource.h for more functionality.
*/


namespace pleb
{
	/*
		Subscriptions are retained by a shared_ptr throughout their lifetimes.
	*/
	class subscription;
	using subscription_ptr = std::shared_ptr<subscription>;

	/*
		Reports are events or messages passed from publishers to subscribers.
	*/
	class event
	{
	public:
		const resource_ptr resource;
		const pleb::status status; // Conventionally set to an HTTP status code, or zero.
		std::any           value;

	public:
		// Access value as a specific type.
		template<typename T> const T *cast() const noexcept    {return std::any_cast<T>(&value);}
		template<typename T> T       *cast()       noexcept    {return std::any_cast<T>(&value);}

		// Access value, allowing it to be supplied by value or shared_ptr.
		template<typename T> const T *get() const noexcept    {return pleb::any_const_ptr<T>(value);}
	};

	using subscriber_function = std::function<void(const event&)>;


	/*
		Class for a registered subscription function which can receive reports.
	*/
	class subscription
	{
	public:
		subscription(std::shared_ptr<resource> _resource, subscriber_function &&_func)
			:
			resource(std::move(_resource)), func(std::move(_func)) {}

		const std::shared_ptr<resource> resource;
		const subscriber_function       func;
	};
}