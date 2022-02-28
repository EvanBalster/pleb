#pragma once


#include "pleb_base.h"
#include "conversion.h"


/*
	PLEB facilitates publishing events to topics,
		whereupon they are handled by subscribers to those topics.
		See topic.h for more functionality.
*/


namespace pleb
{
	/*
		Events are organized under a topic -- a slash-delimited string.
	*/
	class topic;
	class subscription;

	/*
		Topics and subscriptions are retained by shared pointers.
			Topic pointers can be cached for repeated publishing.
	*/
	using topic_ptr        = std::shared_ptr<topic>;
	using subscription_ptr = std::shared_ptr<subscription>;

	/*
		Reports are events or messages passed from publishers to subscribers.
	*/
	class event
	{
	public:
		topic    &topic;
		const int status; // Conventionally set to an HTTP status code, or zero.
		std::any  value;

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
		subscription(std::shared_ptr<topic> _topic, subscriber_function &&_func)
			:
			topic(std::move(_topic)), func(std::move(_func)) {}

		const std::shared_ptr<topic> topic;
		const subscriber_function    func;
	};
}