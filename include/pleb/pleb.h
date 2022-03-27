#pragma once


#include "resource.h"


namespace pleb
{
	// Subscribe to a topic, providing a handler function.
	[[nodiscard]] inline std::shared_ptr<subscription>
		subscribe(
			target_resource       resource,
			subscriber_function &&function) noexcept                   {return resource->subscribe(std::move(function));}

	// Subscribe to a topic, providing a lockable handler pointer and method.
	template<class T> [[nodiscard]] std::shared_ptr<subscription>
		subscribe(
			target_resource  resource,
			std::weak_ptr<T> handler_object,
			void        (T::*handler_method)(const pleb::event&))       {return subscribe(std::move(resource), [m=handler_method, w=std::move(handler_object)](const event &r) {if (auto s=w.lock()) (s->*m)(r);});}


	// Publish a value to a topic.
	template<typename T = std_any::any>
	void publish(
			target_nearest_resource resource,
			status                  status = statuses::OK,
			T                     &&value  = {}) noexcept              {return resource->publish(status, std::forward<T>(value));}
	



	// Serve a resource.
	[[nodiscard]] inline std::shared_ptr<service>
		serve(
			target_resource    resource,
			service_function &&function) noexcept                      {return resource->serve(std::move(function));}

	// Serve a resource, providing a lockable service pointer and method.
	template<class T> [[nodiscard]] std::shared_ptr<service>
		serve(
			target_resource  resource,
			std::weak_ptr<T> service_object,
			void        (T::*service_method)(request&))                 {return serve(std::move(resource), [m=service_method, w=std::move(service_object)](request &r) {if (auto s=w.lock()) (s->*m)(r);});}


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
