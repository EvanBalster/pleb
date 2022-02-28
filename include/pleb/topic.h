#pragma once


#include "pleb_base.h"
#include "conversion.h"
#include "event.h"

#include "topic_base.h"


/*
	A minimal system for publish/subscribe with a global hierarchy of topics.
		Each topic may have any number of subscribers.

	These classes do not provide asynchronous execution, futures or serialization;
		those features can be implemented as part of the function wrapper.
		Subscribers throwing an exception will halt processing of a report.

	TEARDOWN SAFETY NOTICE:
		Calls may be in progress when the subscription is released.
		Some protection against this case is necessary in client code.
*/


namespace pleb
{
	/*
		Internal data of topic.
	*/



	/*
		Topics form a global hierarchy (trie) to which 
	*/
	class topic :
		protected coop::trie_<topic_data>
	{
	public:
		using subscription = pleb::subscription;


	public:
		// Access the root topic.  (TODO allocate statically?)
		static topic_ptr root() noexcept                   {static topic_ptr root = _asTopic(_trie::create()); return root;}

		// Access a topic by path.
		static topic_ptr find        (path_view path) noexcept    {return root()->subtopic(path);}
		static topic_ptr find_nearest(path_view path) noexcept    {return root()->nearest(path);}

		~topic() {}

		// Access the parent topic (root's parent is null)
		topic_ptr parent() noexcept                        {return _asTopic(_trie::parent());}

		// Access a subtopic of this topic.
		topic_ptr subtopic(path_view subtopic)             {return _asTopic(_trie::get    (subtopic));}
		topic_ptr nearest (path_view subtopic) noexcept    {return _asTopic(_trie::nearest(subtopic));}


		/*
			SUBSCRIBE to a topic.
				Subscribers receive subsequent reports to the topic
				and the children/descendants of the topic (not counting aliases).
		*/
		std::shared_ptr<subscription> subscribe(                   subscriber_function &&f)    {return this->emplace_subscriber(shared_from_this(), std::move(f));}
		std::shared_ptr<subscription> subscribe(path_view subpath, subscriber_function &&f)    {return subtopic(subpath)->subscribe(std::move(f));}


		/*
			PUBLISH a report.
				This will be visible to all subscribers to a topic,
				and the parents/ancestors of the topic.
		*/
		template<typename T>
		void publish(path_view subtopic, T &&item)    {this->nearest(subtopic)->publish(std::move(item));}

		template<typename T>
		void publish(T &&item)
		{
			event event = {*this, 0, std::move(item)};

			for (topic_ptr node = shared_from_this(); node; node = node->parent())
				for (subscription &sub : (_trie::coop_type&) *node)
					sub.func(event);
		}


		/*
			SERVE this topic.
				Subsequent requests to this topic will be passed to the function.
				If a service already exists here, this function will fail, returning null.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(                   service_function &&function) noexcept    {return _trie::try_emplace_service(shared_from_this(), std::move(function));}
		[[nodiscard]] std::shared_ptr<service> serve(path_view subpath, service_function &&function) noexcept    {return this->subtopic(subpath)->serve(std::move(function));}


		/*
			REQUEST something from this topic.
				A reply may be provided asynchronously by the registered service.
				If there is no service, pleb::errors::no_such_service is thrown.
		*/
		template<typename T> [[nodiscard]]
		std::future<reply> request(method method, T &&item)    {std::future<reply> r; pleb::request(*this, method, std::move(item), &r); return std::move(r);}

		[[nodiscard]]                   std::future<reply> request       ()         {return request(method::GET, std::any());}
		[[nodiscard]]                   std::future<reply> request_get   ()         {return request(method::GET, std::any());}
		template<class T> [[nodiscard]] std::future<reply> request_post  (T &&v)    {return request(method::POST, std::move(item));}
		template<class T> [[nodiscard]] std::future<reply> request_patch (T &&v)    {return request(method::PATCH, std::move(item));}
		[[nodiscard]]                   std::future<reply> request_delete()         {return request(method::DELETE, std::any());}

		/*
			REQUEST something from this topic.
				A reply will be provided synchronously by the registered service.
				This may require the current thread to block until the reply is available.
				If there is no service, pleb::errors::no_such_service is thrown.
		*/
		template<typename T> [[nodiscard]]
		reply sync_request(method method, T &&item)    {return request(method, std::move(item)).get();}

		[[nodiscard]]                   reply sync_get   ()         {return sync_request(method::GET, std::any());}
		template<class T> [[nodiscard]] reply sync_post  (T &&v)    {return sync_request(method::POST, std::move(item));}
		template<class T> [[nodiscard]] reply sync_patch (T &&v)    {return sync_request(method::PATCH, std::move(item));}
		[[nodiscard]]                   reply sync_delete()         {return sync_request(method::DELETE, std::any());}

		/*
			REQUEST of this topic.
				The request will be routed to the registered service.
				There is no mechanism for a direct reply (this may improve performance).
				If there is no service, pleb::errors::no_such_service is thrown.
		*/
		template<typename T>
		void push(method method, T &&item)    {pleb::request r(*this, method, item, nullptr);}

		template<class T> void push      (T &&v)    {push(method::POST, std::move(v));}
		template<class T> void push_post (T &&v)    {push(method::POST, std::move(v));}
		template<class T> void push_patch(T &&v)    {push(method::PATCH, std::move(v));}
		void                   push_delete()        {push(method::DELETE, std::any());}


		/*
			REQUEST using a prepared pleb::request object.
				pleb::request usually calls this method upon construction;
				it can be used to process the same request repeatedly.
		*/
		void process(pleb::request &request)
		{
			if (auto svc = _trie::service_lock())
			{
				svc->func(request);
				// TODO fill the reply if the service failed to do so
			}
			else throw errors::no_such_service("???");
		}


		/*
			Alias a direct child of this topic to another existing topic.
				The alias has the same lifetime as the original, and is
				interchangeable with it for both publishers and new subscribers.

			Aliasing does not change the parent of the destination topic!

			Fails, returning false, if the child ID is already in use.
		*/
		bool make_alias(std::string_view child_id, topic_ptr destination)    {_trie *t=destination.get(); return this->make_link(child_id, std::shared_ptr<_trie>(std::move(destination), t));}


		// Support shared_from_this
		topic_ptr                    shared_from_this()          {return topic_ptr                   (_trie::shared_from_this(), this);}
		std::shared_ptr<const topic> shared_from_this() const    {return std::shared_ptr<const topic>(_trie::shared_from_this(), this);}
		

	protected:
		using _trie = coop::trie_<topic_data>;

		topic(std::shared_ptr<topic> parent)                   : _trie(std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static topic_ptr _asTopic(std::shared_ptr<_trie> p)    {return std::shared_ptr<topic>(p, (topic*) &*p);}
	};
}

// Request constructor implementation
inline void pleb::request::process()    {topic.process(*this);}
