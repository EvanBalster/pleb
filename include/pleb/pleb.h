#pragma once


#include "resource.h"


namespace pleb
{
#define PLEB_RESOURCE_VERB(METHOD_NAME) \
	template<typename... Args> \
	auto METHOD_NAME(resource_ref resource, Args&& ... args) {\
		return resource->METHOD_NAME(std::forward<Args>(args)...);}

	PLEB_RESOURCE_VERB(subscribe)
	PLEB_RESOURCE_VERB(publish)

	PLEB_RESOURCE_VERB(GET)
	PLEB_RESOURCE_VERB(PUT)
	PLEB_RESOURCE_VERB(POST)
	PLEB_RESOURCE_VERB(PATCH)
	PLEB_RESOURCE_VERB(DELETE)

	PLEB_RESOURCE_VERB(serve)

#undef PLEB_RESOURCE_VERB


	/*
		Access the table of general type conversion rules.
	*/
	inline const std::shared_ptr<conversion_table> &
		conversion_rules()                                                                             {return detail::global_conversion_table();}

	template<typename ConversionFunctor, typename Input = detail::detect_parameter_t<ConversionFunctor>>
	conversion_table::rule_ptr conversion_define(ConversionFunctor &&func)                             {return conversion_rules()->set(std::forward<ConversionFunctor>(func));}

	/*
		Perform a type conversion using the table of general conversion rules.
			convert(...) throws no_conversion_rule if no rule is defined.
			try_convert(...) returns a default value if no rule is defined.
	*/
	std::any convert(const std::any &x, std::type_index to_type)                                       {return conversion_rules()->convert(x,to_type);}
	template<typename To, typename From>
	To       convert(const From     &x)                                                                {return conversion_rules()->convert<To>(x);}

	std::any try_convert(const std::any &x, std::type_index to_type, const std::any &on_error = {})    {return conversion_rules()->try_convert(x, to_type, on_error);}
	template<typename To, typename From>
	To       try_convert(const From     &x, const To &on_error = {})                                   {return conversion_rules()->try_convert(x, on_error);}






#if 0
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

	/*
		Serve a resource with a request handler function.
	*/
	[[nodiscard]] inline
	std::shared_ptr<service> serve(
			resource_ref       resource,
			service_function &&function) noexcept                      {return resource->serve(std::move(function));}

	/*
		Serve a resource with a request handler method.
			The weak pointer is locked whenever the service is called.
			If the service pointer outlives the object pointer, it will respond with "GONE".
	*/
	template<class T> [[nodiscard]]
	std::shared_ptr<service> serve(
			resource_ref     resource,
			std::weak_ptr<T> svc_object,
			void        (T::*svc_method)(request&))
	{
		return serve(std::move(resource), [m=svc_method, w=std::move(svc_object)](request &r)
		{
			if (auto s=w.lock()) (s.get()->*m)(r);
			else r.respond_Gone();
		});
	}
#endif
}
