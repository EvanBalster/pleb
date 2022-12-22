#pragma once


#include "bind.hpp"
#include "topic_impl.hpp"


namespace pleb
{
#define PLEB_RESOURCE_VERB(METHOD_NAME) \
	template<typename... Args, typename Valid = std::void_t<decltype(pleb::topic().METHOD_NAME(std::declval<Args>()...))>> \
	auto METHOD_NAME(pleb::topic topic, Args&& ... args) {\
		return topic.METHOD_NAME(std::forward<Args>(args)...);}

#define PLEV_MESSAGE_VERB(METHOD_NAME) \
	template<typename... Args, typename Valid = std::void_t<decltype(pleb::topic().METHOD_NAME(std::declval<Args>()...))>> \
	auto METHOD_NAME(const pleb::topic_path &topic, Args&& ... args) {\
		return topic.METHOD_NAME(std::forward<Args>(args)...);}

	PLEB_RESOURCE_VERB(subscribe)
	PLEV_MESSAGE_VERB(publish)

	PLEB_RESOURCE_VERB(serve)

	PLEV_MESSAGE_VERB(GET)
	PLEV_MESSAGE_VERB(PUT)
	PLEV_MESSAGE_VERB(POST)
	PLEV_MESSAGE_VERB(PATCH)
	PLEV_MESSAGE_VERB(DELETE)

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
