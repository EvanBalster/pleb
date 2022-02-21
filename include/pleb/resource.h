#pragma once


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
		int          status = 0;

		// TODO optional reply-handling field

	public:
		// Make a request with an empty value.
		request(
			pleb::resource &resource,
			pleb::method    method = pleb::method::GET)    : request(resource, method, std::any()) {}

		// Make a request with a value.
		template<typename T>
		request(
			pleb::resource &resource,
			pleb::method    method,
			T             &&value);

		// Access value as a specific type.
		template<typename T> const T *cast() const noexcept    {return std::any_cast<T>(&value);}
		template<typename T> T       *cast()       noexcept    {return std::any_cast<T>(&value);}

		// Access value, allowing it to be supplied by value or shared_ptr.
		template<typename T> T *get() const noexcept    {return pleb::any_ptr<T>(value);}
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

		// Process an already-constructed request.
		void process(request &request)    {auto svc = _trie::lock(); if (!svc) throw errors::no_such_service("???"); svc->func(request);}

		// Publish something to this topic or a subtopic.
		std::any get    ()                  {request r(*this, method::GET); return std::move(r.value);}
		void     post   (std::any &item)    {request r(*this, method::POST, std::move(item)); item = std::move(r.value);}
		void     patch  (std::any &item)    {request r(*this, method::PATCH, std::move(item)); item = std::move(r.value);}
		std::any delete_()                  {request r(*this, method::DELETE); return std::move(r.value);}

		// Create a subscription to this topic and all subtopics, or a subtopic and its subtopics.
		std::shared_ptr<service> serve(                          service_function &&function) noexcept    {return _trie::try_emplace(shared_from_this(), std::move(function));}
		std::shared_ptr<service> serve(std::string_view subpath, service_function &&function) noexcept    {return this->subpath(subpath)->serve(std::move(function));}


		// Support shared_from_this
		resource_ptr                    shared_from_this()          {return resource_ptr                   (_trie::shared_from_this(), this);}
		std::shared_ptr<const resource> shared_from_this() const    {return std::shared_ptr<const resource>(_trie::shared_from_this(), this);}


	private:
		using _trie = coop::unmanaged::trie<service>;

		resource(std::shared_ptr<resource> parent)                   : _trie(std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static resource_ptr _asResource(std::shared_ptr<_trie> p)    {return std::shared_ptr<resource>(p, (resource*) &*p);}
	};


	static std::shared_ptr<service>
		serve(
			std::string_view   path,
			service_function &&function) noexcept
	{
		return pleb::resource::root()->serve(path, std::move(function));
	}

	template<class T>
	static std::shared_ptr<service>
		serve(
			std::string_view path,
			T               *observer_object,
			void        (T::*observer_method)(request&))
	{
		return serve(path, std::bind(observer_method, observer_object, std::placeholders::_1));
	}

	template<typename T>
	inline request::request(
		pleb::resource &_resource,
		pleb::method    _method,
		T             &&_value)
		:
		resource(_resource),
		method(_method),
		value(std::move(_value))
	{
		resource.process(*this);
	}
}
