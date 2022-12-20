#pragma once


#include <cstdint>

#include "flags.h"
#include "pleb_base.h"
#include "content.h"


namespace pleb
{
	/*
		Base class for messages (request, response and event).
		
		PLEB messages always concern some resource.
	*/
	class message_base
	{
	public:
		using code_t = uint16_t;

	public:
		code_t           code;         // Status or method
		flags::features  features;
		flags::filtering filtering;    // Affects visibility of message
		flags::handling  requirements; // Required properties of handler

		// The resource which the message concerns.
		topic            topic;


	public:
		bool recursive() const noexcept    {return filtering & flags::recursive;}

		void set_non_recursive() noexcept    {filtering = filtering & flags::filtering(~flags::recursive);}
		void set_recursive()     noexcept    {filtering = filtering | flags::recursive;}


	public:
		message_base(
			pleb::topic      _topic,
			uint32_t         _code,
			flags::filtering _filtering,
			flags::handling  _requirements)
			:
			topic(std::move(_topic)), code(_code), features(flags::no_features),
			filtering(_filtering), requirements(_requirements)
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
			pleb::topic       topic,
			code_t            code,
			std::any        &&value,
			flags::filtering  filtering,
			flags::handling   requirements)
			:
			message_base(std::move(topic), code, filtering, requirements),
			content(std::move(value)) {}


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
			flags::filtering _ignored = flags::default_receiver_ignore,
			flags::handling  _handling = flags::no_special_handling)
			:
			ignored(_ignored), handling(_handling) {}

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
