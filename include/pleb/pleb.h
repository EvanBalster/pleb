#pragma once


#include "topic.h"


namespace pleb
{
	// Subscribe to a topic, providing a handler function.
	[[nodiscard]] inline std::shared_ptr<subscription>
		subscribe(
			path_view             topic,
			subscriber_function &&function) noexcept    {return pleb::topic::find(topic)->subscribe(std::move(function));}

	// Subscribe to a topic, providing a handler object and method.
	template<class T> [[nodiscard]] std::shared_ptr<subscription>
		subscribe(
			path_view path,
			T        *handler_object,
			void (T::*handler_method)(const pleb::event&))    {return subscribe(path, std::bind(handler_method, handler_object, std::placeholders::_1));}

	// Publish a value to a topic.
	template<typename T>
	void publish(
			path_view topic,
			T       &&item) noexcept    {return pleb::topic::find_nearest(topic)->publish(std::move(item));}


	[[nodiscard]] inline std::shared_ptr<service>
		serve(
			path_view          path,
			service_function &&function) noexcept
	{
		return pleb::topic::find(path)->serve(std::move(function));
	}

	template<class T> [[nodiscard]] std::shared_ptr<service>
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
	[[nodiscard]] inline            std::future<reply> request_get   (path_view path)           {return topic::find(path)->request_get();}
	template<class T> [[nodiscard]] std::future<reply> request_post  (path_view path, T &&v)    {return topic::find(path)->request_post(std::move(v));}
	template<class T> [[nodiscard]] std::future<reply> request_patch (path_view path, T &&v)    {return topic::find(path)->request_patch(std::move(v));}
	[[nodiscard]] inline            std::future<reply> request_delete(path_view path)           {return topic::find(path)->request_delete();}

	[[nodiscard]] inline                         reply sync_get   (path_view path)           {return topic::find(path)->sync_get();}
	template<class T> [[nodiscard]]              reply sync_post  (path_view path, T &&v)    {return topic::find(path)->sync_post(std::move(v));}
	template<class T> [[nodiscard]]              reply sync_patch (path_view path, T &&v)    {return topic::find(path)->sync_patch(std::move(v));}
	[[nodiscard]] inline                         reply sync_delete(path_view path)           {return topic::find(path)->sync_delete();}

	template<class T>                             void push       (path_view path, T &&v)    {return topic::find(path)->push_post(std::move(v));}
	template<class T>                             void push_post  (path_view path, T &&v)    {return topic::find(path)->push_post(std::move(v));}
	template<class T>                             void push_patch (path_view path, T &&v)    {return topic::find(path)->push_patch(std::move(v));}
	inline                                        void push_delete(path_view path)           {return topic::find(path)->push_delete();}
}
