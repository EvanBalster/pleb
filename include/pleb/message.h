#pragma once


#include <cstdint>

#include "pleb_base.h"
#include "content.h"


namespace pleb
{
	namespace flags
	{
		enum features  : uint16_t;
		enum filtering : uint16_t;
		enum handling  : uint16_t;

#define PLEB_BITWISE_OPS(ENUM_T, INT_T) \
		constexpr inline ENUM_T operator| (ENUM_T  a, ENUM_T  b) noexcept    {return ENUM_T (INT_T(a)|INT_T(b));} \
		constexpr inline ENUM_T operator& (ENUM_T  a, ENUM_T  b) noexcept    {return ENUM_T (INT_T(a)&INT_T(b));} \
		constexpr inline ENUM_T operator|=(ENUM_T &a, ENUM_T  b) noexcept    {return a = a | b;} \
		constexpr inline ENUM_T operator&=(ENUM_T &a, ENUM_T  b) noexcept    {return a = a & b;} \
		constexpr inline ENUM_T operator~ (ENUM_T  a)            noexcept    {return ENUM_T (~INT_T(a));} \

		PLEB_BITWISE_OPS(features,  uint16_t)
		PLEB_BITWISE_OPS(filtering, uint16_t)
		PLEB_BITWISE_OPS(handling,  uint16_t)

		/*
			These flags are used by PLEB to manage message state.
		*/
		enum features : uint16_t
		{
			//has_content = (1 << 0),
			//has_headers = (1 << 1),
			//has_client  = (1 << 2),

			did_send    = (1 << 8),
			did_respond = (1 << 9),

			no_features = 0,
		};

		/*
			These flags describe special properties of a message.
				Receivers (services and subscribers) have policies
				allowing them to ignore messages with certain flags
				or involve an intervening function for others.

			Bits 31..24 are special and reserved for PLEB's own features.

			Bits 23..0 are for application use.
			Bits 23..16 invoke a helper function by default.
			Bits 15..8 cause messages to be ignored by default.
		*/
		enum filtering : uint16_t
		{
			/*
				[recursive] messages are thrown up the resource tree.
					Recursive requests stop when a service accepts them.
					Recursive events will always continue to the root.
					The ignore recursive flag has a special behavior; it will not
					ignore all recursive messages, only those sent to sub-resources.

				[service_status] and [subscription_status] events are sent by PLEB.
					The payload is a service or subscription pointer.
					Subscribers will ignore them by default.
			*/
			recursive            = (1 << 15), // Default: services ignore, subscribers accept

			/*
				These messages are published automatically by PLEB.
			*/
			service_status       = (1 << 14), // Default: ignore
			subscription_status  = (1 << 13), // Default: ignore
			subscriber_exception = (1 << 12), // Default: ignore

			/*
				Suggested-use application flags.

				These have no special function in PLEB other than their
					handling under the default policy.

				LOGGING is useful for messages not relevant to most subscribers.
				INTERNAL is for messages that should not be sent to external networks.
				REMOTE is for messages that originate from external networks.

				REGULAR is for ordinary applications messages.  Set and accepted by default.
			*/
			logging  = (1 << 8), // Default: ignore
			internal = (1 << 7), // Default: accept
			remote   = (1 << 6), // Default: accept


			/*
				The REGULAR flag is set on messages by default.
					Receivers will accept regular messages by default.
			*/
			regular = 1, // Default: accept.



			/*
				PLEB's default behaviors.
			*/
			default_message_filtering = regular | recursive,

			default_receiver_ignore = (0x7F00),

			default_subscriber_ignore = default_receiver_ignore,
			default_service_ignore    = default_receiver_ignore | recursive,
			default_client_ignore     = 0,
		};


		/*
			Restrictions on message handling.

			If a message with a restriction is accepted by a receiver
				which has not marked the requirements, a handler will
				be called to intervene.
				By default, these handlers will throw an exception.
		*/
		enum handling : uint16_t
		{
			/*
				Special requirements for message handling.
					If a message specifies a handling flag and its receiver
					does not, a handler function will be called to intervene.
					If no handler is installed, an exception will be thrown.

				The lower 8 bits are reserved for application use.

				[no_copying] means the message value should not be copied.
				[no_moving] means the message value should not be moved.
					These may be used when the value would be invalid after the call.
					A receiver that doesn't store messages supports both.

				[immediate] means that the message must be processed synchronously.
					In particular, the response cannot be deferred.

				[realtime] means the reciever must work within a strict time limit.
					The nature of this time limit is left to the application,
					but typically implies that the receiver should be nonblocking.
					A service or handler may place realtime messages in a queue
					as long as they are not scoped or immediate.
			*/

			no_copying = (1 << 15),
			no_moving  = (1 << 14),

			immediate  = (1 << 11), // prevents request::defer, may afford optimizations
			realtime   = (1 << 10), // supported by std::future/async/await response handling

			// By default, receivers support no special handling.
			no_special_handling = 0,
		};
	};


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
		resource_ptr     resource;


	public:
		bool recursive() const noexcept    {return filtering & flags::recursive;}

		void set_non_recursive() noexcept    {filtering = filtering & flags::filtering(~flags::recursive);}
		void set_recursive()     noexcept    {filtering = filtering | flags::recursive;}


	public:
		message_base(
			resource_ptr     _resource,
			uint32_t         _code,
			flags::filtering _filtering,
			flags::handling  _requirements)
			:
			resource(std::move(_resource)), code(_code), features(flags::no_features),
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
			resource_ptr      resource,
			code_t            code,
			std::any        &&value,
			flags::filtering  filtering,
			flags::handling   requirements)
			:
			message_base(std::move(resource), code, filtering, requirements),
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
		const flags::handling   handling;

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
