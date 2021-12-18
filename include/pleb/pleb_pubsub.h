#pragma once


#include "pleb_base.h"


/*
	A minimal system for publish/subscribe with a global hierarchy of topics.
		Each topic may have any number of subscribers.

	These classes do not provide asynchronous execution, futures or serialization;
		those features can be implemented as part of the function wrapper.
		Subscribers throwing an exception will halt processing of an event.

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

	/*
		High-performance code 
	*/
	using topic_ptr = std::shared_ptr<topic>;

	/*
		Minimal pub/sub system.
			Can be used for event broadcast and surveyor pattern.
	*/
	class subscription
	{
	public:
		subscription(std::shared_ptr<topic> _topic, function_any_const &&_func)
			:
			topic(std::move(_topic)), func(std::move(_func)) {}

		const std::shared_ptr<topic> topic;
		const function_any_const     func;
	};

	/*
		A trie of topics is 
	*/
	class topic :
		protected coop::unmanaged::multitrie<subscription>
	{
	public:
		using function_type = function_any_const;

		using subscription = pleb::subscription;


	public:
		// Access the root topic.
		static topic_ptr root() noexcept                   {static topic_ptr root = _asTopic(_trie::create()); return root;}

		~topic()
		{
			std::cout << "topic destroyed" << std::endl;
		}

		// Access the parent topic (root's parent is null)
		topic_ptr parent() noexcept                        {return _asTopic(_trie::parent());}

		// Access a subtopic of this topic.
		topic_ptr subtopic(path_view subtopic) noexcept    {return _asTopic(_trie::get    (subtopic));}
		topic_ptr nearest (path_view subtopic) noexcept    {return _asTopic(_trie::nearest(subtopic));}

		// Publish something all subscribers of this topic or a subtopic -- and up the chain.
		template<typename T>
		void publish(path_view subtopic, T &&item)    {this->nearest(subtopic)->publish(std::move(item));}

		template<typename T>
		void publish(                           T &&item)
		{
			std::any any(std::move(item));
			for (topic_ptr node = shared_from_this(); node; node = node->parent())
				for (subscription &sub : (_trie::coop_type&) *node)
					sub.func(any);
		}

		// Create a subscription to this topic and all subtopics, or a subtopic and its subtopics.
		std::shared_ptr<subscription> subscribe_all(                    function_type &&f) noexcept    {return this->emplace(shared_from_this(), std::move(f));}
		std::shared_ptr<subscription> subscribe    (path_view subtopic, function_type &&f) noexcept    {return this->subtopic(subtopic)->subscribe_all(std::move(f));}


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
	inline std::shared_ptr<subscription>
		subscribe(
			std::string_view     topic,
			function_any_const &&function) noexcept
	{
		return pleb::topic::root()->subscribe(topic, std::move(function));
	}

	template<typename T>
	static void
		publish(
			std::string_view topic,
			T              &&item) noexcept
	{
		return pleb::topic::root()->publish(topic, std::move(item));
	}
}