#pragma once


#include "resource.h"


namespace pleb
{
	/*
		Publish-Subscribe convenience functions.
	*/

	// Subscribe to a topic, providing a handler function.
	[[nodiscard]] inline
	std::shared_ptr<subscription> subscribe(
			resource_ref          resource,
			subscriber_function &&function) noexcept                   {return resource->subscribe(std::move(function));}

	// Subscribe to a topic, providing a lockable handler pointer and method.
	template<class T> [[nodiscard]]
	std::shared_ptr<subscription> subscribe(
			resource_ref     resource,
			std::weak_ptr<T> handler_object,
			void        (T::*handler_method)(const pleb::event&))       {return subscribe(std::move(resource), [m=handler_method, w=std::move(handler_object)](const event &r) {if (auto s=w.lock()) (s.get()->*m)(r);});}


	// Publish a value to a topic.
	template<typename T = std_any::any>
	void publish(
			nearest_resource_ref destination,
			status               status = statuses::OK,
			T                  &&value  = {}) noexcept              {return destination->publish(status, std::forward<T>(value));}
	

	/*
		Request-Response convenience functions.
	*/

	// Serve a resource.
	[[nodiscard]] inline
	std::shared_ptr<service> serve(
			resource_ref       resource,
			service_function &&function) noexcept                      {return resource->serve(std::move(function));}

	// Serve a resource, providing a lockable service pointer and method.
	template<class T> [[nodiscard]]
	std::shared_ptr<service> serve(
			resource_ref     resource,
			std::weak_ptr<T> service_object,
			void        (T::*service_method)(request&))                {return serve(std::move(resource), [m=service_method, w=std::move(service_object)](request &r) {if (auto s=w.lock()) (s.get()->*m)(r);});}


	// Make a request, providing a client or future which receives the response.
	inline                       void GET   (client_ref c, resource_ref t)                    {t->GET   (c);}
	template<class V = std::any> void PUT   (client_ref c, resource_ref t, V &&value)         {t->PUT   (c, std::forward<V>(value));}
	template<class V = std::any> void POST  (client_ref c, resource_ref t, V &&value = {})    {t->POST  (c, std::forward<V>(value));}
	template<class V = std::any> void PATCH (client_ref c, resource_ref t, V &&value)         {t->PATCH (c, std::forward<V>(value));}
	inline                       void DELETE(client_ref c, resource_ref t)                    {t->DELETE(c);}

	// Make a request, returning the response.  May block until a response is generated.
	inline                       auto_retrieve GET  (resource_ref t)               {return t->GET   ();}
	template<class V = std::any> auto_request PUT   (resource_ref t, V &&value)    {return t->PUT   (std::forward<V>(value));}
	template<class V = std::any> auto_request POST  (resource_ref t, V &&value)    {return t->POST  (std::forward<V>(value));}
	template<class V = std::any> auto_request PATCH (resource_ref t, V &&value)    {return t->PATCH (std::forward<V>(value));}
	inline                       auto_request DELETE(resource_ref t)               {return t->DELETE();}

#if 0
	// Make a request, with no means for a response.  This can improve performance.
	template<class V = std::any> void push_PUT   (resource_ref t, V &&value)    {t->push_PUT   (std::forward<V>(value));}
	template<class V = std::any> void push_POST  (resource_ref t, V &&value)    {t->push_POST  (std::forward<V>(value));}
	template<class V = std::any> void push_PATCH (resource_ref t, V &&value)    {t->push_PATCH (std::forward<V>(value));}
	inline                       void push_DELETE(resource_ref t)               {t->push_DELETE();}
#endif
}
