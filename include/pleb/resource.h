#pragma once


#include <future>
#include <exception>
#include "pleb_base.h"


/*
	A minimal system for acessing a global hierarchy of resources.
		Each resource may have one service providing it.

	These classes do not provide asynchronous execution, futures or serialization;
		those features can be implemented as part of the function wrapper.
		Subscribers throwing an exception will halt processing of an event.

	TEARDOWN SAFETY NOTICE:
		Calls may be in progress when the service is released.
		Some protection against this case is necessary in client code.
*/


namespace pleb
{
	namespace errors
	{
		class no_such_service : public std::runtime_error
		{
		public:
			no_such_service(const std::string& path)    : runtime_error("no such service: " + path) {}
			no_such_service(path_view path)             : no_such_service(path.string) {}
			no_such_service(std::string_view path)      : no_such_service(std::string(path)) {}
			no_such_service(const char* path)           : no_such_service(std::string(path)) {}

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
	using service_ptr = std::shared_ptr<service>;

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
		resource    &resource;
		const method method;
		std::any     value;
		
	private:
		using promise_t = std::promise<pleb::reply>;
		using future_t = std::future<pleb::reply>;
		std::future<pleb::reply> *_reply;

	public:
		/*
			Compose a request manually.
		*/
		template<typename T>
		request(
			pleb::resource     &resource,
			pleb::method        method,
			T                 &&value,
			std::future<reply> *reply = nullptr,
			bool                process_now = true);

		// Make a request to the given path with method and value.
		template<typename T>
		request(path_view path, pleb::method method, T &&value, std::future<reply> *reply = nullptr);


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
		Minimal services system.
			Can be used for request-reply transactions and pipelines.
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

	/*
		Minimal services system.
			Can be used for request-reply transactions and pipelines.
	*/
	class resource :
		protected coop::unmanaged::trie<service>
	{
	public:
		using service = pleb::service;

	public:
		// Access the global root resource.
		static resource_ptr root() noexcept    {static resource_ptr root = _asResource(_trie::create()); return root;}

		// Access a resource by path.
		static resource_ptr find(path_view path) noexcept    {return root()->subpath(path);}

		// Access the parent resource (root's parent is null)
		resource_ptr parent() noexcept                        {return _asResource(_trie::parent());}

		// Access a child of this resource
		resource_ptr subpath(path_view subpath) noexcept    {return _asResource(_trie::get    (subpath));}
		resource_ptr nearest(path_view subpath) noexcept    {return _asResource(_trie::nearest(subpath));}

		/*
			Process a request.
				pleb::request calls this method upon construction;
				it can be used to process the same request again.
		*/
		void process(pleb::request &request)
		{
			if (auto svc = _trie::lock()) svc->func(request);
			else throw errors::no_such_service("???");
		}

		/*
			Request methods with asynchronous reply.
		*/
		template<typename T> [[nodiscard]]
		std::future<reply> request(method method, T &&item)    {std::future<reply> r; pleb::request(*this, method, item, &r); return std::move(r);}

		[[nodiscard]]                   std::future<reply> request       ()         {return request(method::GET, std::any());}
		[[nodiscard]]                   std::future<reply> request_get   ()         {return request(method::GET, std::any());}
		template<class T> [[nodiscard]] std::future<reply> request_post  (T &&v)    {return request(method::POST, std::move(item));}
		template<class T> [[nodiscard]] std::future<reply> request_patch (T &&v)    {return request(method::PATCH, std::move(item));}
		[[nodiscard]]                   std::future<reply> request_delete()         {return request(method::DELETE, std::any());}

		/*
			Request methods with synchronous (possibly blocking) reply.
		*/
		template<typename T> [[nodiscard]]
		reply sync_request(method method, T &&item)    {return request(method, std::move(item)).get();}

		[[nodiscard]]                   reply sync_get   ()         {return sync_request(method::GET, std::any());}
		template<class T> [[nodiscard]] reply sync_post  (T &&v)    {return sync_request(method::POST, std::move(item));}
		template<class T> [[nodiscard]] reply sync_patch (T &&v)    {return sync_request(method::PATCH, std::move(item));}
		[[nodiscard]]                   reply sync_delete()         {return sync_request(method::DELETE, std::any());}

		/*
			Push methods; ie, request with no opportunity to reply.
		*/
		template<typename T>
		void push(method method, T &&item)    {pleb::request r(*this, method, item, nullptr);}

		template<class T> void push      (T &&v)    {push(method::POST, std::move(v));}
		template<class T> void push_post (T &&v)    {push(method::POST, std::move(v));}
		template<class T> void push_patch(T &&v)    {push(method::PATCH, std::move(v));}
		void                   push_delete()        {push(method::DELETE, std::any());}


		/*
			Register a function to service this resource.
				May fail, returning null, if a service is already registered.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(                   service_function &&function) noexcept    {return _trie::try_emplace(shared_from_this(), std::move(function));}
		[[nodiscard]] std::shared_ptr<service> serve(path_view subpath, service_function &&function) noexcept    {return this->subpath(subpath)->serve(std::move(function));}


		// Support shared_from_this
		[[nodiscard]] resource_ptr                    shared_from_this()          {return resource_ptr                   (_trie::shared_from_this(), this);}
		[[nodiscard]] std::shared_ptr<const resource> shared_from_this() const    {return std::shared_ptr<const resource>(_trie::shared_from_this(), this);}


	private:
		using _trie = coop::unmanaged::trie<service>;

		resource(std::shared_ptr<resource> parent)                   : _trie(std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static resource_ptr _asResource(std::shared_ptr<_trie> p)    {return std::shared_ptr<resource>(p, (resource*) &*p);}
	};


	[[nodiscard]] inline
	std::shared_ptr<service>
		serve(
			path_view          path,
			service_function &&function) noexcept
	{
		return pleb::resource::find(path)->serve(std::move(function));
	}

	template<class T> [[nodiscard]]
	std::shared_ptr<service>
		serve(
			path_view path,
			T        *observer_object,
			void (T::*observer_method)(request&))
	{
		return serve(path, std::bind(observer_method, observer_object, std::placeholders::_1));
	}

	// Perform a manual request to the given path.
	template<typename T> [[nodiscard]]
	inline request::request(path_view path, pleb::method method, T &&value, std::future<pleb::reply> *reply) :
		request(pleb::resource::find(path), method, std::move(value), reply) {}

	/*
		Perform requests with asynchronous replies.
	*/
	[[nodiscard]] inline            std::future<reply> request_get   (path_view path)           {return resource::find(path)->request_get();}
	template<class T> [[nodiscard]] std::future<reply> request_post  (path_view path, T &&v)    {return resource::find(path)->request_post(std::move(v));}
	template<class T> [[nodiscard]] std::future<reply> request_patch (path_view path, T &&v)    {return resource::find(path)->request_patch(std::move(v));}
	[[nodiscard]] inline            std::future<reply> request_delete(path_view path)           {return resource::find(path)->request_delete();}

	[[nodiscard]] inline            reply sync_get   (path_view path)           {return resource::find(path)->sync_get();}
	template<class T> [[nodiscard]] reply sync_post  (path_view path, T &&v)    {return resource::find(path)->sync_post(std::move(v));}
	template<class T> [[nodiscard]] reply sync_patch (path_view path, T &&v)    {return resource::find(path)->sync_patch(std::move(v));}
	[[nodiscard]] inline            reply sync_delete(path_view path)           {return resource::find(path)->sync_delete();}

	template<class T>                void push       (path_view path, T &&v)    {return resource::find(path)->push_post(std::move(v));}
	template<class T>                void push_post  (path_view path, T &&v)    {return resource::find(path)->push_post(std::move(v));}
	template<class T>                void push_patch (path_view path, T &&v)    {return resource::find(path)->push_patch(std::move(v));}
	inline                           void push_delete(path_view path)           {return resource::find(path)->push_delete();}
}


// Request constructor implementation
template<typename T> [[nodiscard]]
inline pleb::request::request(
	pleb::resource          &_resource,
	pleb::method             _method,
	T                      &&_value,
	std::future<pleb::reply> *reply,
	bool                      process_now)
	:
	resource(_resource), method(_method), value(std::move(_value)), _reply(reply)
{
	if (process_now) resource.process(*this);
}
