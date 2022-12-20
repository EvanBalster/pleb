#pragma once

#include <memory>
#include <string_view>
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
		Exception thrown when no resource exists for a given topic.
	*/
	class no_such_topic : public detail::topic_runtime_error
	{
	public:
		using detail::topic_runtime_error::topic_runtime_error;
	};



	/*
		Topics form a global hierarchy (trie) to which 
	*/
	class topic
	{
	protected:
		// A pointer to the "resolved" resource node.
		//   IE, the nearest resource that existed when this reference was set up.
		resource_node_ptr _base;

		// String with "unresolved" path.
		//   IE, path elements indicating a sub-resource that didn't exist at setup time.
		std::string       _subpath;


	protected:
		std::string _resolve(topic_view subpath);
		void        _resolve()    {if (_subpath.length()) _subpath = _resolve(_subpath);}
		void        _realize(topic_view subpath);
		void        _realize()    {if (_subpath.length()) _realize(_subpath); _subpath.clear();}


	public:
		using subscription = pleb::subscription;
		using service      = pleb::service;


	public:
		// Access the root resource.  (TODO allocate statically?)
		static resource_node_ptr root_node() noexcept;
		static topic             root()      noexcept                   {return topic(root_node());}


		topic()  : _base(nullptr) {}
		~topic() = default;

		// Create a topic referring to the given resource node.
		topic(resource_node_ptr node)                              : _base(std::move(node)) {}
		topic(resource_node_ptr node, topic_view       subpath)    : _base(std::move(node)), _subpath(subpath.string)     {}
		topic(resource_node_ptr node, std::string      subpath)    : _base(std::move(node)), _subpath(std::move(subpath)) {}
		topic(resource_node_ptr node, std::string_view subpath)    : _base(std::move(node)), _subpath(subpath)            {}
		topic(resource_node_ptr node, const char      *subpath)    : _base(std::move(node)), _subpath(subpath)            {}

		// Create from a string.
		topic(topic_view         topic)                            : _base(root_node()), _subpath(_resolve(topic)) {}
		topic(const std::string& topic)                            : _base(root_node()), _subpath(_resolve(topic)) {}
		topic(std::string_view   topic)                            : _base(root_node()), _subpath(_resolve(topic)) {}
		topic(const char*        topic)                            : _base(root_node()), _subpath(_resolve(topic)) {}

		// Access a subtopic.
		topic operator/(topic_view subpath) const;

		// Access the parent topic.
		//    The root is treated as its own parent.
		topic parent()        const;
		void  set_to_parent() noexcept;

		// Get the leaf identifier of this topic, or its full path.
		//   id() returns "[root]" for the root node.
		//   path_view() returns the path in up to two pieces and does not simplify slashes.
		//   path() returns the path in one piece and does not simplify slashes.
		std::string_view                id       () const noexcept;
		//std::array<std::string_view, 2> path_view() const noexcept;
		std::string                     path     () const;


		/*
			Make this topic refer to a resource more directly.
				resolve() stops when a resource node does not exist.
				realize() creates missing resource nodes, fully resolving the path.

			These methods won't normally affect the behavior of this class, only its performance.
		*/
		topic &resolve()    {_resolve(); return *this;}
		topic &realize()    {_resolve(); return *this;}


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
			flags::handling  handling  = flags::no_special_handling);


		/*
			SERVE this resource.
				Subsequent events on this resource will be passed to the function.
				If a service already exists here, this function will fail, returning null.
		*/
		[[nodiscard]] std::shared_ptr<service> serve(service_function &&function) noexcept;

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


		/*
			REQUEST something from this resource, providing a client for responding.
				client_ref may be a client_ptr or a std::future.
				The response target will receive a response, now or later.
				If there is no service, pleb::service_not_found is thrown.
		*/
		template<class V = std::any>
		void request(client_ref client, method method, V &&value = {});

		/* */                        void GET   (client_ref c)                    {return request(c, method::GET);}
		template<class V = std::any> void PUT   (client_ref c, V &&value)         {return request(c, method::PUT,    std::forward<V>(value));}
		template<class V = std::any> void POST  (client_ref c, V &&value = {})    {return request(c, method::POST,   std::forward<V>(value));}
		template<class V = std::any> void PATCH (client_ref c, V &&value)         {return request(c, method::PATCH,  std::forward<V>(value));}
		/* */                        void DELETE(client_ref c)                    {return request(c, method::DELETE);}

		/*
			Convenience API for requests.

			The returned request may be issued (sent) by:
			- calling async<T> or converting to std::future<T>
			- calling await<T>
			- calling push() or issue(client) -- auto_retrieve only.
		*/
		template<class V = std::any>
		auto_request request(method method, V &&value = {});
		template<class V = std::any>
		auto_retrieve retrieve(method method, V &&value = {});

		/* */                       auto_retrieve GET   ();
		template<class V = std::any> auto_request PUT   (V &&value);
		template<class V = std::any> auto_request POST  (V &&value = {});
		template<class V = std::any> auto_request PATCH (V &&value);
		/* */                        auto_request DELETE();

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
		void issue(pleb::request &msg);

		/*
			PUBLISH using a prepared pleb::event object.
				pleb::event usually calls this method upon construction;
				it can be used to publish the same event repeatedly.
		*/
		void publish(const pleb::event &msg);


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
			bool skip_this = false);

		template<typename Callback,        std::enable_if_t<std::is_invocable_v<Callback, service_ptr>, int> SFINAE = 0>
		void visit_services(
			const Callback &callback,
			size_t recursion_depth = 255);

		template<typename Callback,        std::enable_if_t<std::is_invocable_v<Callback, subscription_ptr>, int> SFINAE = 0>
		void visit_subscriptions(
			const Callback &callback,
			size_t recursion_depth = 255);
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
