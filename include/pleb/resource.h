#pragma once


#include "coop_trie.h"

#include "request.h"
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
		Subscribers throwing an exception will halt processing of an event.

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

		resource(resource_ptr parent, std::string_view id)       : _trie(id, std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static resource_ptr _asTopic(std::shared_ptr<_trie> p)   {return resource_ptr(p, (resource*) &*p);}


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
		resource_ptr parent() const noexcept                   {return _asTopic(_trie::parent());}

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
		void publish(
			status           status    = statuses::OK,
			T              &&item      = {},
			flags::filtering filtering = flags::default_message_filtering,
			flags::handling  handling  = flags::no_special_handling)
		{
			pleb::event e(shared_from_this(), status, std::forward<T>(item), filtering, handling);
			publish(e);
		}

		template<typename T = std_any::any>
		void publish(
			topic_view       subtopic,
			status           status    = statuses::OK,
			T              &&item      = {},
			flags::filtering filtering = flags::default_message_filtering,
			flags::handling  handling  = flags::no_special_handling)
		{
			this->nearest(subtopic)->publish(status, std::forward<T>(item), filtering, handling);
		}


		/*
			SERVE this resource.
				Subsequent events on this resource will be passed to the function.
				If a service already exists here, this function will fail, returning null.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(                    service_function &&function) noexcept    {return _trie::try_emplace_service(shared_from_this(), std::move(function));}
		[[nodiscard]] std::shared_ptr<service> serve(topic_view subpath, service_function &&function) noexcept    {return this->subtopic(subpath)->serve(std::move(function));}


		/*
			REQUEST something from this resource, providing a client for responding.
				client_ref may be a client_ptr or a std::future.
				The response target will receive a response, now or later.
				If there is no service, pleb::service_not_found is thrown.
		*/
		template<class V = std::any>
		void request(client_ref client, method method, V &&value = {})
		{
			pleb::request r(std::move(client), shared_from_this(), method, std::forward<V>(value));
			issue(r);
		}

		/* */                        void GET   (client_ref c)                    {return request(c, method::GET);}
		template<class V = std::any> void PUT   (client_ref c, V &&value)         {return request(c, method::PUT,    std::forward<V>(value));}
		template<class V = std::any> void POST  (client_ref c, V &&value = {})    {return request(c, method::POST,   std::forward<V>(value));}
		template<class V = std::any> void PATCH (client_ref c, V &&value)         {return request(c, method::PATCH,  std::forward<V>(value));}
		/* */                        void DELETE(client_ref c)                    {return request(c, method::DELETE);}

		/*
			Convenience API for requests.

			The returned request may be issued (sent) by:
			- calling async<T> or converting to std::future<T>
			- calling await<T>
			- calling push() or issue(client) -- auto_retrieve only.
		*/
		template<class V = std::any>
		auto_request request(method method, V &&value = {})
		{
			return auto_request(shared_from_this(), method, std::forward<V>(value));
		}
		template<class V = std::any>
		auto_retrieve retrieve(method method, V &&value = {})
		{
			return auto_retrieve(shared_from_this(), method, std::forward<V>(value));
		}

		/* */                        auto_retrieve GET  ()                  {return retrieve(method::GET);}
		template<class V = std::any> auto_request PUT   (V &&value)         {return request(method::PUT,   std::forward<V>(value));}
		template<class V = std::any> auto_request POST  (V &&value = {})    {return request(method::POST,  std::forward<V>(value));}
		template<class V = std::any> auto_request PATCH (V &&value)         {return request(method::PATCH, std::forward<V>(value));}
		/* */                        auto_request DELETE()                  {return request(method::DELETE);}

#if 0
		/*
			PUSH a request to a resource.
				There is no mechanism for a direct reply (this may improve performance).
				If there is no service, pleb::service_not_found is thrown.
		*/
		template<class V = std::any>
		void push(method method, V &&value = {})
		{
			request(nullptr, method, std::forward<V>(value));
		}

		template<class V = std::any> void push_PUT   (V &&value)         {push(method::PUT,   std::forward<V>(value));}
		template<class V = std::any> void push_POST  (V &&value = {})    {push(method::POST,  std::forward<V>(value));}
		template<class V = std::any> void push_PATCH (V &&value)         {push(method::PATCH, std::forward<V>(value));}
		/* */                        void push_DELETE()                  {push(method::DELETE);}
#endif


		/*
			REQUEST using a prepared pleb::request object.
				pleb::request usually calls this method upon construction;
				it can be used to issue the same request repeatedly.
		*/
		void issue(pleb::request &msg) const
		{
			msg.features &= ~flags::did_respond;

			auto svc = _trie::service_lock();
			if (svc)
				if (svc->accepts(msg.filtering & ~flags::recursive))
					goto service_found;
			
			if (msg.recursive()) // Propagate up the chain.
				for (resource_ptr node = parent(); node; node = node->parent())
					if (svc = _trie::service_lock())
						if (svc->accepts(msg.filtering | flags::recursive))
							goto service_found;

		//service_not_found:
			throw service_not_found("No service available", topic());
			return;

		service_found:
			svc->func(msg);

			// Default response if service did not respond or move message.
			if (!(msg.features & flags::did_respond))
				msg.respond(statuses::NoContent);
		}

		/*
			PUBLISH using a prepared pleb::event object.
				pleb::event usually calls this method upon construction;
				it can be used to publish the same event repeatedly.
		*/
		void publish(const pleb::event &msg) const
		{
			// Publish to this resource's direct subscribers.
			for (subscription &sub : *this)
				if (sub.accepts(msg.filtering & ~flags::recursive))
					sub.func(msg);

			if (msg.recursive()) // Propagate up the chain.
				for (resource_ptr node = parent(); node; node = node->parent())
					for (subscription &sub : *node)
						if (sub.accepts(msg.filtering | flags::recursive))
							sub.func(msg);
		}


		/*
			Alias a direct child of this resource to another existing resource.
				The alias has the same lifetime as the original, and is
				interchangeable with it for both publishers and new subscribers.

			Aliasing does not change the parent of the destination resource!

			The alias is broken if the pointer to destination is allowed to expire.

			Fails, returning false, if the child ID is already in use.
		*/
		resource_ptr make_alias(std::string_view child_id, resource_ref destination)
		{
			if (destination && this->make_link(child_id, std::shared_ptr<_trie>(destination, (_trie*) destination.get())))
				return std::move(destination);
			return nullptr;
		}


		// Support shared_from_this
		resource_ptr                    shared_from_this()          {return resource_ptr                (_trie::shared_from_this(), this);}
		std::shared_ptr<const resource> shared_from_this() const    {return std::shared_ptr<const resource>(_trie::shared_from_this(), this);}
	};
}

/*
	==============
	IMPLEMENTATION
	==============
*/

inline pleb::resource_ptr pleb::operator/(const resource_ptr &ptr, topic_view subtopic)    {return ptr->subtopic(subtopic);}

inline pleb::resource_ptr pleb::find_nearest_resource(pleb::topic_view topic)    {return pleb::resource::find_nearest(topic);}
inline pleb::resource_ptr pleb::find_resource        (pleb::topic_view topic)    {return pleb::resource::find(topic);}

// Process a request.
inline void pleb::request::issue  ()    {resource->issue  (*this); features = features|flags::did_send;}
inline void pleb::event  ::publish()    {resource->publish(*this); features = features|flags::did_send;}

inline pleb::service::service(resource_ptr _resource, service_function &&_func)
	:
	receiver(flags::default_service_ignore), resource(std::move(_resource)),
	func(std::move(_func))
{
	resource->publish(statuses::Created, this,
		flags::service_status      | flags::recursive);
}

inline pleb::subscription::subscription(resource_ptr _resource, subscriber_function &&_func)
	:
	receiver(flags::default_subscriber_ignore), resource(std::move(_resource)),
	func(std::move(_func))
{
	resource->publish(statuses::Created, this,
		flags::subscription_status | flags::recursive);
}

inline pleb::service     ::~service()         {resource->publish(statuses::Gone, this, flags::service_status      | flags::recursive);}
inline pleb::subscription::~subscription()    {resource->publish(statuses::Gone, this, flags::subscription_status | flags::recursive);}
