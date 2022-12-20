#pragma once


#include "resource.h"


namespace pleb
{
#define PLEB_RESOURCE_VERB(METHOD_NAME) \
	template<typename... Args> \
	auto METHOD_NAME(pleb::topic topic, Args&& ... args) {\
		return topic.METHOD_NAME(std::forward<Args>(args)...);}

	PLEB_RESOURCE_VERB(subscribe)
	PLEB_RESOURCE_VERB(publish)

	PLEB_RESOURCE_VERB(serve)

	PLEB_RESOURCE_VERB(GET)
	PLEB_RESOURCE_VERB(PUT)
	PLEB_RESOURCE_VERB(POST)
	PLEB_RESOURCE_VERB(PATCH)
	PLEB_RESOURCE_VERB(DELETE)

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
