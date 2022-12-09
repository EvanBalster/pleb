#pragma once


/*
	This header defines methods for scanning PLEB's resource tree for
		services and subscribers, both existing and future, in order to
		maintain a comprehensive list.
		
	This is useful for implementing gateways and network communications.
*/

#include "resource.h"


namespace pleb
{
	namespace detail
	{
		template<class Callback, class ScanResult>
		subscription_ptr scan_subscribe(flags::filtering select, Callback callback, resource_ptr root, flags::handling handling)
		{
			return root->subscribe([callback = std::move(callback), select](const pleb::event &event)
			{
				if (event.filtering & select)
					if (auto *ptr = event.value_cast<ScanResult>())
						callback(*ptr);
			},
				flags::regular, handling);
		}
	}

	/*
		scan_services and scan_subscriptions invoke the provided functor once for
			each existing service/subscription and afterwards will be invoked
			whenever a new one is created until the scan subscription is released.
			These calls will arrive from whatever thread creates the service/subscription

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
	subscription_ptr scan_services(
		Callback        callback,
		resource_ptr    root     = resource::root(),
		flags::handling handling = flags::no_special_handling)
	{
		auto watch = detail::scan_subscribe<Callback, service_ptr>(flags::service_status, callback, root, handling);
		root->visit_services     ([&](service_ptr      svc) {callback(svc);});
		return watch;
	}

	template<class Callback,            std::enable_if_t<std::is_invocable_v<Callback, subscription_ptr>,int> Dummy = 0>
	subscription_ptr scan_subscriptions(
		Callback        callback,
		resource_ptr    root     = resource::root(),
		flags::handling handling = flags::no_special_handling)
	{
		auto watch = detail::scan_subscribe<Callback, subscription_ptr>(flags::subscription_status, callback, root, handling);
		root->visit_subscriptions([&](subscription_ptr sub) {callback(sub);});
		return watch;
	}
}
