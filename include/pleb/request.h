#pragma once


#include <future>
#include <exception>
#include "pleb_base.h"
#include "method.h"
#include "status.h"


/*
	PLEB delivers requests to services, which may then reply
		according to the method of requesting.
		See resource.h for more functionality.
*/


namespace pleb
{
	namespace errors
	{
		class no_such_service : public std::runtime_error
		{
		public:
			no_such_service(const std::string& topic)    : runtime_error("no such service: " + topic) {}
			no_such_service(topic_view topic)            : no_such_service(topic.string) {}
			no_such_service(std::string_view topic)      : no_such_service(std::string(topic)) {}
			no_such_service(const char* topic)           : no_such_service(std::string(topic)) {}

		private:
			std::string _str;
		};
	}

	/*
		Services are retained by a shared_ptr throughout their lifetimes.
	*/
	class service;
	using service_ptr  = std::shared_ptr<service>;

	/*
		Replies may be produced in response to a request (see below)
	*/
	class reply
	{
	public:
		status   status;
		std::any value;
	};


	class request
	{
	public:
		const resource_ptr resource;
		const method       method;
		std::any           value;
		
	private:
		using promise_t = std::promise<pleb::reply>;
		using future_t = std::future<pleb::reply>;
		std::future<pleb::reply> *_reply;

	public:
		// Compose a request manually.
		template<typename T>
		request(
			resource_ptr             _resource,
			pleb::method             _method,
			T                      &&_value,
			std::future<pleb::reply> *reply       = nullptr,
			bool                      process_now = true)
			:
			resource(std::move(_resource)), method(_method), value(std::forward<T>(_value)), _reply(reply)
				{if (process_now) process();}

		// Make a request to the given topic with method and value.  (defined in resource.h)
		template<typename T>
		request(topic_view topic, pleb::method method, T &&value, std::future<reply> *reply = nullptr, bool process_now = true);


		// Process this request.  This may be done repeatedly.
		//    This function is defined in resource.h
		void process();


		// Syntactic sugar:  Allow for  std::future<reply> reply = pleb::request(...)
		operator std::future<pleb::reply>() const    {return _reply ? std::move(*_reply) : std::future<pleb::reply>();}


		// Access value as a specific type.  Only succeeds if the type is an exact match.
		template<class T> const T *value_cast() const noexcept    {return std::any_cast<T>(&value);}
		template<class T> T       *value_cast()       noexcept    {return std::any_cast<T>(&value);}

		// Get a constant pointer to the value.
		//  This method automatically deals with indirect values.
		template<class T> const T *get() const noexcept    {return pleb::any_const_ptr<T>(value);}

		// Access a mutable pointer to the value.
		//  This method automatically deals with indirect values.
		//  This will fail when a const request holds a value directly.
		template<class T> T       *get_mutable() const noexcept    {return pleb::any_ptr<T>(value);}
		template<class T> T       *get_mutable()       noexcept    {return pleb::any_ptr<T>(value);}

		/*
			Post an immediate reply.
		*/
		template<class T = std::any>
		void reply(status status, T &&value = {}) const
		{
			if (!_reply) return;
			promise_t p; p.set_value(pleb::reply{status, std::move(value)});
			*_reply = p.get_future();
		}

		/*
			Promise a later reply.
		*/
		std::promise<pleb::reply> promise()                               const    {promise_t p; promise(p.get_future()); return std::move(p);}
		void                      promise(std::future<pleb::reply> reply) const    {if (_reply) *_reply = std::move(reply);}


		/*
			Convenience methods for replying with common statuses.
		*/
#define REPLY_SHORTHAND(MethodName, Status)    template<class T = std::any> void MethodName(T &&value = {}) {reply(Status, std::forward<T>(value));}

		REPLY_SHORTHAND( reply_OK,                   statuses::OK )
		REPLY_SHORTHAND( reply_Created,              statuses::Created )

		REPLY_SHORTHAND( reply_NotFound,             statuses::NotFound )
		REPLY_SHORTHAND( reply_MethodNotAllowed,     statuses::MethodNotAllowed )
		REPLY_SHORTHAND( reply_Gone,                 statuses::Gone )
		REPLY_SHORTHAND( reply_UnsupportedMediaType, statuses::UnsupportedMediaType )

		REPLY_SHORTHAND( reply_InternalServerError,  statuses::InternalServerError )
		REPLY_SHORTHAND( reply_NotImplemented,       statuses::NotImplemented )
	};

	

	using service_function = std::function<void(request&)>;


	/*
		Class for a registered service function which can fulfill requests.
	*/
	class service
	{
	public:
		service(std::shared_ptr<resource> _resource, service_function &&_func)
			:
			resource(std::move(_resource)), func(std::move(_func)) {}

		const std::shared_ptr<resource> resource;
		const service_function          func;
	};
}
