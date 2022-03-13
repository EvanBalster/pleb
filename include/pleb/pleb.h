#pragma once


#include "resource.h"


namespace pleb
{
	// Subscribe to a topic, providing a handler function.
	[[nodiscard]] inline std::shared_ptr<subscription>
		subscribe(
			target_resource       resource,
			subscriber_function &&function) noexcept                   {return resource->subscribe(std::move(function));}

	// Subscribe to a topic, providing a handler object and method.
	template<class T> [[nodiscard]] std::shared_ptr<subscription>
		subscribe(
			target_resource resource,
			T              *handler_object,
			void       (T::*handler_method)(const pleb::event&))       {return subscribe(std::move(resource), std::bind(handler_method, handler_object, std::placeholders::_1));}


	// Publish a value to a topic.
	template<typename T>
	void publish(
			target_nearest_resource resource,
			T                     &&item,
			status                  status = statuses::OK) noexcept    {return resource->publish(std::forward<T>(item), status);}

	// Publish only a status to a topic.
	inline void publish(
		target_nearest_resource resource,
		status                  status) noexcept                       {return resource->publish(status);}
	

	// Serve a resource.
	[[nodiscard]] inline std::shared_ptr<service>
		serve(
			target_resource    resource,
			service_function &&function) noexcept                      {return resource->serve(std::move(function));}

	template<class T> [[nodiscard]] std::shared_ptr<service>
		serve(
			target_resource resource,
			T              *service_object,
			void       (T::*service_method)(request&))                 {return serve(std::move(resource), std::bind(service_method, service_object, std::placeholders::_1));}


	// Make a request with asynchronous reply.
	[[nodiscard]] inline            std::future<reply> request_get   (target_resource t)           {return t->request_get();}
	template<class T> [[nodiscard]] std::future<reply> request_put   (target_resource t, T &&v)    {return t->request_put(std::forward<T>(v));}
	template<class T> [[nodiscard]] std::future<reply> request_post  (target_resource t, T &&v)    {return t->request_post(std::forward<T>(v));}
	template<class T> [[nodiscard]] std::future<reply> request_patch (target_resource t, T &&v)    {return t->request_patch(std::forward<T>(v));}
	[[nodiscard]] inline            std::future<reply> request_delete(target_resource t)           {return t->request_delete();}

	// Make a request with synchronous reply.
	[[nodiscard]] inline                         reply sync_get   (target_resource t)           {return t->sync_get();}
	template<class T> [[nodiscard]]              reply sync_put   (target_resource t, T &&v)    {return t->sync_put(std::forward<T>(v));}
	template<class T> [[nodiscard]]              reply sync_post  (target_resource t, T &&v)    {return t->sync_post(std::forward<T>(v));}
	template<class T> [[nodiscard]]              reply sync_patch (target_resource t, T &&v)    {return t->sync_patch(std::forward<T>(v));}
	[[nodiscard]] inline                         reply sync_delete(target_resource t)           {return t->sync_delete();}

	// Make a request, declining any reply.  (this is a bit more lightweight.)
	template<class T>                             void push/*PUT*/(target_resource t, T &&v)    {return t->push   (std::forward<T>(v));}
	template<class T>                             void push_put   (target_resource t, T &&v)    {return t->push_put(std::forward<T>(v));}
	template<class T>                             void push_post  (target_resource t, T &&v)    {return t->push_post(std::forward<T>(v));}
	template<class T>                             void push_patch (target_resource t, T &&v)    {return t->push_patch(std::forward<T>(v));}
	inline                                        void push_delete(target_resource t)           {return t->push_delete();}
}
