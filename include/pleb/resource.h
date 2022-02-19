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
		High-performance code 
	*/
	using resource_ptr = std::shared_ptr<resource>;
	using service_ptr = std::shared_ptr<service>;

	/*
		Minimal services system.
			Can be used for request-reply transactions and pipelines.
	*/
	class service
	{
	public:
		service(std::shared_ptr<resource> _resource, handler_function &&_func)
			:
			resource(std::move(_resource)), func(std::move(_func)) {}

		const std::shared_ptr<resource> resource;
		const handler_function          func;
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

		// Publish something to this topic or a subtopic.
		void request(std::any &item)                 {auto svc = _trie::lock(); if (!svc) throw errors::no_such_service("???"); svc->func(std::move(item));}
		template<typename T>
		void request(                   T &&item)    {std::any payload(std::move(item)); request(payload);}
		template<typename T>
		void request(path_view subpath, T &&item)    {return this->subpath(subpath)->request(std::move(item));}

		// Create a subscription to this topic and all subtopics, or a subtopic and its subtopics.
		std::shared_ptr<service> serve(                          handler_function &&function) noexcept    {return _trie::try_emplace(shared_from_this(), std::move(function));}
		std::shared_ptr<service> serve(std::string_view subpath, handler_function &&function) noexcept    {return this->subpath(subpath)->serve(std::move(function));}


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
			handler_function &&function) noexcept
	{
		return pleb::resource::root()->serve(path, std::move(function));
	}

	template<class T>
	static std::shared_ptr<service>
		serve(
			std::string_view path,
			T               *observer_object,
			void        (T::*observer_method)(std::any&))
	{
		return serve(path, std::bind(observer_method, observer_object, std::placeholders::_1));
	}

	template<typename T>
	static void
		request(
			std::string_view path,
			T              &&item) noexcept
	{
		return pleb::resource::root()->request(path, std::move(item));
	}
}
