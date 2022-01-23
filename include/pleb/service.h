#pragma once


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
	/*
		Events are organized under a topic -- a slash-delimited string.
	*/
	class resource;

	/*
		High-performance code 
	*/
	using resource_ptr = std::shared_ptr<resource>;

	/*
		Minimal pub/sub system.
			Can be used for event broadcast and surveyor pattern.
	*/
	class service
	{
	public:
		service(std::shared_ptr<resource> _resource, function_any_ref &&_func)
			:
			resource(std::move(_resource)), func(std::move(_func)) {}

		const std::shared_ptr<resource> resource;
		const function_any_ref          func;
	};

	/*
		Minimal services system.
			Can be used for request-reply transactions and pipelines.
	*/
	class resource :
		protected coop::unmanaged::trie<service>
	{
	public:
		using function_type = function_any_ref;

		using service = pleb::service;

	public:
		// Access the global root topic.
		static resource_ptr root() noexcept    {static resource_ptr root = _asResource(_trie::create()); return root;}

		// Access the parent resource (root's parent is null)
		resource_ptr parent() noexcept                        {return _asResource(_trie::parent());}

		// Access a child of this resource
		resource_ptr subpath(path_view subpath) noexcept    {return _asResource(_trie::get    (subpath));}
		resource_ptr nearest(path_view subpath) noexcept    {return _asResource(_trie::nearest(subpath));}

		// Publish something to this topic or a subtopic.
		template<typename T>
		bool request(                   T &&item)    {auto svc = _trie::lock(); if (!svc) return false; svc->func(std::move(item)); return true;}
		template<typename T>
		bool request(path_view subpath, T &&item)    {return this->subpath(subpath)->request(std::move(item));}

		// Create a subscription to this topic and all subtopics, or a subtopic and its subtopics.
		std::shared_ptr<service> serve(                          function_type &&function) noexcept    {return _trie::try_emplace(shared_from_this(), std::move(function));}
		std::shared_ptr<service> serve(std::string_view subpath, function_type &&function) noexcept    {this->subpath(subpath)->serve(std::move(function));}


		// Support shared_from_this
		resource_ptr                    shared_from_this()          {return resource_ptr                   (_trie::shared_from_this(), this);}
		std::shared_ptr<const resource> shared_from_this() const    {return std::shared_ptr<const resource>(_trie::shared_from_this(), this);}


	private:
		using _trie = coop::unmanaged::trie<service>;

		resource(std::shared_ptr<resource> parent)                   : _trie(std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static resource_ptr _asResource(std::shared_ptr<_trie> p)    {return std::shared_ptr<resource>(p, (resource*) &*p);}
	};


	static std::shared_ptr<service> serve(
		std::string_view   path,
		function_any_ref &&function) noexcept
	{
		return pleb::resource::root()->serve(path, std::move(function));
	}

	template<typename T>
	static void request(
		std::string_view path,
		T              &&item) noexcept
	{
		return pleb::resource::root()->request(path, std::move(item));
	}
}
