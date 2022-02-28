#pragma once


#include "resource.h"


namespace pleb
{
	// Subscribe to a topic, providing a handler function.
	[[nodiscard]] inline std::shared_ptr<subscription>
		subscribe(
			topic_view             topic,
			subscriber_function &&function) noexcept          {return pleb::resource::find(topic)->subscribe(std::move(function));}

	// Subscribe to a topic, providing a handler object and method.
	template<class T> [[nodiscard]] std::shared_ptr<subscription>
		subscribe(
			topic_view path,
			T        *handler_object,
			void (T::*handler_method)(const pleb::event&))    {return subscribe(path, std::bind(handler_method, handler_object, std::placeholders::_1));}


	// Publish a value to a topic.
	template<typename T>
	void publish(
			topic_view topic,
			T       &&item) noexcept                          {return pleb::resource::find_nearest(topic)->publish(std::move(item));}


	// Serve a resource.
	[[nodiscard]] inline std::shared_ptr<service>
		serve(
			topic_view          path,
			service_function &&function) noexcept             {return pleb::resource::find(path)->serve(std::move(function));}

	template<class T> [[nodiscard]] std::shared_ptr<service>
		serve(
			topic_view path,
			T        *observer_object,
			void (T::*observer_method)(request&))             {return serve(path, std::bind(observer_method, observer_object, std::placeholders::_1));}


	// Perform a manual request to the given path.
	template<typename T> [[nodiscard]]
		inline request::request(topic_view path, pleb::method method, T &&value, std::future<pleb::reply> *reply) :
		request(pleb::resource::find(path), method, std::move(value), reply) {}


	// Make a request with asynchronous reply.
	[[nodiscard]] inline            std::future<reply> request_get   (topic_view t)           {return resource::find(t)->request_get();}
	template<class T> [[nodiscard]] std::future<reply> request_post  (topic_view t, T &&v)    {return resource::find(t)->request_post(std::move(v));}
	template<class T> [[nodiscard]] std::future<reply> request_patch (topic_view t, T &&v)    {return resource::find(t)->request_patch(std::move(v));}
	[[nodiscard]] inline            std::future<reply> request_delete(topic_view t)           {return resource::find(t)->request_delete();}

	// Make a request with synchronous reply.
	[[nodiscard]] inline                         reply sync_get   (topic_view t)           {return resource::find(t)->sync_get();}
	template<class T> [[nodiscard]]              reply sync_post  (topic_view t, T &&v)    {return resource::find(t)->sync_post(std::move(v));}
	template<class T> [[nodiscard]]              reply sync_patch (topic_view t, T &&v)    {return resource::find(t)->sync_patch(std::move(v));}
	[[nodiscard]] inline                         reply sync_delete(topic_view t)           {return resource::find(t)->sync_delete();}

	// Make a request, declining any reply.  (this is a bit more lightweight.)
	template<class T>                             void push       (topic_view t, T &&v)    {return resource::find(t)->push_post(std::move(v));}
	template<class T>                             void push_post  (topic_view t, T &&v)    {return resource::find(t)->push_post(std::move(v));}
	template<class T>                             void push_patch (topic_view t, T &&v)    {return resource::find(t)->push_patch(std::move(v));}
	inline                                        void push_delete(topic_view t)           {return resource::find(t)->push_delete();}
}
