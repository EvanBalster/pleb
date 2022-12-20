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

	inline resource_node_ptr topic::root_node() noexcept
	{
		static resource_node_ptr root = resource_node::create("[root]"); return root;
	}

	std::string topic::_resolve(topic_view subpath)
	{
		std::string result;
		if (!_base) _base = root_node();
		for (auto part : subpath)
		{
			auto child = _base->try_child(part);
			if (!child)
			{
				result = subpath.string.substr(part.data()-subpath.string.data());
				break;
			}
			_base = std::move(child);
		}
		return result;
	}

	void topic::_realize(topic_view subpath)
	{
		if (!_base) _base = root_node();
		for (auto part : subpath) _base = _base->get_child(part);
	}

	inline topic topic::operator/(topic_view subpath) const
	{
		// TODO possible optimizations here
		if (_subpath.length()) return topic(_base, _subpath+"/"+std::string(subpath.string));
		else                   return topic(_base, _subpath);
	}

	inline topic topic::parent() const
	{
		if (_subpath.length())
			return topic(_base, topic_view(_subpath).parent());
		else
			return topic(_base->parent());
	}

	inline void topic::set_to_parent() noexcept
	{
		if (_subpath.length()) _subpath = topic_view(_subpath).parent();
		else                   _base = _base->parent();
	}

	inline std::string_view topic::id() const noexcept
	{
		if (_subpath.length()) return topic_view(_subpath).last_id();
	}

	/*inline std::array<std::string_view, 2> topic::path_view() const noexcept
	{
		return {_base->path(), 
	}*/

	inline std::string topic::path() const
	{
		std::string result = _base->path();
		if (_subpath.length())
		{
			result.push_back('/');
			result.append(_subpath);
		}
		return result;
	}
	
	inline [[nodiscard]] std::shared_ptr<subscription> topic::subscribe(
		subscriber_function &&f,
		flags::filtering      ignore_flags,
		flags::handling       handling)
	{
		_realize();
		return _base->emplace_subscriber(_base, std::move(f));
	}

	template<class T> [[nodiscard]]
	inline std::shared_ptr<subscription> topic::subscribe(
		std::weak_ptr<T> handler_object,
		void        (T::*handler_method)(const pleb::event&),
		flags::filtering ignore_flags,
		flags::handling  handling)
	{
		return subscribe([m=handler_method, w=std::move(handler_object)](const pleb::event &r)
		{
			if (auto s=w.lock()) (s.get()->*m)(r);
		}, ignore_flags, handling);
	}

	template<typename T>
	void topic::publish(
		status           status,
		T              &&item,
		flags::filtering filtering,
		flags::handling  handling)
	{
		pleb::event e(*this, status, std::forward<T>(item), filtering, handling);
		publish(e);
	}

	[[nodiscard]] inline
	std::shared_ptr<service> topic::serve(service_function &&function) noexcept
	{
		_realize();
		return _base->try_emplace_service(_base, std::move(function));
	}

	template<class T> [[nodiscard]]
	std::shared_ptr<service> topic::serve(
		std::weak_ptr<T> svc_object,
		void        (T::*svc_method)(pleb::request&))
	{
		return serve([m=svc_method, w=std::move(svc_object)](pleb::request &r)
		{
			if (auto s = w.lock()) (s.get()->*m)(r);
			else r.respond_Gone();
		});
	}

	/*
		Serve a POST-only resource.
			Commonly used for creating things or causing side effects.
	*/
	template<class T> [[nodiscard]]
	std::shared_ptr<service> topic::serve_POST(
		std::weak_ptr<T> svc_object,
		status      (T::*svc_method)(pleb::request&))
	{
		return serve([m=svc_method, w=std::move(svc_object)](pleb::request &r)
		{
			if (auto s = w.lock()) switch (r.method())
			{
			case method::POST:
				r.respond((s.get()->*m)(r));
				break;
			case method::OPTIONS:
				r.respond_OK(method::POST | method::OPTIONS);
				break;
			default:
				r.respond_MethodNotAllowed();
			}
			else r.respond_Gone();
		});
	}

	template<class V>
	void topic::request(client_ref client, method method, V &&value)
	{
		pleb::request r(std::move(client), *this, method, std::forward<V>(value));
		issue(r);
	}

	template<class V>
	auto_request topic::request(method method, V &&value)
	{
		return auto_request(*this, method, std::forward<V>(value));
	}
	template<class V>
	auto_retrieve topic::retrieve(method method, V &&value)
	{
		return auto_retrieve(*this, method, std::forward<V>(value));
	}

	inline           auto_retrieve topic::GET   ()             {return retrieve(method::GET);}
	template<class V> auto_request topic::PUT   (V &&value)    {return request(method::PUT,   std::forward<V>(value));}
	template<class V> auto_request topic::POST  (V &&value)    {return request(method::POST,  std::forward<V>(value));}
	template<class V> auto_request topic::PATCH (V &&value)    {return request(method::PATCH, std::forward<V>(value));}
	inline            auto_request topic::DELETE()             {return request(method::DELETE);}

	inline void topic::issue(pleb::request &msg)
	{
		_resolve();

		msg.features &= ~flags::did_respond;

		auto svc = _base->service_lock();
		if (svc)
			if (svc->accepts(msg.filtering & ~flags::recursive))
				goto service_found;
			
		if (msg.recursive()) // Propagate up the chain.
			for (resource_node_ptr node = _base->parent(); node; node = node->parent())
				if (svc = node->service_lock())
					if (svc->accepts(msg.filtering | flags::recursive))
						goto service_found;

	//service_not_found:
		throw service_not_found("No service available", path());
		return;

	service_found:
		try
		{
			svc->func(msg);
		}
		catch (status s)               {msg.respond(s);}
		catch (status_exception &e)    {msg.respond(e.status);}

		// Default response if service did not respond or move message.
		if (!(msg.features & flags::did_respond))
			msg.respond(statuses::NoContent);

		msg.features |= flags::did_send;
	}

	/*
		PUBLISH using a prepared pleb::event object.
			pleb::event usually calls this method upon construction;
			it can be used to publish the same event repeatedly.
	*/
	inline void topic::publish(const pleb::event &msg)
	{
		_resolve();

		const bool recursive = msg.recursive();
		auto filtering = msg.filtering & ~flags::recursive;
		for (resource_node_ptr node = _base; node; node = node->parent())
		{
			for (subscription &sub : node->subscribers()) if (sub.accepts(filtering))
			{
				try {sub.func(msg);}
				catch (...)
				{
					auto exception = std::current_exception();
					if (msg.filtering & flags::subscriber_exception)
					{
						// Throw exception handler exceptions to the parent.
						if (resource_node_ptr parent = node->parent())
							topic(parent).publish(statuses::InternalServerError, exception,
								flags::subscriber_exception | flags::recursive,
								msg.requirements);
					}
					else
						topic(node).publish(statuses::InternalServerError, exception,
							flags::subscriber_exception | flags::recursive,
							msg.requirements);
					// TODO what if nobody handled the exception??  Unsafe to proceed?
				}
			}
			if (!recursive) break;
			filtering |= flags::recursive;
		}
	}


	/*
		"visit" entities within this resource, via callback.
			visit_resources invokes for this resource and each of its descendants.
			visit_services invokes for each service beneath this resource.
			visit_subscribers invokes for each service beneath this resource.

		callback        -- a functor accepting a shared_ptr to the visited item.
		recursion_depth -- how many generations of children to visit (0 = just this)

		Resources mainly exist in order to host services and subscribers, but may exist
			as a result of child resources, forced resolution or dangling references.
			Think of them as folders -- on their own, more suggestive than informative.


		WARNING: pending integration of a robust lock-free hashmap, these methods lock
			the branches of the resource tree in read mode.  This can lead to deadlock
			if the callback attempts to modify the resources it is traversing.
			Avoid performing complex actions within the callback.
	*/
	template<typename Callback, std::enable_if_t<std::is_invocable_v<Callback, const resource_node_ptr&>, int>>
	void topic::visit_resources(const Callback &callback, size_t recursion_depth, bool skip_this)
	{
		_realize();
		if (_subpath.length()) return;

		if (!skip_this) callback(_base);
		auto scan = [&](const std::string &, resource_node_ptr node)
		{
			callback(node);
			if (recursion_depth--) {topic(node).visit_resources(callback, recursion_depth-1, true);}
		};
		if (recursion_depth--) _base->visit_children(scan);
	}
	template<typename Callback, std::enable_if_t<std::is_invocable_v<Callback, service_ptr>, int>>
	void topic::visit_services(const Callback &callback, size_t recursion_depth)
	{
		visit_resources([&](const FIXME_ptr &rc)
			{if (auto svc=rc->service_lock()) callback(std::move(svc));},
			recursion_depth);
	}
	template<typename Callback, std::enable_if_t<std::is_invocable_v<Callback, subscription_ptr>, int>>
	void topic::visit_subscriptions(const Callback &callback, size_t recursion_depth)
	{
		visit_resources([&](const FIXME_ptr &rc)
			{for (auto i=rc->begin(),e=rc->end(); i!=e; ++i) callback(i);},
			recursion_depth);
	}
}

/*
	==============
	IMPLEMENTATION
	==============
*/

// Process a request.
inline void pleb::request::issue  ()    {topic.issue  (*this);}
inline void pleb::event  ::publish()    {topic.publish(*this); features |= flags::did_send;}

inline pleb::service::service(resource_node_ptr _resource, service_function &&_func,
	flags::filtering ignored, flags::handling handling)
	:
	receiver(ignored, handling), resource(std::move(_resource)),
	func(std::move(_func))
{
	topic(resource).publish(statuses::Created, this,
		flags::service_status      | flags::recursive);
}

inline pleb::subscription::subscription(resource_node_ptr _resource, subscriber_function &&_func,
	flags::filtering ignored, flags::handling handling)
	:
	receiver(ignored, handling), resource(std::move(_resource)),
	func(std::move(_func))
{
	topic(resource).publish(statuses::Created, this,
		flags::subscription_status | flags::recursive);
}

/*
	Publishing status messages from a destructor is likely to create stability issues.
		Instead, applications should hold weak pointers and actively monitor for expiration.
*/
inline pleb::service     ::~service()         {} //{resource->publish(statuses::Gone, this, flags::service_status      | flags::recursive);}
inline pleb::subscription::~subscription()    {} //{resource->publish(statuses::Gone, this, flags::subscription_status | flags::recursive);}
