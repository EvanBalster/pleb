#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <functional>
#include <iosfwd>
#include <future>

#ifdef PLEB_REPLACEMENT_ANY_HEADER
	#include PLEB_REPLACEMENT_ANY_HEADER
#else
	#include <any>
#endif

#include "flags.hpp"
#include "method.hpp"
#include "status.hpp"

#include "conversion.hpp"

namespace coop
{
	template<typename T> class trie_;
}

namespace pleb
{
	#ifdef PLEB_REPLACEMENT_ANY_NAMESPACE
		namespace std_any = ::PLEB_REPLACEMENT_ANY_NAMESPACE;
	#else
		namespace std_any = ::std;
	#endif
	
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

	class service_relay;
	using service_relay_ptr = std::shared_ptr<service_relay>;

	class response;
	class client;
	using client_ptr = std::shared_ptr<client>;
	using response_function = std::function<void(response&)>;

	class event;
	class subscription;
	using subscription_ptr = std::shared_ptr<subscription>;
	using subscriber_function = std::function<void(const event&)>;

	class event_relay;
	using event_relay_ptr = std::shared_ptr<event_relay>;

	class resource_data;
	using resource_node = coop::trie_<resource_data>;
	using resource_node_ptr = std::shared_ptr<resource_node>;

	resource_node_ptr global_root_resource() noexcept;

	class bound_service_function;
	

	/*
		client_ref is simply a shared_ptr to a client with additional constructors.
			Extended constructors make it more versatile as a function parameter.
	*/
	class client_ref : public std::shared_ptr<client>
	{
	public:
		// 1. No method for responding; response is discarded.
		constexpr explicit client_ref()               noexcept    {}
		constexpr          client_ref(std::nullptr_t) noexcept    {}

		// 2. Set the provided future to receive the response.
		template<typename T>
		client_ref(std::future<T> *f);

		// 3. Provide a callback function to handle the response.
		client_ref(response_function &&f);

		// 4. Pass a client object to handle the response.
		client_ref(std::shared_ptr<client>      &&ptr)    : shared_ptr(std::move(ptr)) {}
		client_ref(const std::shared_ptr<client> &ptr)    : shared_ptr(ptr)            {}
		using shared_ptr::shared_ptr;
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

			explicit operator bool() const noexcept    {return _sub.data() != _eos;}

			const std::string_view& operator* () const noexcept    {return  _sub;}
			const std::string_view* operator->() const noexcept    {return &_sub;}

			bool operator==(const iterator &o) const noexcept    {return _sub.data() == o._sub.data();}
			bool operator!=(const iterator &o) const noexcept    {return _sub.data() != o._sub.data();}
			bool operator< (const iterator &o) const noexcept    {return _sub.data() <  o._sub.data();}
			bool operator<=(const iterator &o) const noexcept    {return _sub.data() <= o._sub.data();}
			bool operator> (const iterator &o) const noexcept    {return _sub.data() >  o._sub.data();}
			bool operator>=(const iterator &o) const noexcept    {return _sub.data() >= o._sub.data();}

			iterator& operator++() noexcept    {_sub = _consume(_sub.data()+_sub.length()); return *this;}

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


		// Produce a string_view removing the last path segment (and preceding slash if any)
		std::string_view parent() const noexcept
		{
			size_t p = 0;
			for (auto &i : *this) p = i.data() - string.data();
			return string.substr(0, p ? p-1 : 0);
		}

		std::string_view last_id() const noexcept
		{
			std::string_view id;
			for (auto i : *this) id = i;
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
		This exception may be thrown when using a null-valued pleb::topic
			to send a message, set up a receiver or convert to topic_path.
	*/
	class null_topic_error : public detail::topic_runtime_error
	{
	public:
		using detail::topic_runtime_error::topic_runtime_error;

		static const resource_node_ptr &check(
			const resource_node_ptr &p,
			const char *preamble = "null topic not allowed",
			const char *topic_name = "(null resource_node_ptr)")
		{
			if (!p.get()) throw null_topic_error(preamble, topic_name);
			return p;
		}
	};



	// Tag type used to distinguish topic_path from topic below.
	struct lazy_path {};

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

	using topic_path = topic_<lazy_path>;


	// Internal datamodel for the topic classes.
	template<class SubPath>
	class topic_base_
	{
		static_assert(std::is_same_v<SubPath,void> || std::is_same_v<SubPath,lazy_path>,
			"topic_base_ must use void or pleb::lazy_path as a template parameter.");
	};
	template<> class topic_base_<void>
	{
	public:
		topic_base_()  : _node(nullptr) {}
		~topic_base_() = default;

		topic_base_(                               std::string_view    path) noexcept    : _node(global_root_resource()) {_push(   path);}

		explicit operator bool() const noexcept    {return bool(_node);}

		bool operator==(const topic_base_ &other) const noexcept    {return _node == other._node;}
		bool operator!=(const topic_base_ &other) const noexcept    {return _node == other._node;}


	protected:
		topic_base_(const resource_node_ptr &node)                           noexcept    : _node(node)                   {}
		topic_base_(const resource_node_ptr &node, std::string_view subpath) noexcept    : _node(node)                   {_push(subpath);}

		template<class P> friend class topic_base_;
		template<class P> friend class topic_; // visit_subscriptions
		resource_node_ptr _node; // Never null.

	protected:
		constexpr bool             _is_resolved () const noexcept    {return true;}
		bool                       _is_null     () const noexcept    {return !_node;}
		constexpr std::string_view _unresolved  () const noexcept    {return std::string_view();}
		const resource_node_ptr   &_nearest_node() const noexcept    {return _node;}

		const topic_base_&         _resolve()     const    {return *this;}
		topic_base_&               _resolve()              {return *this;}
		const resource_node_ptr   &_realize()     const    {return _node;}

		void             _push(topic_view subpath);
		void             _pop ();
		std::string_view _back() const noexcept;
		std::string_view _view() const;
	};
	template<> class topic_base_<lazy_path>
	{
	public:
		topic_base_()  : _nearest(global_root_resource()) {}
		~topic_base_() = default;

		topic_base_(                               std::string_view    path)    : _nearest(global_root_resource()) {_push(   path); _resolve();}

		// Conversion with pathless topic
		topic_base_(const topic_base_<void> &o);
		topic_base_(topic_base_<void>      &&o);
		operator topic_base_<void>() const &       {return topic_base_<void>(_realize());}
		operator topic_base_<void>() &&            {_realize(); return topic_base_<void>(std::move(_nearest));}

		bool operator==(const topic_base_       &other) const    {return _path == other._path;}
		bool operator!=(const topic_base_       &other) const    {return _path != other._path;}
		

	protected:
		topic_base_(const resource_node_ptr &node);
		topic_base_(const resource_node_ptr &node, std::string_view subpath);

		template<class P> friend class topic_base_;
		template<class P> friend class topic_;  // visit_subscriptions
		resource_node_ptr _nearest; // Never null.
		std::string       _path;    // Complete path, never has extra slashes.

	protected:
		constexpr bool           _is_null     () const noexcept    {return false;}
		bool                     _is_resolved()  const noexcept;
		std::string_view         _unresolved()   const noexcept;
		const resource_node_ptr &_nearest_node() const noexcept    {return _nearest;}

		topic_base_&             _resolve()       noexcept;
		topic_base_              _resolve() const noexcept    {auto tmp = *this; tmp._resolve(); return tmp;}
		const resource_node_ptr &_realize();
		resource_node_ptr        _realize() const             {auto tmp = *this; tmp._realize(); return std::move(tmp._nearest);}

		void             _push(topic_view subpath);
		void             _pop () noexcept;
		std::string_view _back() const noexcept    {return topic_view(_path).last_id();}
		std::string_view _view() const             {return _path;}
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

		static constexpr bool type_can_be_null   = std::is_same_v<SubPath, void>;
		static constexpr bool type_is_topic_path = std::is_same_v<SubPath, lazy_path>;

		using other_topic_t = std::conditional_t<type_is_topic_path, topic, topic_path>;

		static_assert(std::is_same_v<SubPath,void> || std::is_same_v<SubPath,lazy_path>,
			"topic_base_ must use void or pleb::lazy_path as a template parameter.");


	protected:
		topic_(base_t      &&base)    : base_t(std::move(base)) {}
		topic_(const base_t &base)    : base_t(base) {}


	public:
		using subscription = pleb::subscription;
		using service      = pleb::service;


	public:
		// Access the root resource.  (TODO allocate statically?)
		
		static topic_ root() noexcept    {return topic_(pleb::global_root_resource());}


		topic_()  = default;
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
		topic_(const other_topic_t &other)                   : base_t(other) {}
		topic_(other_topic_t &&other)            noexcept    : base_t(std::move(other)) {}
		topic_& operator=(const other_topic_t &other)        {static_cast<base_t&>(*this) = other;            return *this;}
		topic_& operator=(other_topic_t &&other) noexcept    {static_cast<base_t&>(*this) = std::move(other); return *this;}

		bool operator==(const other_topic_t &&other) const noexcept    {return path() == other.path();}
		bool operator!=(const other_topic_t &&other) const noexcept    {return path() != other.path();}



		// Access a child topic.
		topic_  operator/   (topic_view subpath) const    {auto copy = *this; copy._push(subpath); return copy;}
		topic_& operator/=  (topic_view subpath)          {base_t::_push(subpath); return *this;}
		topic_  child       (topic_view subpath) const    {auto copy = *this; copy._push(subpath); return copy;}
		topic_& set_to_child(topic_view subpath)          {base_t::_push(subpath); return *this;}

		/*
			Access the parent topic.
				When using topic, the parent of a root topic is null.
				When using topic_path, the parent of a root topic_path is itself.
		*/
		topic_ parent()        const noexcept(noexcept(base_t()._pop()))    {auto copy = *this; copy._pop(); return copy;}
		void   set_to_parent()       noexcept(noexcept(base_t::_pop()))     {base_t::_pop();}


		/*
			Check if another topic is an ancestor of this one.
		*/
		template<typename T>
		bool is_ancestor_of(topic_<T> other) const noexcept
		{
			size_t len = path().length();
			while (true)
			{
				auto olen = other.path().length();
				if (olen < len) return false;
				if (olen == len) return path() == other.path();
				other._pop();
			}
		}
		template<typename T>
		bool is_descendant_of(const topic_<T> &other) const noexcept    {return other.is_ancestor_of(*this);}


		/*
			Get the leaf identifier of this topic, or its full path.
				id() returns the last part of the topic path.
				path() returns the complete path, with no redundant slashes.
		*/
		std::string_view id  () const noexcept    {return base_t::_back();}
		std::string_view path() const             {return base_t::_view();}


		/*
			Optimization methods for topic_path:
				Resolve to the most specific existing resource.
				These methods won't affect behavior of this class,
				but may avoid repetitive resolve operations when sending messages.

			These methods may be called on topic but do nothing.
			The resource can be forced into existence by converting topic_path to topic.
		*/
		topic_& resolve() noexcept    {this->_resolve(); return *this;}
		topic_  resolved() const      {auto copy = *this; copy._resolve(); return copy;}


		/*
			SUBSCRIBE to a resource.
				Subscribers receive subsequent reports to the resource
				and the children/descendants of the resource (not counting aliases).
		*/
		[[nodiscard]] std::shared_ptr<subscription> subscribe(
			subscriber_function &&handler,
			subscription_config   flags = {});

		/*
			Subscribe to a resource via calls to a method of some object.
		*/
		template<class T> [[nodiscard]]
		std::shared_ptr<subscription> subscribe(
			std::weak_ptr<T> handler_object,
			void        (T::*handler_method)(const pleb::event&),
			subscription_config flags = {});


		/*
			PUBLISH an event.
				This will be visible to all subscribers to a resource,
				and the parents/ancestors of the resource.
		*/
		template<typename T = std_any::any>
		void publish(
			pleb::status     status    = statuses::OK,
			T              &&item      = {},
			message_flags    flags     = {}) const;


		/*
			Create a subscription which re-publishes events to another topic.
				Forwarding will continue as long as the returned pointer is held.

			Throws an exception if destination is a child of this topic,
				unless the forwarder is configured to ignore recursive events.
		*/
		std::shared_ptr<event_relay> forward_events(
			topic_path          destination_topic,
			subscription_config flags = {});



		/*
			SERVE this resource.
				Subsequent events on this resource will be passed to the function.
				If a service already exists here, this function will fail, returning null.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(
			service_function &&handler,
			service_config      flags = {}) noexcept;


		/*
			SERVE this resource, with some automatic glue code to make life easier.

			The service_binding type is designed solely for use in functions like this.
				Enclose arguments in {braces} and they will be passed to bind_service(...).
				This allows a service function to wrap a weak pointer and class method.
				See bind.hpp for available bindings and their arguments.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(
			bound_service_function handler,
			service_config         flags = {});


		/*
			Create a service which forwards requests to another topic.
				Forwarding will continue as long as the returned pointer is held.
				Responses will refer to the service topic, not the forwarding one.

			If flags permit the forwarder to accept recursive messages,
				the destination topic must not be a child of this topic.
		*/
		std::shared_ptr<service_relay> forward_requests(
			topic          service_topic,
			service_config flags = {});


		/*
			REQUEST something from this resource, providing a client for responding.
				client_ref may be a client_ptr or a std::future.
				The response target will receive a response, now or later.
				If there is no service, pleb::service_not_found is thrown.
		*/
		template<class V = std::any>
		void request(client_ref client, method method, V &&value = {}) const;

		/* */                        void GET   (client_ref c)                 const    {return request(c, method::GET);}
		/* */                        void HEAD  (client_ref c)                 const    {return request(c, method::HEAD);}
		/* */                        void OPTIONS(client_ref c)                const    {return request(c, method::OPTIONS);}
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
		/* */                       auto_retrieve HEAD  ()               const;
		/* */                       auto_retrieve OPTIONS()              const;
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
			Get the service, if any, at this specific topic.
				This can be used to check if serve() would fail.
			
			Note: requests can be handled by recursive services in a parent topic
				even if a topic provides no current_service.
		*/
		service_ptr current_service() const noexcept;

		/*
			Find the service which will respond when making a request to this topic.
				This can be used to check if a request would instantly fail.
				This method is also invoked when making a request.
		*/
		service_ptr find_service(flags::filtering filtering = flags::default_message_filtering) const noexcept;


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
		template<typename Callback>
		auto visit_resources(
			const Callback &callback,
			size_t recursion_depth = 255,
			bool skip_this = false)       const    -> decltype(callback(std::declval<topic>()), void());

		template<typename Callback>
		auto visit_services(
			const Callback &callback,
			size_t recursion_depth = 255) const    -> decltype(callback(std::declval<service_ptr>()), void());

		template<typename Callback>
		auto visit_subscriptions(
			const Callback &callback,
			size_t recursion_depth = 255) const    -> decltype(callback(std::declval<subscription_ptr>()), void());


	protected:
		template<typename P> friend class topic_;
		void _publish_exception(const pleb::event&, const subscription&, std::exception_ptr) const;
	};


	
	inline bool operator==(const topic &lhs, const topic_path &rhs) noexcept    {return lhs.path() == rhs.path();}
	inline bool operator!=(const topic &lhs, const topic_path &rhs) noexcept    {return lhs.path() != rhs.path();}
	inline bool operator==(const topic_path &lhs, const topic &rhs) noexcept    {return lhs.path() == rhs.path();}
	inline bool operator!=(const topic_path &lhs, const topic &rhs) noexcept    {return lhs.path() != rhs.path();}


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
