#pragma once


#include "bind.hpp"
#include "topic_impl.hpp"


namespace pleb
{
#define PLEB_FORWARD_TO_TOPIC(METHOD_NAME) \
	template<typename... Args> auto METHOD_NAME(pleb::topic topic, Args&& ... args) -> \
		decltype(topic.METHOD_NAME(std::forward<Args>(args)...)) \
		{return topic.METHOD_NAME(std::forward<Args>(args)...);}

#define PLEB_FORWARD_TO_TOPIC_PATH(METHOD_NAME) \
	template<typename... Args> auto METHOD_NAME(const topic_path &topic, Args&& ... args) -> \
		decltype(topic.METHOD_NAME(std::forward<Args>(args)...)) \
		{return topic.METHOD_NAME(std::forward<Args>(args)...);}

#define PLEB_CALLABLE_REQUEST_METHOD(METHOD_NAME) \
	constexpr class : public pleb::method { using method::method; public: \
		template<typename... Args> auto operator()(const topic_path &topic, Args&& ... args) const -> \
		decltype(topic.METHOD_NAME(std::forward<Args>(args)...)) \
		{return topic.METHOD_NAME(std::forward<Args>(args)...);} \
	} METHOD_NAME(method_enum::METHOD_NAME)

#define PLEB_UNCALLABLE_REQUEST_METHOD(METHOD_NAME) \
	constexpr pleb::method METHOD_NAME(method_enum::METHOD_NAME)

	/*
		These global methods forward to methods of topic.

		Pass a topic or path as the first argument and subsequent arguments
			will be passed to the corresponding topic method (see topic.hpp).

		e.g:  auto my_subscription = pleb::subscribe("topic/1");
	*/
	PLEB_FORWARD_TO_TOPIC     (subscribe);
	PLEB_FORWARD_TO_TOPIC_PATH(publish);

	PLEB_FORWARD_TO_TOPIC     (serve);

	/*
		Names like pleb::GET can be used as constants to refer to REST methods
			or as functions to make a request using that method.

		Pass a topic or path as the first argument and subsequent arguments
			will be passed to the corresponding topic method (see topic.hpp).

		e.g:  pleb::POST("log/info", log_string);
		These can also be used in case statements, e.g.  case pleb::POST: break;
	*/
	PLEB_CALLABLE_REQUEST_METHOD(GET);
	PLEB_CALLABLE_REQUEST_METHOD(HEAD);
	PLEB_CALLABLE_REQUEST_METHOD(OPTIONS);

	PLEB_CALLABLE_REQUEST_METHOD(PUT);
	PLEB_CALLABLE_REQUEST_METHOD(POST);
	PLEB_CALLABLE_REQUEST_METHOD(PATCH);
	PLEB_CALLABLE_REQUEST_METHOD(DELETE);

	// Global symbols for methods, which don't currently act as functions.
	PLEB_UNCALLABLE_REQUEST_METHOD(TRACE);
	PLEB_UNCALLABLE_REQUEST_METHOD(CONNECT);

#undef PLEB_RESOURCE_VERB


	/*
		Access the table of general type conversion rules.
	*/
	inline const std::shared_ptr<conversion_table> &
		conversion_rules()                                                                             {return detail::global_conversion_table();}

	template<typename ConversionFunctor, typename Input = detail::detect_parameter_t<ConversionFunctor>>
	conversion_table::rule_ptr conversion_define(ConversionFunctor &&func)                             {return conversion_rules()->set(std::forward<ConversionFunctor>(func));}

	/*
		Perform a type conversion using the table of general conversion rules.
			convert(...) throws no_conversion_rule if no rule is defined.
			try_convert(...) returns a default value if no rule is defined.
	*/
	inline std::any convert(const std::any &x, std::type_index to_type)                                       {return conversion_rules()->convert(x,to_type);}
	template<typename To, typename From>
	To              convert(const From     &x)                                                                {return conversion_rules()->convert<To>(x);}

	inline std::any try_convert(const std::any &x, std::type_index to_type, const std::any &on_error = {})    {return conversion_rules()->try_convert(x, to_type, on_error);}
	template<typename To, typename From>
	To              try_convert(const From     &x, const To &on_error = {})                                   {return conversion_rules()->try_convert(x, on_error);}
}
