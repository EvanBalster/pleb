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
		// Access value as a specific type.  Only succeeds if the type is an exact match.
		template<class T> const T *value_cast() const noexcept    {return std::any_cast<T>(&value);}
		template<class T> T       *value_cast()       noexcept    {return std::any_cast<T>(&value);}

		// Get a constant pointer to the value.
		//  This method automatically deals with indirect values.
		template<class T> const T *get() const noexcept    {return pleb::any_const_ptr<T>(value);}

		// Access a mutable pointer to the value.
		//  This method automatically deals with indirect values.
		//  This will fail when a const event holds a value directly.
		template<class T> T       *get_mutable() const noexcept    {return pleb::any_ptr<T>(value);}
		template<class T> T       *get_mutable()       noexcept    {return pleb::any_ptr<T>(value);}
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