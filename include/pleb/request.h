#pragma once


#include <future>
#include <exception>
#include "pleb_base.h"


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
		Events are organized under a topic -- a slash-delimited string.
	*/
	class resource;
	class service;

	/*
		Resources and services are retained by shared pointers.
			Resource pointers can be cached for repeated requests.
	*/
	using resource_ptr = std::shared_ptr<resource>;
	using service_ptr  = std::shared_ptr<service>;

	/*
		Replies may be produced in response to a request (see below)
	*/
	class reply
	{
	public:
		int      status;
		std::any value;
	};

	/*
		Requests are messages directed at resources (fulfilled by services).
	*/
	enum class method
	{
		GET    = 1, // Query data.  Nullipotent.
		PATCH  = 2, // Modify existing data. Not necessarily idempotent.
		POST   = 3, // create things, call functions. Not idempotent.
		DELETE = 4, // Delete things.  Idempotent.
	};
	static const method PLEB_GET    = method::GET;
	static const method PLEB_PATCH  = method::PATCH;
	static const method PLEB_POST   = method::POST;
	static const method PLEB_DELETE = method::DELETE;

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
			resource(std::move(_resource)), method(_method), value(std::move(_value)), _reply(reply)
				{if (process_now) process();}

		// Make a request to the given topic with method and value.  (defined in resource.h)
		template<typename T>
		request(topic_view topic, pleb::method method, T &&value, std::future<reply> *reply = nullptr, bool process_now = true);


		// Process this request.  This may be done repeatedly.
		//    This function is defined in resource.h
		void process();


		// Syntactic sugar:  Allow for  std::future<reply> reply = pleb::request(...)
		operator std::future<pleb::reply>() const    {return _reply ? std::move(*_reply) : std::future<pleb::reply>();}


		// Access value as a specific type.
		template<typename T> const T *cast() const noexcept    {return std::any_cast<T>(&value);}
		template<typename T> T       *cast()       noexcept    {return std::any_cast<T>(&value);}

		// Access value, allowing it to be supplied by value or shared_ptr.
		template<typename T> T *get() const noexcept    {return pleb::any_ptr<T>(value);}

		/*
			Post an immediate reply.
		*/
		template<class T = std::any>
		void reply(int status = 200, T &&value = {}) const
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
