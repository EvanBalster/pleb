#pragma once

#include "topic.hpp"
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
			T               &&value  = {},
			message_flags     flags  = {})
			:
			message(topic, code_t(status.code), std::forward<T>(value), flags) {}


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
		// Note this class will normally only be created by topic::subscribe() and co.
		subscription(
			const pleb::topic    &_topic,
			subscriber_function &&_func,
			service_config        flags = {})
			:
			receiver(flags), topic(_topic), func(std::move(_func)) {}
	};



	/*
		An event relay is a special subscription that republishes messages.
			It has no additional functionality at the moment.
	*/
	class event_relay : public subscription
	{
	protected:
		using subscription::subscription;
	};


	/*
		Implementation of methods from the topic class.
	*/
	template<typename SubPath>
	std::shared_ptr<event_relay> topic_<SubPath>::forward_events(
		topic_path          destination_topic,
		subscription_config flags)
	{
		if (!(flags.filtering & pleb::flags::recursive) && is_ancestor_of(destination_topic))
			throw std::logic_error("Forwarding events to a child topic would cause a stack overflow.");

		return std::static_pointer_cast<event_relay>(
			subscribe([destination_topic = std::move(destination_topic)](const pleb::event &event)
		{
			// Generate a copy of the event...
			destination_topic.publish(
				event.status(),
				event.value(),
				event.filtering | event.requirements);
		}, flags));
	}
}