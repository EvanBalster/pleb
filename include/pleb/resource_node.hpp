#pragma once

#include <deque>

#include "coop/pool.hpp"
#include "coop/trie.hpp"
#include "request.hpp"
#include "event.hpp"


namespace coop {template<class T> class trie_;}

/*
	Base class for topic trie.
*/

namespace pleb
{
	/*
		Represents the internal, protected data of a resource.
	*/
	class resource_data :
		public std::enable_shared_from_this<resource_data>
	{
	public:
		using service_slot = coop::unmanaged::slot<service>;

		using subscriber_list = coop::unmanaged::pool<subscription>;
		using subscriber_iterator = typename subscriber_list::iterator;


	public:
		// Try to emplace a service.  May fail if service already exists.
		[[nodiscard]] std::shared_ptr<service>
			try_emplace_service(
				const resource_node_ptr &p,
				service_function       &&f)    {return _service.try_emplace(p, std::move(f));}

		// Access the service like a weak_ptr
		std::shared_ptr<service> service_lock() const noexcept    {return _service.lock();}
		long service_use_count()                const noexcept    {return _service.use_count();}
		bool service_expired()                  const noexcept    {return _service.expired();}


		// Emplace a subscriber.
		[[nodiscard]] std::shared_ptr<subscription>
			emplace_subscriber(
				const resource_node_ptr &p,
				subscriber_function    &&f)    {return _subs.emplace(p, std::move(f));}

		// Iterate over subscribers.
		const subscriber_list &subscribers() const    {return _subs;}


	private:
		// This class is intended for use only as a base class of topic.
		friend class topic;
		friend class coop::trie_<resource_data>;
		resource_data() {}


	private:
		subscriber_list _subs;
		service_slot    _service;
	};
}

