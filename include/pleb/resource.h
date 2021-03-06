#pragma once


#include "pleb_base.h"
#include "conversion.h"
#include "event.h"

#include "resource_base.h"


/*
	A minimal system for pub-sub and request-reply messaging patterns,
		with native calling and native object passing.
	
	A resource supports:
		One service (or none)
		Any number of subscribers
		Any number of child resources (organized like filesystem directories)

	These classes do not provide asynchronous execution or serialization;
		those features can be implemented as part of the function wrapper.
		Subscribers throwing an exception will halt processing of a report.

	TEARDOWN SAFETY NOTICE:
		Calls may be in progress when the subscription is released.
		Some protection against this case is necessary in client code.
*/


namespace pleb
{
	/*
		Topics form a global hierarchy (trie) to which 
	*/
	class resource :
		protected coop::trie_<resource_data>
	{
	protected:
		using _trie = coop::trie_<resource_data>;

		resource(resource_ptr parent, std::string_view id)              : _trie(id, std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static resource_ptr _asTopic(std::shared_ptr<_trie> p)          {return std::shared_ptr<resource>(p, (resource*) &*p);}


	public:
		using subscription = pleb::subscription;
		using service      = pleb::service;


	public:
		// Access the root resource.  (TODO allocate statically?)
		static resource_ptr root() noexcept                   {static resource_ptr root = _asTopic(_trie::create("[root]")); return root;}

		// Access a resource by its global topic name.
		static resource_ptr find        (topic_view path) noexcept    {return root()->subtopic(path);}
		static resource_ptr find_nearest(topic_view path) noexcept    {return root()->nearest(path);}

		~resource() {}

		// Access the parent resource (root's parent is null)
		resource_ptr parent() noexcept                        {return _asTopic(_trie::parent());}

		// Access a subtopic of this resource.
		resource_ptr subtopic(topic_view subtopic)             {return _asTopic(_trie::get    (subtopic));}
		resource_ptr nearest (topic_view subtopic) noexcept    {return _asTopic(_trie::nearest(subtopic));}


		// Get this resource's ID or full topic name.
		std::string_view id   () const noexcept    {return _trie::id();}
		std::string      topic() const noexcept    {return _trie::path();}


		/*
			SUBSCRIBE to a resource.
				Subscribers receive subsequent reports to the resource
				and the children/descendants of the resource (not counting aliases).
		*/
		std::shared_ptr<subscription> subscribe(                    subscriber_function &&f)    {return this->emplace_subscriber(shared_from_this(), std::move(f));}
		std::shared_ptr<subscription> subscribe(topic_view subpath, subscriber_function &&f)    {return subtopic(subpath)->subscribe(std::move(f));}


		/*
			PUBLISH an event.
				This will be visible to all subscribers to a resource,
				and the parents/ancestors of the resource.
		*/
		template<typename T = std_any::any>
		void publish(topic_view subtopic, status status = statuses::OK, T &&item = {})    {this->nearest(subtopic)->publish(status, std::forward<T>(item));}

		template<typename T = std_any::any>
		void publish(                     status status = statuses::OK, T &&item = {})
		{
			event event = {shared_from_this(), status, std::forward<T>(item)};

			for (resource_ptr node = shared_from_this(); node; node = node->parent())
				for (subscription &sub : (_trie::coop_type&) *node)
					sub.func(event);
		}


		/*
			SERVE this resource.
				Subsequent events on this resource will be passed to the function.
				If a service already exists here, this function will fail, returning null.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(                    service_function &&function) noexcept    {return _trie::try_emplace_service(shared_from_this(), std::move(function));}
		[[nodiscard]] std::shared_ptr<service> serve(topic_view subpath, service_function &&function) noexcept    {return this->subtopic(subpath)->serve(std::move(function));}


		/*
			REQUEST something from this resource.
				A reply may be provided asynchronously by the registered service.
				If there is no service, pleb::errors::no_such_service is thrown.
		*/
		template<typename T = std_any::any> [[nodiscard]]
		std::future<reply> request(method method, T &&item = {})    {std::future<reply> r; pleb::request(shared_from_this(), method, std::forward<T>(item), &r); return std::move(r);}

		[[nodiscard]]                   std::future<reply> request       ()         {return request(method::GET);}
		[[nodiscard]]                   std::future<reply> request_get   ()         {return request(method::GET);}
		template<class T> [[nodiscard]] std::future<reply> request_put   (T &&v)    {return request(method::PUT, std::forward<T>(v));}
		template<class T> [[nodiscard]] std::future<reply> request_post  (T &&v)    {return request(method::POST, std::forward<T>(v));}
		template<class T> [[nodiscard]] std::future<reply> request_patch (T &&v)    {return request(method::PATCH, std::forward<T>(v));}
		[[nodiscard]]                   std::future<reply> request_delete()         {return request(method::DELETE);}

		/*
			REQUEST something from this resource.
				A reply will be provided synchronously by the registered service.
				This may require the current thread to block until the reply is available.
				If there is no service, pleb::errors::no_such_service is thrown.
		*/
		template<typename T = std_any::any> [[nodiscard]]
		reply sync_request(method method, T &&item = {})    {return request(method, std::move(item)).get();}

		[[nodiscard]]                   reply sync_get   ()         {return sync_request(method::GET);}
		template<class T> [[nodiscard]] reply sync_put   (T &&v)    {return sync_request(method::PUT, std::forward<T>(v));}
		template<class T> [[nodiscard]] reply sync_post  (T &&v)    {return sync_request(method::POST, std::forward<T>(v));}
		template<class T> [[nodiscard]] reply sync_patch (T &&v)    {return sync_request(method::PATCH, std::forward<T>(v));}
		[[nodiscard]]                   reply sync_delete()         {return sync_request(method::DELETE);}

		/*
			PUSH data or commands to a resource.
				The request will be routed to the registered service.
				There is no mechanism for a direct reply (this may improve performance).
				If there is no service, pleb::errors::no_such_service is thrown.
		*/
		template<typename T = std_any::any>
		void push(method method, T &&item = {})    {pleb::request r(shared_from_this(), method, item, nullptr);}

		template<class T> void push/*PUT*/(T &&v)    {push(method::PUT, std::forward<T>(v));}
		template<class T> void push_put   (T &&v)    {push(method::PUT, std::forward<T>(v));}
		template<class T> void push_post  (T &&v)    {push(method::POST, std::forward<T>(v));}
		template<class T> void push_patch (T &&v)    {push(method::PATCH, std::forward<T>(v));}
		void                   push_delete()         {push(method::DELETE);}


		/*
			REQUEST using a prepared pleb::request object.
				pleb::request usually calls this method upon construction;
				it can be used to p rocess the same request repeatedly.
		*/
		void process(pleb::request &request)
		{
			if (auto svc = _trie::service_lock())
			{
				svc->func(request);
				// TODO fill the reply if the service failed to do so
			}
			else throw errors::no_such_service(topic());
		}


		/*
			Alias a direct child of this resource to another existing resource.
				The alias has the same lifetime as the original, and is
				interchangeable with it for both publishers and new subscribers.

			Aliasing does not change the parent of the destination resource!

			The alias is broken if the pointer to destination is allowed to expire.

			Fails, returning false, if the child ID is already in use.
		*/
		resource_ptr make_alias(std::string_view child_id, target_resource destination)
		{
			if (destination.resource && this->make_link(child_id, std::shared_ptr<_trie>(destination.resource, (_trie*) destination.resource.get())))
				return std::move(destination.resource);
			return nullptr;
		}


		// Support shared_from_this
		resource_ptr                    shared_from_this()          {return resource_ptr                (_trie::shared_from_this(), this);}
		std::shared_ptr<const resource> shared_from_this() const    {return std::shared_ptr<const resource>(_trie::shared_from_this(), this);}
	};
}

inline pleb::resource_ptr pleb::operator/(const resource_ptr &ptr, topic_view subtopic)    {return ptr->subtopic(subtopic);}

inline pleb::resource_ptr pleb::find_nearest_resource(pleb::topic_view topic)    {return pleb::resource::find_nearest(topic);}
inline pleb::resource_ptr pleb::find_resource        (pleb::topic_view topic)    {return pleb::resource::find(topic);}

// Perform a manual request to the given path.
template<typename T>
inline pleb::request::request(topic_view path, pleb::method method, T &&value, std::future<pleb::reply> *reply, bool process_now) :
	request(pleb::resource::find(path), method, std::move(value), reply, process_now) {}

// Process a request.
inline void pleb::request::process()    {resource->process(*this);}
