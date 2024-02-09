#pragma once


#include <cstdint>
#include <atomic>

#include "flags.hpp"
#include "topic.hpp"
#include "content.hpp"


namespace pleb
{
	namespace detail
	{
		using id_integer_t = uintptr_t;

		template<class C>
		std::atomic<id_integer_t> id_counter = 1;
	}

	/*
		Base class for messages (request, response and event).
		
		PLEB messages always concern some resource.
	*/
	class message_base
	{
	public:
		using code_t = uint16_t;

		enum class id_t : detail::id_integer_t
		{
			no_id = ~detail::id_integer_t(0)
		};
		static constexpr id_t no_id = id_t::no_id;

	public:
		code_t           code;         // Status or method
		flags::features  features;
		flags::filtering filtering;    // Affects visibility of message
		flags::handling  requirements; // Required properties of handler

		id_t             id;           // message ID, unique within the process

		// The topic of the message, which resembles a pathname.
		topic_path       topic;


	public:
		bool recursive() const noexcept    {return filtering & flags::recursive;}

		void set_non_recursive() noexcept    {filtering = filtering & flags::filtering(~flags::recursive);}
		void set_recursive()     noexcept    {filtering = filtering | flags::recursive;}

		static id_t generate_unique_id() noexcept    {return id_t(detail::id_counter<message_base>++);}


	public:
		message_base(
			const topic_path &_topic,
			uint32_t          _code,
			message_flags     flags)
			:
			code(_code), features(flags::no_features),
			filtering(flags.filtering), requirements(flags.handling),
			id(generate_unique_id()), topic(_topic)
			{}
	};

	/*
		PLEB messages carry content in the form of a std::any container.
	*/
	class message : public message_base, public content
	{
	public:
		static const size_t BASE_SIZE = sizeof(message_base);

		message(
			const topic_path &topic,
			code_t            code,
			std_any::any    &&value,
			message_flags     flags)
			:
			message_base(topic, code, flags),
			content(std::move(value)) {}

		message(
			const topic_path   &topic,
			code_t              code,
			const std_any::any &value,
			message_flags       flags)
			:
			message_base(topic, code, flags),
			content(value) {}


		// Methods and members inherited from class content include the following:
		using content::value;
		using content::value_cast;
		using content::get;
		using content::get_mutable;
	};

	static const size_t MSGSIZE = sizeof(message);


	/*
		A parent class for objects that can receive PLEB messages.
			Base class for service, client and subscription.
	*/
	class receiver
	{
	public:
		const flags::filtering ignored;
		const flags::handling  handling;

	public:
		receiver(
			receiver_config<flags::default_receiver_ignore> flags = {})
			:
			ignored(flags.filtering), handling(flags.handling) {}

		bool accepts(flags::filtering message_filtering) const noexcept
		{
			return !(message_filtering & ignored);
		}

		flags::handling unhandled_flags(flags::handling requirements) const noexcept
		{
			return requirements & (~handling);
		}


	protected:
		// This type and its descendants are neither copyable nor movable.
		receiver(const receiver&) = delete;
		receiver(receiver&&) = delete;
		void operator=(const receiver&) = delete;
		void operator=(receiver&&) = delete;
	};



	/*
		Exception thrown when a service or subscriber receives
			a type it does not understand.
	*/
	class incompatible_type : public detail::topic_logic_error
	{
	public:
		using detail::topic_logic_error::topic_logic_error;
	};

	/*
		Exception thrown when a message needs special handling,
			the receiver cannot provide that handling and no
			handler function can provide it either.
	*/
	class handling_unavailable : public detail::topic_logic_error
	{
	public:
		using detail::topic_logic_error::topic_logic_error;
	};
}
