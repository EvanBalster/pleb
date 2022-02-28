#pragma once


#include "pleb_base.h"
#include "conversion.h"

#include "request.h"
#include "event.h"


namespace coop {template<class T> class trie_;}

/*
	Base class for topic trie.
*/
namespace pleb
{
	class topic;


	/*
		
	*/
	class topic_data :
		public std::enable_shared_from_this<topic_data>
	{
	public:
		using service_slot = coop::unmanaged::slot<service>;
		using subscriber_list = coop::unmanaged::pool<subscription>;
		using subscriber_iterator = typename subscriber_list::iterator;


	public:
		// Try to emplace a service.  May fail if service already exists.
		[[nodiscard]] std::shared_ptr<service>
			try_emplace_service(topic_ptr &&p, service_function &&f)    {return _service.try_emplace(std::move(p), std::move(f));}

		// Access the service like a weak_ptr
		std::shared_ptr<service> service_lock() const noexcept    {return _service.lock();}
		long service_use_count()                const noexcept    {return _service.use_count();}
		bool service_expired()                  const noexcept    {return _service.expired();}


		// Emplace a subscriber.
		[[nodiscard]] std::shared_ptr<subscription>
			emplace_subscriber(topic_ptr &&p, subscriber_function &&f)    {return _subs.emplace(std::move(p), std::move(f));}

		// Iterate over subscribers.
		subscriber_iterator begin() const    {return _subs.begin();}
		subscriber_iterator end  () const    {return _subs.end  ();}


	private:
		// This class is intended for use only as a base class of topic.
		friend class topic;
		friend class coop::trie_<topic_data>;
		topic_data() {}


	private:
		coop::unmanaged::pool<subscription> _subs;
		service_slot                        _service;
	};
}

