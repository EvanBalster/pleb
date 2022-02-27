#pragma once


#include "pleb_base.h"
#include "conversion.h"


/*
	A minimal system for publish/subscribe with a global hierarchy of topics.
		Each topic may have any number of subscribers.

	These classes do not provide asynchronous execution, futures or serialization;
		those features can be implemented as part of the function wrapper.
		Subscribers throwing an exception will halt processing of a report.

	TEARDOWN SAFETY NOTICE:
		Calls may be in progress when the subscription is released.
		Some protection against this case is necessary in client code.
*/


namespace pleb
{
	/*
		Events are organized under a topic -- a slash-delimited string.
	*/
	class topic;
	class subscription;

	/*
		Topics and subscriptions are retained by shared pointers.
			Topic pointers can be cached for repeated publishing.
	*/
	using topic_ptr        = std::shared_ptr<topic>;
	using subscription_ptr = std::shared_ptr<subscription>;

	/*
		Reports are events or messages passed from publishers to subscribers.
	*/
	class report
	{
	public:
		topic    &topic;
		const int status; // Conventionally set to an HTTP status code, or zero.
		std::any  value;

	public:
		// Access value as a specific type.
		template<typename T> const T *cast() const noexcept    {return std::any_cast<T>(&value);}
		template<typename T> T       *cast()       noexcept    {return std::any_cast<T>(&value);}

		// Access value, allowing it to be supplied by value or shared_ptr.
		template<typename T> const T *get() const noexcept    {return pleb::any_const_ptr<T>(value);}
	};
	using subscriber_function = std::function<void(const report&)>;

	/*
		Minimal pub/sub system.
			Can be used for report broadcast and surveyor pattern.
	*/
	class subscription
	{
	public:
		subscription(std::shared_ptr<topic> _topic, subscriber_function &&_func)
			:
			topic(std::move(_topic)), func(std::move(_func)) {}

		const std::shared_ptr<topic> topic;
		const subscriber_function    func;
	};

	/*
		Topics form a global hierarchy (trie) to which 
	*/
	class topic :
		protected coop::unmanaged::multitrie<subscription>
	{
	public:
		using subscription = pleb::subscription;


	public:
		// Access the root topic.
		static topic_ptr root() noexcept                   {static topic_ptr root = _asTopic(_trie::create()); return root;}

		// Access a topic by path.
		static topic_ptr find        (path_view path) noexcept    {return root()->subtopic(path);}
		static topic_ptr find_nearest(path_view path) noexcept    {return root()->nearest(path);}

		~topic() {}

		// Access the parent topic (root's parent is null)
		topic_ptr parent() noexcept                        {return _asTopic(_trie::parent());}

		// Access a subtopic of this topic.
		topic_ptr subtopic(path_view subtopic)             {return _asTopic(_trie::get    (subtopic));}
		topic_ptr nearest (path_view subtopic) noexcept    {return _asTopic(_trie::nearest(subtopic));}

		// Publish something all subscribers of this topic or a subtopic -- and up the chain.
		template<typename T>
		void publish(path_view subtopic, T &&item)    {this->nearest(subtopic)->publish(std::move(item));}

		template<typename T>
		void publish(                           T &&item)
		{
			report report = {*this, 0, std::move(item)};

			for (topic_ptr node = shared_from_this(); node; node = node->parent())
				for (subscription &sub : (_trie::coop_type&) *node)
					sub.func(report);
		}

		// Create a subscription to this topic and all subtopics, or a subtopic and its subtopics.
		std::shared_ptr<subscription> subscribe(                   subscriber_function &&f)    {return this->emplace(shared_from_this(), std::move(f));}
		std::shared_ptr<subscription> subscribe(path_view subpath, subscriber_function &&f)    {return subtopic(subpath)->subscribe(std::move(f));}


		/*
			Alias a direct child of this topic to another existing topic.
				The alias has the same lifetime as the original, and is
				interchangeable with it for both publishers and new subscribers.
			Fails, returning false, if the child ID is already in use.
		*/
		bool make_alias(std::string_view child_id, topic_ptr destination)    {_trie *t=destination.get(); return this->make_link(child_id, std::shared_ptr<_trie>(std::move(destination), t));}


		// Support shared_from_this
		topic_ptr                    shared_from_this()          {return topic_ptr                   (_trie::shared_from_this(), this);}
		std::shared_ptr<const topic> shared_from_this() const    {return std::shared_ptr<const topic>(_trie::shared_from_this(), this);}
		

	protected:
		using _trie = coop::unmanaged::multitrie<subscription>;

		topic(std::shared_ptr<topic> parent)                   : _trie(std::shared_ptr<_trie>(parent, (_trie*) &*parent)) {}
		static topic_ptr _asTopic(std::shared_ptr<_trie> p)    {return std::shared_ptr<topic>(p, (topic*) &*p);}
	};


	/*
		Simple interface for synchronous publish / subscribe.

		class Hive
		{
			void sendBees(std::any 
		}

		auto mySubscription = pleb::subscribe("flowers/found", &Hive::sendBees, hivePtr);

		...

		publish(
	*/
	[[nodiscard]] inline
	std::shared_ptr<subscription>
		subscribe(
			path_view             topic,
			subscriber_function &&function) noexcept
	{
		return pleb::topic::find(topic)->subscribe(std::move(function));
	}

	template<class T> [[nodiscard]]
	std::shared_ptr<subscription>
		subscribe(
			path_view path,
			T        *handler_object,
			void (T::*handler_method)(const pleb::report&))
	{
		return subscribe(path, std::bind(handler_method, handler_object, std::placeholders::_1));
	}

	template<typename T>
	void publish(
			path_view topic,
			T       &&item) noexcept
	{
		return pleb::topic::find_nearest(topic)->publish(std::move(item));
	}
}