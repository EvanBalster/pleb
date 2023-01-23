#pragma once


namespace pleb
{
	namespace flags
	{
		enum features  : uint16_t;
		enum filtering : uint16_t;
		enum handling  : uint16_t;

#define PLEB_BITWISE_ENUM_OPS(ENUM_T, INT_T) \
		constexpr inline ENUM_T operator| (ENUM_T  a, ENUM_T  b) noexcept    {return ENUM_T (INT_T(a)|INT_T(b));} \
		constexpr inline ENUM_T operator& (ENUM_T  a, ENUM_T  b) noexcept    {return ENUM_T (INT_T(a)&INT_T(b));} \
		constexpr inline ENUM_T operator^ (ENUM_T  a, ENUM_T  b) noexcept    {return ENUM_T (INT_T(a)^INT_T(b));} \
		constexpr inline ENUM_T operator|=(ENUM_T &a, ENUM_T  b) noexcept    {return a = a | b;} \
		constexpr inline ENUM_T operator&=(ENUM_T &a, ENUM_T  b) noexcept    {return a = a & b;} \
		constexpr inline ENUM_T operator^=(ENUM_T &a, ENUM_T  b) noexcept    {return a = a ^ b;} \
		constexpr inline ENUM_T operator~ (ENUM_T  a)            noexcept    {return ENUM_T (~INT_T(a));} \

		PLEB_BITWISE_ENUM_OPS(features,  uint16_t)
		PLEB_BITWISE_ENUM_OPS(filtering, uint16_t)
		PLEB_BITWISE_ENUM_OPS(handling,  uint16_t)
#undef PLEB_BITWISE_OPS

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
			Filtering flags are used to block certain messages from
				reaching certain receivers.  A receiver will ignore
				messages with any filtering flags matching one of its
				own filtering flags.
				The recursive flag is special (see below).

			Bits 15..8 are special and reserved for PLEB's own features.
			Bits 7..0 are for application use.

			Bits 23..16 invoke a helper function by default.
			Bits 15..8 cause messages to be ignored by default.
		*/
		enum filtering : uint16_t
		{
			/*
				[recursive] messages are propagate up up the resource tree.
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
				Messages with these flags are published automatically by PLEB.
					announce_receiver    -- this service or subscriber has been added.
					subscriber_exception -- a subscriber has thrown this exception.
			*/
			announce_receiver    = (1 << 14), // Default: ignore
			subscriber_exception = (1 << 13), // Default: ignore

			/*
				Suggested-use application flags.

				These have no special function in PLEB other than their
					handling under the default policy.

				LOGGING is useful for messages not relevant to most subscribers.
				INTERNAL is for messages that should not be sent to external networks.
				REMOTE is for messages that originated from external networks.

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

			default_subscription_ignore = default_receiver_ignore,
			default_service_ignore      = default_receiver_ignore | recursive,
			default_client_ignore       = 0,
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
		This structure is used to combine filtering and handling flags into one value.
	*/
	struct message_flags
	{
		flags::filtering filtering;
		flags::handling  handling;

		constexpr message_flags()
			:
			filtering(flags::default_message_filtering), handling(flags::no_special_handling) {}

		constexpr message_flags(flags::filtering f)
			:
			filtering(f), handling(flags::no_special_handling) {}

		constexpr message_flags(flags::filtering f, flags::handling h)
			:
			filtering(f), handling(h) {}

		constexpr message_flags  operator| (flags::filtering f) const noexcept    {return {filtering|f,handling};}
		constexpr message_flags  operator| (flags::handling  h) const noexcept    {return {filtering,handling|h};}
		message_flags&           operator|=(flags::filtering f)       noexcept    {filtering |= f; return *this;}
		message_flags&           operator|=(flags::handling  h)       noexcept    {handling  |= h; return *this;}
	};

	inline constexpr message_flags operator|(flags::filtering f, flags::handling h) noexcept    {return {f,h};}
	inline constexpr message_flags operator|(flags::handling h, flags::filtering f) noexcept    {return {f,h};}
	inline constexpr message_flags operator|(flags::filtering f, message_flags   m) noexcept    {return {m.filtering|f,m.handling};}
	inline constexpr message_flags operator|(flags::handling h,  message_flags   m) noexcept    {return {m.filtering,h|m.handling};}

	/*
		This template is used as a parameter in subscribe and serve functions.
	*/
	template<flags::filtering DefaultFiltering>
	struct receiver_config : public message_flags
	{
	public:
		constexpr receiver_config()
			:
			message_flags(DefaultFiltering, flags::no_special_handling) {}

		constexpr receiver_config(const message_flags &flags)
			:
			message_flags(flags) {}

		constexpr receiver_config(flags::handling h)
			:
			message_flags(DefaultFiltering, h) {}
	};

	using subscription_config = receiver_config<flags::default_subscription_ignore>;
	using service_config      = receiver_config<flags::default_service_ignore>;
	using client_config       = receiver_config<flags::default_client_ignore>;
}

