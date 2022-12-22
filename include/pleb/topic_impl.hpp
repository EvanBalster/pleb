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
		static resource_node_ptr root = resource_node::create("[root]"); return root;
	}


	inline void topic_base_<void>::_push(topic_view subpath)
	{
		for (auto part : subpath) _node = _node->get_child(part);
	}

	inline void topic_base_<void>::_pop() noexcept
	{
		_node = _node->parent();
	}

	inline void topic_base_<std::string>::_push(topic_view addition)
	{
		for (auto part : addition)
		{
			if (_subpath.length()) _subpath.push_back('/');
			_subpath.append(part.data(), part.length());
		}
	}

	inline void topic_base_<std::string>::_pop() noexcept
	{
		if (_subpath.length())
		{
			auto parent = topic_view(_subpath).parent();
			_subpath.assign(parent.data(), parent.length());
		}
		else _nearest = _nearest->parent();
	}

	inline std::string_view topic_base_<void>::_back() const noexcept
	{
		return _node->id();
	}
	inline std::string_view topic_base_<std::string>::_back() const noexcept
	{
		if (_subpath.length()) return topic_view(_subpath).last_id();
		else                   return _nearest->id();
	}


	inline std::string topic_base_<std::string>::_resolve_with(std::string_view subpath) noexcept
	{
		std::string unresolved;
		for (auto part : topic_view(subpath))
		{
			if (auto child = _nearest->try_child(part)) _nearest = std::move(child);
			else {unresolved = subpath.substr(part.data()-subpath.data()); break;}
		}
		return unresolved;
	}
	inline const resource_node_ptr &topic_base_<std::string>::_realize()
	{
		for (auto part : topic_view(_subpath)) _nearest = _nearest->get_child(part);
		_subpath.clear();
	}

	/*inline std::array<std::string_view, 2> topic::path_view() const noexcept
	{
		return {_base->path(), 
	}*/

	inline std::string topic_base_<void>::_path() const
	{
		return _node->path();
	}

	inline std::string topic_base_<std::string>::_path() const
	{
		std::string result = _nearest->path();
		if (_subpath.length())
		{
			if (result.length()) result.push_back('/');
			result.append(_subpath);
		}
		return result;
	}
	
	template<typename P> [[nodiscard]]
	std::shared_ptr<subscription> topic_<P>::subscribe(
		subscriber_function &&f,
		flags::filtering      ignore_flags,
		flags::handling       handling)
	{
		auto &node = _realize();
		return node->emplace_subscriber(node, std::move(f)); // TODO ignore_flags, handling
	}

	template<typename P>
	template<class T> [[nodiscard]]
	inline std::shared_ptr<subscription> topic_<P>::subscribe(
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

	template<typename P>
	template<typename T>
	void topic_<P>::publish(
		status           status,
		T              &&item,
		flags::filtering filtering,
		flags::handling  handling) const
	{
		pleb::event e(*this, status, std::forward<T>(item), filtering, handling);
		publish(e);
	}

	template<typename P> [[nodiscard]]
	std::shared_ptr<service> topic_<P>::serve(service_function &&function) noexcept
	{
		auto &node = _realize();
		return node->try_emplace_service(node, std::move(function));
	}

#if 0
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
#endif

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
		const topic_<P>  &target = base_t::_resolve();
		resource_node_ptr node   = target._nearest_node();

		// Mark the message as unresponded.
		msg.features &= ~flags::did_respond;

		const bool recursive = msg.recursive();
		auto filtering = msg.filtering & ~flags::recursive;

		if (target._is_resolved()) goto start_resolved;

		while (recursive && node)
		{
			filtering |= flags::recursive;

		start_resolved:
			if (auto svc = node->service_lock()) if (svc->accepts(filtering))
			{
				try                            {svc->func(msg);}
				catch (status s)               {msg.respond(s);}
				catch (status_exception &e)    {msg.respond(e.status);}

				// Default response if service did not respond or move message.
				if (!(msg.features & flags::did_respond)) msg.respond(statuses::NoContent);
				
				msg.features |= flags::did_send;
				return;
			}

			node = node->parent();
		}

		throw service_not_found("No service available", path());
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

		const bool recursive = msg.recursive();
		auto filtering = msg.filtering & ~flags::recursive;
		
		if (target._is_resolved()) goto start_resolved;

		while (recursive && node)
		{
			filtering |= flags::recursive;

		start_resolved:
			for (subscription &sub : node->subscribers()) if (sub.accepts(filtering))
			{
				try            {sub.func(msg);}
				catch (...)    {_publish_exception(msg, sub, std::current_exception());}
			}

			node = node->parent();
		}
	}

	template<typename P>
	void topic_<P>::_publish_exception(
		const pleb::event  &msg,
		const subscription &sub,
		std::exception_ptr  exception) const
	{
		auto node = sub.resource;

		if (msg.filtering & flags::subscriber_exception)
		{
			// If an exception subscriber throws an exception, publish to the parent instead.
			if (resource_node_ptr parent = node->parent())
				topic(parent).publish(statuses::InternalServerError, exception,
					flags::subscriber_exception | flags::recursive, msg.requirements);
		}
		else
			topic(node).publish(statuses::InternalServerError, exception,
				flags::subscriber_exception, msg.requirements);

		// TODO what if nobody handled the exception??  Unsafe to proceed?
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
	template<typename P>
	template<typename Callback, std::enable_if_t<std::is_invocable_v<Callback, const resource_node_ptr&>, int>>
	void topic_<P>::visit_resources(const Callback &callback, size_t recursion_depth, bool skip_this) const
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
	template<typename P>
	template<typename Callback, std::enable_if_t<std::is_invocable_v<Callback, service_ptr>, int>>
	void topic_<P>::visit_services(const Callback &callback, size_t recursion_depth) const
	{
		visit_resources([&](const FIXME_ptr &rc)
			{if (auto svc=rc->service_lock()) callback(std::move(svc));},
			recursion_depth);
	}
	template<typename P>
	template<typename Callback, std::enable_if_t<std::is_invocable_v<Callback, subscription_ptr>, int>>
	void topic_<P>::visit_subscriptions(const Callback &callback, size_t recursion_depth) const
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
