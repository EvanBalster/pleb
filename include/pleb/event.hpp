#pragma once

#include "message.hpp"
#include "status.hpp"

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
	class event : public message
	{
	public:
		using content::value;
		using content::value_cast;
		using content::get;
		using content::get_mutable;
		

	public:
		template<typename T = std::any>
		event(
			const topic_path &topic,
			pleb::status      status,
			T               &&value     = {},
			flags::filtering  filtering = flags::default_message_filtering,
			flags::handling   handling  = flags::no_special_handling)
			:
			message(topic, code_t(status.code),
				std::forward<T>(value), filtering, handling)
			{}


		// Event status from <status.h>.  Stored in the code field.
		status status() const noexcept    {return pleb::status_enum(code);}


		// Publish this event.  This may be done repeatedly.
		void publish()                    {topic.publish(*this); features |= flags::did_send;}
	};


	/*
		Subscribers are implemented as a function taking an event.
	*/
	using subscriber_function = std::function<void(const event&)>;


	/*
		Class for a registered subscription function which can receive reports.
	*/
	class subscription : public receiver
	{
	public:
		const pleb::topic topic;


	private:
		template<class P> friend class topic_;
		const subscriber_function func;


	public:
		subscription(
			const pleb::topic    &_topic,
			subscriber_function &&_func,
			flags::filtering      ignored = flags::default_subscriber_ignore,
			flags::handling       handling = flags::no_special_handling)
			:
			receiver(ignored, handling), topic(_topic), func(std::move(_func))
		{
			topic.publish(statuses::Created, this,
				flags::subscription_status | flags::recursive);
		}
	};
}