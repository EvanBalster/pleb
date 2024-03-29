#pragma once

#include <future>
#include <exception>

#include "response.hpp"
#include "method.hpp"

/*
	PLEB delivers requests to services, which may then respond
		by way of a future or callback function.
		Response mechanisms are defined in <response.h>.
	
	See <resource.h> for more functionality.
*/


namespace pleb
{
	/*
		Exception thrown when no service can handle a request.
	*/
	class service_not_found : public detail::topic_runtime_error
	{
	public:
		using detail::topic_runtime_error::topic_runtime_error;
	};


	/*
		Services are retained by a shared_ptr throughout their lifetimes.
	*/
	class service;
	using service_ptr  = std::shared_ptr<service>;


	/*
		A request is a message directed to a single service.
			It may optionally provide a method to send a respond.
	*/
	class request : public message
	{
	public:
		using content::value;
		using content::value_cast;
		using content::get;
		using content::get_mutable;


	private:
		client_ptr _client;


	public:
		/*
			Compose a request manually.
				If a client is supplied, the request will occur immediately.
		*/
		template<typename T = std_any::any>
		request(
			client_ref        client,
			const topic_path &topic,
			pleb::method      method,
			T               &&value  = {},
			message_flags     flags  = {})
			:
			message(topic, code_t(method.code), std::forward<T>(value), flags),
			_client(client)
			{}


		~request() noexcept    {}


		// Request method from <method.h>.  Stored in the code field.
		method method() const noexcept    {return pleb::method_enum(code);}


		/*
			Issue this request without accepting any response.
				This is more lightweight than a respondable request.
		*/
		void push()                                     {issue(nullptr);}

		/*
			Issue this request and deliver the reply through std::future.
		*/
		template<typename Response = pleb::response>
		std::future<Response> async()                   {std::future<Response> f; issue(std::make_shared<client_promise<Response>>(&f)); return f;}

		/*
			Issue this request and return the response.
				This may block if the service doesn't respond immediately.
		*/
		template<typename Response = pleb::response>
		Response await()                                {return async<Response>().get();} // TODO elide std::future for immediate services


		// Issue this request to its targeted resource.
		//    Normally this happens automatically when constructing request.
		//    This may be done repeatedly.
		//    This function is defined in resource.h
		void issue(client_ref client)    {_client = std::move(client); issue();}

		void issue()                     {topic.issue(*this);}



		/*
			Respond to the request.
				This is usually called by the receiving service.
		*/
		template<class T = std_any::any>
		void respond(status status, T &&value = {}, message_flags flags = {})
		{
			features = features | flags::did_respond;
			if (_client) _client->respond(topic, status, std::forward<T>(value), flags);
		}

		/*
			Claim the client for this message.
				Think of this as a promise to respond later.
		*/
		client_ptr claim_client() noexcept    {return std::move(_client);}


		/*
			Convenience methods for replying with common statuses.
		*/
#define REPLY_SHORTHAND(MethodName, Status)    template<class T = std_any::any> void MethodName(T &&value = {}) {respond(Status, std::forward<T>(value));}

		REPLY_SHORTHAND( respond_OK,                   statuses::OK )
		REPLY_SHORTHAND( respond_Created,              statuses::Created )

		REPLY_SHORTHAND( respond_NotFound,             statuses::NotFound )
		REPLY_SHORTHAND( respond_MethodNotAllowed,     statuses::MethodNotAllowed )
		REPLY_SHORTHAND( respond_Gone,                 statuses::Gone )
		REPLY_SHORTHAND( respond_UnsupportedMediaType, statuses::UnsupportedMediaType )

		REPLY_SHORTHAND( respond_InternalServerError,  statuses::InternalServerError )
		REPLY_SHORTHAND( respond_NotImplemented,       statuses::NotImplemented )
	};

	
	/*
		Services are implemented as a function taking a request.
	*/
	using service_function = std::function<void(request&)>;


	/*
		Class for a registered service function which can fulfill requests.
	*/
	class service : public receiver
	{
	public:
		const pleb::topic topic;


	private:
		template<class P> friend class topic_;
		const service_function func;


	public:
		// Note this class will normally only be created by topic::serve() and co.
		service(
			const pleb::topic &_topic,
			service_function &&_func,
			service_config     flags = {})
			:
			receiver(flags), topic(_topic), func(std::move(_func)) {}
	};



	/*
		A request, returned from some function, which automatically
			dispatches itself based on how it is handled by the caller.
	*/
	class auto_request : public request
	{
	public:
		/*
			auto_request is constructed without a client reference.
		*/
		template<typename T = std_any::any>
		auto_request(
			pleb::topic    topic,
			pleb::method   method,
			T            &&value = {})
			:
			request(nullptr, std::move(topic), method, std::forward<T>(value)) {}

		/*
			1: if auto_request is discarded without being issued,
				the request will be pushed (no way to respond).

			eg:  pleb::DELETE("/resource/1");
		*/
		~auto_request() noexcept(false)
		{
			// auto_request will not fire if destroyed as a result of stack unwinding.
#if __cplusplus >= 201700 || _MSVC_LANG >= 201700
			if (std::uncaught_exceptions()) return;
#else
			if (std::uncaught_exception()) return;
#endif
			if (!(features & flags::did_send))
			{
				this->push();
			}
		}

		/*
			2: auto_request may be converted into a std::future.

			eg:  std::future<response> result = pleb::GET("/resource/1");

			When using types other than pleb::response, status is discarded
			and a response of incompatible type will throw bad_any_cast.
		*/
		template<typename Response>
		operator std::future<Response>()    {return this->async<Response>();}

		/*
			3: a request may be explicitly converted to some other type.
				This results in a call to await<T>, which may block.

			eg:  auto resultText = std::string(pleb::POST("/resource/1", ""));
			
			When using types other than pleb::response, status is discarded
			and a response of incompatible type will throw bad_any_cast.
		*/
		template<typename T>
		explicit operator T()    {return this->await<T>();}

		operator response()    {return this->await<response>();}

		/*
			4: messages may be sent by calling push(), async<T> or await<T> on this object.
		*/
	};

	/*
		Variant of auto_request for GET and other side-effect-free methods.
			Throws a compile-time warning if the response is not captured.
			Requests will not be pushed to the service if this happens.
	*/
	class [[nodiscard]] auto_retrieve : public auto_request
	{
	public:
		using auto_request::auto_request;

		~auto_retrieve()
		{
			features |= flags::did_send;
		}
	};



	/*
		A request relay is a special service that forwards messages.
			It has no additional functionality at the moment.
	*/
	class service_relay : public service
	{
	protected:
		using service::service;
	};


	/*
		Implementation of methods from the topic class.
	*/
	template<typename SubPath>
	std::shared_ptr<service_relay> topic_<SubPath>::forward_requests(
		topic          service_topic,
		service_config flags)
	{
		if (!(flags.filtering & pleb::flags::recursive) && is_ancestor_of(service_topic))
			throw std::logic_error("Forwarding requests to a child topic might cause a stack overflow.");

		return std::static_pointer_cast<service_relay>(
			serve([service_topic = std::move(service_topic)](pleb::request &request)
		{
			request.topic = service_topic;
			service_topic.issue(request);
		}, flags));
	}
}
