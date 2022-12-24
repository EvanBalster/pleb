#pragma once


#include <future>
#include <exception>

#include "message.hpp"
#include "status.hpp"

/*
	Certain messages, such as requests, provide for responding to the sender.
*/

namespace pleb
{
	/*
		Services are retained by a shared_ptr throughout their lifetimes.
	*/
	class service;
	using service_ptr = std::shared_ptr<service>;
	class client;
	using client_ptr = std::shared_ptr<client>;


	/*
		Replies may be produced in response to a request (see below)
	*/
	class response : public message
	{
	public:
		using content::value;
		using content::value_cast;
		using content::get;
		using content::get_mutable;


	public:
		template<typename T = std::any>
		response(
			const topic_path &topic, // Topic of the request (also provides handling rules)
			pleb::status      status,
			T               &&value     = {},
			flags::filtering  filtering = flags::default_message_filtering,
			flags::handling   handling  = flags::no_special_handling)
			:
			message(std::move(topic), code_t(status.code),
				std::forward<T>(value), filtering, handling)
			{}


		// Response status from <status.h>.  Stored in the code field.
		pleb::status status() const noexcept    {return pleb::status_enum(code);}


	private:
#if _MSC_VER // Workaround for a bug in Visual Studio's implementation of futures
		friend class std::_Associated_state<response>;
		response() : message(pleb::topic_path(),0,{},flags::filtering(0),flags::handling(0)) {}
#endif
	};

	/*
		A response_function is provided whenever a message must support responding.
	*/
	using response_function = std::function<void(response&)>;


	/*
		Caller is an interface for receiving a response.
		A client is a mechanism for accepting replies from a server.
			You may implement your own or use PLEB's futures.
	*/
	class client : public receiver
	{
	protected:
		friend class request;
		const response_function func;


	public:
		client(response_function &&_func, flags::handling handling = flags::no_special_handling)
			:
			receiver(flags::default_client_ignore, handling), func(std::move(_func)) {}


		/*
			Reply to this client.
		*/
		template<class T = std_any::any>
		void respond(topic topic, status status, T &&value = {}) const
		{
			if (func) func(response(std::move(topic), status, std::move(value)));
		}
	};



	/*
		An implementation of client adapting it to C++ futures and promises.
		
		Unless T is pleb::response, non-success statuses will become exceptions.
		Unless T is std::any, mismatched response types may throw std::bad_any_cast.
	*/
	namespace detail
	{
		template<typename T>
		class client_promise_base : public client
		{
		protected:
			std::promise<T> promise;

			void _set(T &&t) noexcept            {try {promise.set_value    (std::move(t));} catch (std::future_error) {}}
			void _set(std::exception_ptr p)      {try {promise.set_exception(std::move(p));} catch (std::future_error) {}}

		public:
			client_promise_base(response_function &&f)    : client(std::move(f), flags::realtime) {}

			std::future<T> get_future()                   {return promise.get_future();}
		};
	}

	template<typename T = response>
	class client_promise : public detail::client_promise_base<T>
	{
	public:
		using base = client_promise_base;

		client_promise(std::future<T> *future)    : client_promise() {if (future) *future = this->get_future();}
		client_promise() :
			client_promise_base([this](response &r)
			{
				try {base::_set(r.move_as<T>());} catch (std::bad_any_cast) {base::_set(std::current_exception());}
			}) {}
	};

	template<>
	class client_promise<response> : public detail::client_promise_base<response>
	{
	public:
		using base = client_promise_base;

		client_promise(std::future<response> *future)    : client_promise() {if (future) *future = this->get_future();}
		client_promise() :
			client_promise_base([this](response &r)
			{
				base::_set(std::move(r));
			}) {}
	};



	template<typename T>
	client_ref::client_ref(std::future<T> *f)
		: client_ptr(f ? std::make_shared<client_promise<T>>(f) : nullptr) {}

	inline client_ref::client_ref(response_function &&f)
		: client_ptr(f ? std::make_shared<pleb::client>(std::move(f)) : nullptr) {}
}