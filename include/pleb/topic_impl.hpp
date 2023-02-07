#pragma once


#include "resource_node.hpp"

#include "request.hpp"
#include "event.hpp"


/*
	A minimal system for pub-sub and request-reply messaging patterns,
		with native calling and native object passing.
	
	A resource supports:
		One service (or none)
		Any number of subscribers
		Any number of child resources (organized like filesystem directories)

	These classes do not provide asynchronous execution or serialization;
		those features can be implemented as part of the function wrapper.
		Subscribers throwing an exception will halt processing of an event.

	TEARDOWN SAFETY NOTICE:
		Calls may be in progress when the subscription is released.
		Some protection against this case is necessary in client code.
*/


namespace pleb
{

	inline resource_node_ptr global_root_resource() noexcept
	{
		static resource_node_ptr root = resource_node::create(""); return root;
	}


	inline topic_base_<lazy_path>::topic_base_(const topic_base_<void> &o)
		:
		_nearest(null_topic_error::check(o._node,  "can't make topic_path", "(null topic)")),
		_path(_nearest->path()) {}

	inline topic_base_<lazy_path>::topic_base_(topic_base_<void> &&o)
		:
		_nearest(std::move(o._node))
	{
		null_topic_error::check(_nearest, "can't make topic_path", "(null topic)");
		_path = _nearest->path();
	}

	inline topic_base_<lazy_path>::topic_base_(const resource_node_ptr &node)
		:
		_nearest(null_topic_error::check(node, "can't make topic_path")),
		_path(node->path())
	{
	}
	inline topic_base_<lazy_path>::topic_base_(const resource_node_ptr &node, std::string_view subpath)
		:
		_nearest(null_topic_error::check(node, "can't make topic_path")),
		_path(node->path())
	{
		_push(subpath);
		_resolve();
	}


	inline void topic_base_<void>::_push(topic_view subpath)
	{
		for (auto part : subpath) _node = _node->get_child(part);
	}

	inline void topic_base_<void>::_pop()
	{
		null_topic_error::check(_node, "null topic has no parent", "(null topic)");
		_node = _node->parent();
	}

	inline void topic_base_<lazy_path>::_push(topic_view addition)
	{
		for (auto part : addition)
		{
			if (_path.length()) _path.push_back('/');
			_path.append(part.data(), part.length());
		}
	}

	inline void topic_base_<lazy_path>::_pop() noexcept
	{
		if (_path.length())
		{
			auto parent = topic_view(_path).parent();
			_path.assign(parent.data(), parent.length());
		}
		else if (const auto &parent_node = _nearest->parent())
		{
			_nearest = parent_node;
		}
		// Otherwise it's a root, and no change is made.
	}

	inline std::string_view topic_base_<void>::_back() const noexcept
	{
		return _node ? _node->id() : "<null>";
	}


	inline bool topic_base_<lazy_path>::_is_resolved() const noexcept
	{
		return _nearest->path().length() >= _path.length();
	}

	inline std::string_view topic_base_<lazy_path>::_unresolved() const noexcept
	{
		return _is_resolved() ? std::string_view() :
			std::string_view(_path).substr(
				_nearest->path().length() ? _nearest->path().length()+1 : 0);
	}

	inline topic_base_<lazy_path>& topic_base_<lazy_path>::_resolve() noexcept
	{
		auto unresolved = _unresolved();
		for (auto part : topic_view(_unresolved()))
		{
			if (auto child = _nearest->try_child(part))
				_nearest = std::move(child);
			else break;
		}
		return *this;
	}
	inline const resource_node_ptr &topic_base_<lazy_path>::_realize()
	{
		for (auto part : topic_view(_unresolved())) _nearest = _nearest->get_child(part);
		return _nearest;
	}

	inline std::string_view topic_base_<void>::_view() const
	{
		return _node ? _node->path() : "<null>";
	}
	
	template<typename P> [[nodiscard]]
	std::shared_ptr<subscription> topic_<P>::subscribe(
		subscriber_function &&f,
		subscription_config   flags)
	{
		auto &node = _realize();
		if constexpr (type_can_be_null) null_topic_error::check(node, "can't subscribe", "(null topic)");
		auto ptr = node->emplace_subscriber(node, std::move(f), flags);
		publish(statuses::Created, ptr, flags::announce_receiver | flags::recursive);
		return ptr;
	}

	template<typename P>
	template<class T> [[nodiscard]]
	inline std::shared_ptr<subscription> topic_<P>::subscribe(
		std::weak_ptr<T> handler_object,
		void        (T::*handler_method)(const pleb::event&),
		subscription_config flags)
	{
		return subscribe([m=handler_method, w=std::move(handler_object)](const pleb::event &r)
		{
			if (auto s=w.lock()) (s.get()->*m)(r);
		}, flags);
	}

	template<typename P>
	template<typename T>
	void topic_<P>::publish(
		status           status,
		T              &&item,
		message_flags    flags) const
	{
		if constexpr (type_can_be_null) null_topic_error::check(base_t::_nearest_node(), "can't publish", "(null topic)");
		pleb::event e(*this, status, std::forward<T>(item), flags);
		publish(e);
	}

	template<typename P> [[nodiscard]]
	std::shared_ptr<service> topic_<P>::serve(
		service_function &&function,
		service_config     flags) noexcept
	{
		auto &node = _realize();
		if constexpr (type_can_be_null) null_topic_error::check(node, "can't serve", "(null topic)");
		auto ptr = node->try_emplace_service(node, std::move(function), flags);
		if (ptr) publish(statuses::Created, ptr, flags::announce_receiver | flags::recursive);
		return ptr;
	}

	template<typename P>
	template<class V>
	void topic_<P>::request(client_ref client, method method, V &&value) const
	{
		pleb::request r(std::move(client), *this, method, std::forward<V>(value));
		issue(r);
	}

	template<typename P>
	template<class V>
	auto_request topic_<P>::request(method method, V &&value) const
	{
		return auto_request(*this, method, std::forward<V>(value));
	}
	template<typename P>
	template<class V>
	auto_retrieve topic_<P>::retrieve(method method, V &&value) const
	{
		return auto_retrieve(*this, method, std::forward<V>(value));
	}

	template<typename P>
	/* */            auto_retrieve topic_<P>::GET   ()          const    {return retrieve(method::GET);}
	template<typename P>
	/* */            auto_retrieve topic_<P>::HEAD  ()          const    {return retrieve(method::HEAD);}
	template<typename P>
	/* */            auto_retrieve topic_<P>::OPTIONS()         const    {return retrieve(method::OPTIONS);}
	template<typename P>
	template<class V> auto_request topic_<P>::PUT   (V &&value) const    {return request(method::PUT,   std::forward<V>(value));}
	template<typename P>
	template<class V> auto_request topic_<P>::POST  (V &&value) const    {return request(method::POST,  std::forward<V>(value));}
	template<typename P>
	template<class V> auto_request topic_<P>::PATCH (V &&value) const    {return request(method::PATCH, std::forward<V>(value));}
	template<typename P>
	/* */             auto_request topic_<P>::DELETE()          const    {return request(method::DELETE);}

	template<typename P>
	void topic_<P>::issue(pleb::request &msg) const
	{
		// Mark the message as unresponded.
		msg.features &= ~flags::did_respond;

		if (service_ptr svc = find_service(msg.filtering))
		{
			try                            {svc->func(msg);}
			catch (status s)               {msg.respond(s);}
			catch (statuses s)             {msg.respond(s);}
			catch (status_exception &e)    {msg.respond(e.status);}

			// Default response if service did not respond or move message.
			if (!(msg.features & flags::did_respond)) msg.respond(statuses::NoContent);

			msg.features |= flags::did_send;
		}
		else
		{
			throw service_not_found("No service available", path());
		}
	}

	/*
		PUBLISH using a prepared pleb::event object.
			pleb::event usually calls this method upon construction;
			it can be used to publish the same event repeatedly.
	*/
	template<typename P>
	void topic_<P>::publish(const pleb::event &msg) const
	{
		const topic_<P>  &target = base_t::_resolve();
		resource_node_ptr node   = target._nearest_node();

		if constexpr (type_can_be_null)
			null_topic_error::check(node, "can't publish event", "(null topic)");

		const bool recursive = msg.recursive();
		auto filtering = msg.filtering & ~flags::recursive;
		
		if (target._is_resolved()) goto start_resolved;

		while (recursive && node)
		{
			filtering |= flags::recursive;

		start_resolved:
			for (subscription &sub : node->subscriptions()) if (sub.accepts(filtering))
			{
				try            {sub.func(msg);}
				catch (...)    {sub.topic._publish_exception(msg, sub, std::current_exception());}
			}

			node = node->parent();
		}
	}

	//template<typename P>
	inline void topic_<void>::_publish_exception(
		const pleb::event  &msg,
		const subscription &sub,
		std::exception_ptr  exception) const
	{
		if (msg.filtering & flags::subscriber_exception)
		{
			// If an exception subscriber throws an exception, publish to the parent instead.
			if (resource_node_ptr parent = _nearest_node()->parent())
				topic(parent).publish(statuses::InternalServerError, exception,
					flags::subscriber_exception | flags::recursive | msg.requirements);
		}
		else
			this->publish(statuses::InternalServerError, exception,
				flags::subscriber_exception | msg.requirements);

		// TODO what if nobody handled the exception??  Unsafe to proceed?
	}

	template<typename P>
	inline service_ptr topic_<P>::find_service(flags::filtering filtering) const noexcept
	{
		if constexpr (type_can_be_null)
			null_topic_error::check(base_t::_nearest_node(), "can't request service", "(null topic)");

		service_ptr service;

		const topic_<P>  &target = base_t::_resolve();
		resource_node_ptr node   = target._nearest_node();

		const bool recursive = (filtering & flags::recursive);
		filtering &= ~flags::recursive;

		if (target._is_resolved()) goto start_resolved;

		while (recursive && node)
		{
			filtering |= flags::recursive;
		start_resolved:
			if (service = node->service_lock()) 
			{
				if (service->accepts(filtering)) break;
				service.reset();
			}
			node = node->parent();
		}

		return service;
	}

	template<typename P>
	inline service_ptr topic_<P>::current_service() const noexcept
	{
		if constexpr (type_can_be_null)
			null_topic_error::check(base_t::_nearest_node(), "can't request service", "(null topic)");

		const topic_<P> &target = base_t::_resolve();
		return target._is_resolved() ? target._nearest_node()->service_lock() : nullptr;
	}


	template<typename P>
	template<typename Callback>
	auto topic_<P>::visit_resources(const Callback &callback, size_t recursion_depth, bool skip_this) const
		-> decltype(callback(std::declval<topic>()), void())
	{
		auto &node = _realize();

		if constexpr (type_can_be_null)
			null_topic_error::check(node, "can't visit resources", "(null topic)");

		if (!skip_this) callback(node);
		auto scan = [&](const std::string &, resource_node_ptr node)
		{
			callback(node);
			if (recursion_depth--) {topic(node).visit_resources(callback, recursion_depth-1, true);}
		};
		if (recursion_depth--) node->visit_children(scan);
	}
	template<typename P>
	template<typename Callback>
	auto topic_<P>::visit_services(const Callback &callback, size_t recursion_depth) const
		-> decltype(callback(std::declval<service_ptr>()), void())
	{
		visit_resources([&](const topic &rc)
			{if (auto svc=rc.current_service()) callback(std::move(svc));},
			recursion_depth);
	}
	template<typename P>
	template<typename Callback>
	auto topic_<P>::visit_subscriptions(const Callback &callback, size_t recursion_depth) const
		-> decltype(callback(std::declval<subscription_ptr>()), void())
	{
		visit_resources([&](const topic &rc)
			{for (auto i=rc._nearest_node()->subscriptions().begin(),e=rc._nearest_node()->subscriptions().end(); i!=e; ++i) callback(i);},
			recursion_depth);
	}
}
