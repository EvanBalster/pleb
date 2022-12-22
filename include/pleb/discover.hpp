#pragma once


/*
	This header defines methods for scanning PLEB's resource tree for
		services and subscribers, both existing and future, in order to
		maintain a comprehensive list.
		
	This is useful for implementing gateways and network communications.
*/

#include "topic.hpp"
#include "topic_impl.hpp"


namespace pleb
{
	namespace detail
	{
		template<class Callback, class ScanResult>
		subscription_ptr discover_subscribe(flags::filtering select, Callback callback, topic root, flags::handling handling)
		{
			return root.subscribe([callback = std::move(callback), select](const pleb::event &event)
			{
				if (event.filtering & select)
					if (auto *ptr = event.value_cast<ScanResult>())
						callback(*ptr);
			},
				flags::regular, handling);
		}
	}

	/*
		These methods invoke the provided functor once for each existing service/subscription
			and afterwards will be invoked whenever a new one is created until the discovery
			subscription itself is released.
			Calls will arrive from various threads as these 

		callback -- copyable functor capable of receiving service_ptr or subscription_ptr.
		root     -- the root resource to monitor, defaulting to PLEB's global root.
		handling -- special properties of the callback (rarely used).

		return value: a subscription *to* new services or subscriptions. 
			Retain the returned subscription pointer to keep receiving notifications.

		WARNING: While this function is setting up, you may receive up to one redundant
			call for each service/subscriber if it is coming into existence concurrently.
			Callbacks should be robust against these redundant calls.

		WARNING: PLEB currently uses a locking resource tree, and locks resources in read mode
			for the initial scan here.  Attempts to modify the resource subtree being scanned
			may result in deadlock.  This restriction will be lifted at a later time.
	*/
	template<class Callback,            std::enable_if_t<std::is_invocable<Callback, service_ptr>::value,int> Dummy = 0>
	subscription_ptr discover_services(
		Callback        callback,
		topic           root     = topic::root(),
		flags::handling handling = flags::no_special_handling)
	{
		auto watch = detail::discover_subscribe<Callback, service_ptr>(flags::service_status, callback, root, handling);
		root->visit_services     ([&](service_ptr      svc) {callback(svc);});
		return watch;
	}

	template<class Callback,            std::enable_if_t<std::is_invocable_v<Callback, subscription_ptr>,int> Dummy = 0>
	subscription_ptr discover_subscriptions(
		Callback        callback,
		topic           root     = topic::root(),
		flags::handling handling = flags::no_special_handling)
	{
		auto watch = detail::discover_subscribe<Callback, subscription_ptr>(flags::subscription_status, callback, root, handling);
		root->visit_subscriptions([&](subscription_ptr sub) {callback(sub);});
		return watch;
	}
}
