#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <functional>

#include "flags.hpp"
#include "method.hpp"
#include "status.hpp"

#include "conversion.hpp"


namespace std
{
	template<class T> class future;
}

namespace coop
{
	template<typename T> class trie_;
}

namespace pleb
{
#define PLEB_ND [[nodiscard]] inline

	/*
		Resources are used to route all communications through PLEB.
			Pointers to resources may be stored to keep them alive and
			avoid the overhead of looking them up every time.
	*/
	class request;
	class auto_request;
	class auto_retrieve;
	class service;
	using service_ptr = std::shared_ptr<service>;
	using service_function = std::function<void(request&)>;

	class response;
	class client;
	using client_ptr = std::shared_ptr<client>;
	using response_function = std::function<void(response&)>;

	class event;
	class subscription;
	using subscription_ptr = std::shared_ptr<subscription>;
	using subscriber_function = std::function<void(const event&)>;

	class resource_data;
	using resource_node = coop::trie_<resource_data>;
	using resource_node_ptr = std::shared_ptr<resource_node>;

	resource_node_ptr global_root_resource() noexcept;

	/*
		client_ref is simply a shared_ptr to a client with additional constructors.
			Extended constructors make it more versatile as a function parameter.
	*/
	class client_ref : public std::shared_ptr<client>
	{
	public:
		// 1. No method for responding; response is discarded.
		constexpr explicit client_ref()          noexcept    {}
		constexpr          client_ref(nullptr_t) noexcept    {}

		// 2. Set the provided future to receive the response.
		template<typename T>
		client_ref(std::future<T> *f);

		// 3. Provide a callback function to handle the response.
		client_ref(response_function &&f);

		// 4. Pass a client object to handle the response.
		using client_ptr::client_ptr;
	};



	template<char Delimiter> class topic_view_;
	using topic_view = topic_view_<'/'>;

	/*
		Class for iterating over a topic name with a delimiting character.
			Leading, trailing and consecutive slashes will be ignored when iterating.
	*/
	template<char Delimiter>
	class topic_view_
	{
	public:
		struct end_tag {};

		class iterator
		{
		public:
			iterator()                                : _eos(0) {}
			iterator(std::string_view s)              : _eos(s.data()+s.length()), _sub(_consume(s.data())) {}
			iterator(std::string_view s, end_tag)     : _eos(s.data()+s.length()), _sub(_eos, 0) {}

			std::string_view operator* () const noexcept    {return  _sub;}
			std::string_view operator->() const noexcept    {return &_sub;}

			bool operator==(const iterator &o) const noexcept    {return _sub.data() == o._sub.data();}
			bool operator!=(const iterator &o) const noexcept    {return _sub.data() != o._sub.data();}
			bool operator< (const iterator &o) const noexcept    {return _sub.data() <  o._sub.data();}
			bool operator<=(const iterator &o) const noexcept    {return _sub.data() <= o._sub.data();}
			bool operator> (const iterator &o) const noexcept    {return _sub.data() >  o._sub.data();}
			bool operator>=(const iterator &o) const noexcept    {return _sub.data() >= o._sub.data();}

			void operator++() noexcept    {_sub = _consume(_sub.data()+_sub.length());}

		private:
			const char      *_eos;
			std::string_view _sub;

			std::string_view _consume(const char *p) noexcept
			{
				while (p < _eos && *p == Delimiter) {++p;}
				auto e = p;
				while (e < _eos && *e != Delimiter) {++e;}
				return std::string_view(p, e-p);
			}
		};

		using const_iterator = iterator;


	public:
		topic_view_(std::string_view s)   noexcept    : string(s) {}
		topic_view_(const std::string &s) noexcept    : string(s) {}
		topic_view_(const char *cstr)     noexcept    : string(cstr) {}

		iterator begin() const noexcept    {return iterator(string);}
		iterator end  () const noexcept    {return iterator(string, end_tag {});}

		// Returns true if the string begins with a delimiter.
		bool is_absolute() const noexcept    {return string.length() && string[0] == Delimiter;}


		// Produce a string_view removing the last path segment.
		//   May leave trailing slashes if the parent is not a blank.
		std::string_view parent() const noexcept
		{
			const char *p = string.data();
			for (auto &i : *this) p = i.data();
			return string.substr(0, p - string.data());
		}

		std::string_view last_id() const noexcept
		{
			std::string_view id;
			for (auto &i : *this) id = i;
			return id;
		}


	public:
		std::string_view string;
	};



	namespace detail
	{
		template<typename Base>
		class topic_exception : public Base
		{
		public:
			topic_exception(std::string_view preamble, const std::string& topic)    : Base(preamble.data() + (": " + topic)) {}
			topic_exception(std::string_view preamble, topic_view topic)            : topic_exception(preamble, topic.string) {}
			topic_exception(std::string_view preamble, std::string_view topic)      : topic_exception(preamble, std::string(topic)) {}
			topic_exception(std::string_view preamble, const char* topic)           : topic_exception(preamble, std::string(topic)) {}
		};
		using topic_runtime_error = topic_exception<std::runtime_error>;
		using topic_logic_error = topic_exception<std::logic_error>;
	}


	/*
		The topic class exists in two variants with the same API and behavior.
			These are interchangeable and differ only in performance.

		topic      -- points directly to the resource, which is created if it does not exist.
		topic_path -- points to the nearest resource, holding any leftover path as a string.

		"topic" is the right choice in most cases and is cheaper to pass around.
		"topic_path" is used in messages and is preferred when the resource path includes trailing
			elements (IDs, keys, etc) which don't directly correspond to services or subscribers.
	*/
	template<class SubPath> class topic_;

	using topic      = topic_<void>;

	using topic_path = topic_<std::string>;


	// Internal datamodel for the topic classes.
	template<class SubPath>
	class topic_base_
	{
		static_assert(std::is_same_v<SubPath,void> || std::is_same_v<SubPath,std::string>,
			"topic_base_ must use void or std::string as a template parameter.");
	};
	template<> class topic_base_<void>
	{
	public:
		topic_base_()  : _node(global_root_resource()) {}
		~topic_base_() = default;

		topic_base_(const resource_node_ptr &node)                              : _node(node ? node : global_root_resource()) {}
		topic_base_(const resource_node_ptr &node, std::string_view subpath)    : _node(node ? node : global_root_resource()) {_push(subpath);}
		topic_base_(                               std::string_view    path)    : _node(global_root_resource())               {_push(   path);}


	protected:
		template<class P> friend class topic_base_;
		resource_node_ptr _node; // Never null.

	protected:
		constexpr bool             _is_resolved() const    {return true;}
		constexpr std::string_view _unresolved()  const    {return std::string_view();}
		const resource_node_ptr   &_nearest_node() const    {return _node;}

		const topic_base_&         _resolve()     const    {return *this;}
		topic_base_&               _resolve()              {return *this;}
		const resource_node_ptr   &_realize()     const    {return _node;}

		void             _push(topic_view subpath);
		void             _pop () noexcept;
		std::string_view _back() const noexcept;
		std::string      _path() const;
	};
	template<> class topic_base_<std::string>
	{
	public:
		topic_base_()  : _nearest(global_root_resource()) {}
		~topic_base_() = default;

		topic_base_(const resource_node_ptr &node)                              : _nearest(node ? node : global_root_resource()) {}
		topic_base_(const resource_node_ptr &node, std::string_view subpath)    : _nearest(node ? node : global_root_resource()) {_push(subpath); _resolve();}
		topic_base_(                               std::string_view    path)    : _nearest(global_root_resource())               {_push(   path); _resolve();}

		// Conversion with pathless topic
		topic_base_(const topic_base_<void> &o)    : _nearest(o._node) {}
		topic_base_(topic_base_<void>      &&o)    : _nearest(std::move(o._node)) {}
		operator topic_base_<void>() const &       {return topic_base_<void>(          _nearest,  _subpath);}
		operator topic_base_<void>() &&            {return topic_base_<void>(std::move(_nearest), _subpath);}


	protected:
		template<class P> friend class topic_base_;
		resource_node_ptr _nearest; // Never null.
		std::string       _subpath; // Never has extra slashes.  Resolved at construction time or by resolve().

	protected:
		bool                     _is_resolved()  const    {return _subpath.length();}
		std::string_view         _unresolved()   const    {return _subpath;}
		const resource_node_ptr &_nearest_node() const    {return _nearest;}

		topic_base_&             _resolve()       noexcept    {_subpath = _resolve_with(_subpath); return *this;}
		topic_base_              _resolve() const noexcept    {auto tmp = *this; tmp._resolve(); return tmp;}
		const resource_node_ptr &_realize();
		resource_node_ptr        _realize() const             {auto tmp = *this; tmp._realize(); return std::move(tmp._nearest);}

		void             _push(topic_view subpath);
		void             _pop () noexcept;
		std::string_view _back() const noexcept;
		std::string      _path() const;

	private:
		std::string _resolve_with(std::string_view new_subpath) noexcept;
	};


	/*
		Topics form a global hierarchy (trie) to which 
	*/
	template<class SubPath>
	class topic_ :
		public topic_base_<SubPath>
	{
	public:
		using base_t = topic_base_<SubPath>;

		static constexpr bool type_is_topic_path = std::is_same_v<SubPath, std::string>;

		using other_topic_t = std::conditional_t<type_is_topic_path, topic, topic_path>;

		static_assert(std::is_same_v<SubPath,void> || std::is_same_v<SubPath,std::string>,
			"topic_base_ must use void or std::string as a template parameter.");


	protected:
		/*std::string _resolve(topic_view subpath);
		void        _resolve()    {if (_subpath.length()) _subpath = _resolve(_subpath);}
		void        _realize(topic_view subpath);
		void        _realize()    {if (_subpath.length()) _realize(_subpath); _subpath.clear();}*/


		topic_(base_t      &&base)    : base_t(std::move(base)) {}
		topic_(const base_t &base)    : base_t(base) {}


	public:
		using subscription = pleb::subscription;
		using service      = pleb::service;


	public:
		// Access the root resource.  (TODO allocate statically?)
		
		static topic_ root() noexcept    {return topic_(pleb::root_node());}


		topic_()  : base_t(nullptr) {}
		~topic_() = default;

		// Look up a resource within the given resource node.
		topic_(resource_node_ptr node)                              : base_t(std::move(node)) {}
		topic_(resource_node_ptr node, topic_view       subpath)    : base_t(std::move(node), subpath.string) {}
		topic_(resource_node_ptr node, std::string      subpath)    : base_t(std::move(node), std::string_view(subpath)) {}
		topic_(resource_node_ptr node, std::string_view subpath)    : base_t(std::move(node), std::string_view(subpath)) {}
		topic_(resource_node_ptr node, const char      *subpath)    : base_t(std::move(node), std::string_view(subpath)) {}

		// Look up a resource by its path.
		topic_(topic_view         topic)                            : base_t(topic.string) {}
		topic_(const std::string& topic)                            : base_t(std::string_view(topic)) {}
		topic_(std::string_view   topic)                            : base_t(std::string_view(topic)) {}
		topic_(const char*        topic)                            : base_t(std::string_view(topic)) {}


		// Convert between topic and topic_path.
		topic_(const other_topic_t &other)               : base_t(other) {}
		topic_(other_topic_t &&other)                    : base_t(std::move(other)) {}
		topic_& operator=(const other_topic_t &other)    {static_cast<base_t&>(*this) = other;}
		topic_& operator=(other_topic_t &&other)         {static_cast<base_t&>(*this) = std::move(other);}



		// Access a child topic.
		topic_  operator/   (topic_view subpath) const    {auto copy = *this; copy._push(subpath); return copy;}
		topic_& operator/=  (topic_view subpath)          {base_t::_push(subpath); return *this;}
		topic_  child       (topic_view subpath) const    {auto copy = *this; copy._push(subpath); return copy;}
		topic_& set_to_child(topic_view subpath)          {base_t::_push(subpath); return *this;}

		// Access the parent topic.
		//    The root is treated as its own parent.
		topic_ parent()        const       {auto copy = *this; copy._pop(); return copy;}
		void   set_to_parent() noexcept    {base_t::_pop();}


		// Get the leaf identifier of this topic, or its full path.
		//   id() returns "[root]" for the root node.
		//   path_view() returns the path in up to two pieces and does not simplify slashes.
		//   path() returns the path in one piece and does not simplify slashes.
		std::string_view                id       () const noexcept    {return base_t::_back();}
		//std::array<std::string_view, 2> path_view() const noexcept;
		std::string                     path     () const             {return base_t::_path();}


		/*
			Optimization methods for topic_path:
				Resolve to the most specific existing resource.
				These methods won't affect behavior of this class,
				but may avoid repetitive resolve operations when sending messages.

			These methods may be called on topic but do nothing.
			The resource can be forced into existence by converting topic_path to topic.
		*/
		topic_& resolve() noexcept    {_resolve(); return *this;}
		topic_  resolved() const      {auto copy = *this; copy._resolve(); return copy;}


		/*
			SUBSCRIBE to a resource.
				Subscribers receive subsequent reports to the resource
				and the children/descendants of the resource (not counting aliases).
		*/
		[[nodiscard]] std::shared_ptr<subscription> subscribe(
			subscriber_function &&f,
			flags::filtering      ignore_flags = flags::default_subscriber_ignore,
			flags::handling       handling     = flags::no_special_handling);

		/*
			Subscribe to a resource via calls to a method of some object.
		*/
		template<class T> [[nodiscard]]
		std::shared_ptr<subscription> subscribe(
			std::weak_ptr<T> handler_object,
			void        (T::*handler_method)(const pleb::event&),
			flags::filtering ignore_flags = flags::default_subscriber_ignore,
			flags::handling  handling     = flags::no_special_handling);


		/*
			PUBLISH an event.
				This will be visible to all subscribers to a resource,
				and the parents/ancestors of the resource.
		*/
		template<typename T = std_any::any>
		void publish(
			pleb::status     status    = statuses::OK,
			T              &&item      = {},
			flags::filtering filtering = flags::default_message_filtering,
			flags::handling  handling  = flags::no_special_handling) const;


		/*
			SERVE this resource.
				Subsequent events on this resource will be passed to the function.
				If a service already exists here, this function will fail, returning null.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(service_function &&function) noexcept;

		/*
			SERVE this resource, with some automatic glue code to make life easier.
			
			Refer to bind_service(...) in bind.hpp for possible arguments.
				bind.hpp should be included to enable these overloads.
		*/
		template<typename ... Args,    typename Valid = std::void_t<decltype(bind_service(std::declval<Args>()...))>>
		std::shared_ptr<service> serve(Args&& ... args)
		{
			return serve(bind_service(std::forward<Args>(args...)));
		}

#if 0
		/*
			Serve a resource using calls to a method of some object.
				The weak pointer is locked whenever the service is called.
				If the service pointer outlives the object pointer, it will respond with "GONE".
		*/
		template<class T> [[nodiscard]]
		std::shared_ptr<service> serve(
			std::weak_ptr<T> svc_object,
			void        (T::*svc_method)(pleb::request&));

		/*
			Serve a POST-only resource.
				Commonly used for creating things or causing side effects.
		*/
		template<class T> [[nodiscard]]
		std::shared_ptr<service> serve_POST(
			std::weak_ptr<T> svc_object,
			status      (T::*svc_method)(pleb::request&));
#endif


		/*
			REQUEST something from this resource, providing a client for responding.
				client_ref may be a client_ptr or a std::future.
				The response target will receive a response, now or later.
				If there is no service, pleb::service_not_found is thrown.
		*/
		template<class V = std::any>
		void request(client_ref client, method method, V &&value = {}) const;

		/* */                        void GET   (client_ref c)                 const    {return request(c, method::GET);}
		template<class V = std::any> void PUT   (client_ref c, V &&value)      const    {return request(c, method::PUT,    std::forward<V>(value));}
		template<class V = std::any> void POST  (client_ref c, V &&value = {}) const    {return request(c, method::POST,   std::forward<V>(value));}
		template<class V = std::any> void PATCH (client_ref c, V &&value)      const    {return request(c, method::PATCH,  std::forward<V>(value));}
		/* */                        void DELETE(client_ref c)                 const    {return request(c, method::DELETE);}

		/*
			Convenience API for requests.

			The returned request may be issued (sent) by:
			- calling async<T> or converting to std::future<T>
			- calling await<T>
			- calling push() or issue(client) -- auto_retrieve only.
		*/
		template<class V = std::any>
		auto_request request(method method, V &&value = {}) const;
		template<class V = std::any>
		auto_retrieve retrieve(method method, V &&value = {}) const;

		/* */                       auto_retrieve GET   ()               const;
		template<class V = std::any> auto_request PUT   (V &&value)      const;
		template<class V = std::any> auto_request POST  (V &&value = {}) const;
		template<class V = std::any> auto_request PATCH (V &&value)      const;
		/* */                        auto_request DELETE()               const;

#if 0
		/*
			PUSH a request to a resource.
				There is no mechanism for a direct reply (this may improve performance).
				If there is no service, pleb::service_not_found is thrown.
		*/
		template<class V = std::any>
		void push(method method, V &&value = {})
		{
			request(nullptr, method, std::forward<V>(value));
		}

		template<class V = std::any> void push_PUT   (V &&value)         {push(method::PUT,   std::forward<V>(value));}
		template<class V = std::any> void push_POST  (V &&value = {})    {push(method::POST,  std::forward<V>(value));}
		template<class V = std::any> void push_PATCH (V &&value)         {push(method::PATCH, std::forward<V>(value));}
		/* */                        void push_DELETE()                  {push(method::DELETE);}
#endif


		/*
			REQUEST using a prepared pleb::request object.
				pleb::request usually calls this method upon construction;
				it can be used to issue the same request repeatedly.
		*/
		void issue(pleb::request &msg) const;

		/*
			PUBLISH using a prepared pleb::event object.
				pleb::event usually calls this method upon construction;
				it can be used to publish the same event repeatedly.
		*/
		void publish(const pleb::event &msg) const;


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
		template<typename Callback,        std::enable_if_t<std::is_invocable_v<Callback, const resource_node_ptr&>, int> SFINAE = 0>
		void visit_resources(
			const Callback &callback,
			size_t recursion_depth = 255,
			bool skip_this = false) const;

		template<typename Callback,        std::enable_if_t<std::is_invocable_v<Callback, service_ptr>, int> SFINAE = 0>
		void visit_services(
			const Callback &callback,
			size_t recursion_depth = 255) const;

		template<typename Callback,        std::enable_if_t<std::is_invocable_v<Callback, subscription_ptr>, int> SFINAE = 0>
		void visit_subscriptions(
			const Callback &callback,
			size_t recursion_depth = 255) const;


	protected:
		void _publish_exception(const pleb::event&, const subscription&, std::exception_ptr) const;
	};


	namespace detail
	{
		// For now, we use a global conversion table.
		inline const std::shared_ptr<conversion_table> &global_conversion_table()
		{
			static auto t = std::make_shared<conversion_table>();
			return t;
		}
	}
}
