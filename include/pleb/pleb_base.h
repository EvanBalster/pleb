#pragma once

#include <string_view>
#include <functional>

#include "conversion.h"

namespace pleb
{
#define PLEB_ND [[nodiscard]] inline

	/*
		Resources are used to route all communications through PLEB.
			Pointers to resources may be stored to keep them alive and
			avoid the overhead of looking them up every time.
	*/
	class resource;
	using resource_ptr = std::shared_ptr<resource>;



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


	public:
		std::string_view string;
	};


	/*
		These methods are defined in resource.h
	*/
	inline resource_ptr find_nearest_resource(topic_view topic);
	inline resource_ptr find_resource        (topic_view topic);

	inline resource_ptr operator/(const resource_ptr &ptr, topic_view subtopic);



	/*
		"resource identity" designed for use in functions referring to a resource.
			Accepts string arguments or resource pointers.
	*/
	class resource_ref : public std::shared_ptr<resource>
	{
	public:
		using shared_ptr::shared_ptr;

		resource_ref(topic_view         topic)    : shared_ptr(find_resource(topic)) {}
		resource_ref(const std::string& topic)    : shared_ptr(find_resource(topic)) {}
		resource_ref(std::string_view   topic)    : shared_ptr(find_resource(topic)) {}
		resource_ref(const char*        topic)    : shared_ptr(find_resource(topic)) {}
	};

	class nearest_resource_ref : public std::shared_ptr<resource>
	{
	public:
		using shared_ptr::shared_ptr;

		nearest_resource_ref(topic_view         topic)    : shared_ptr(find_nearest_resource(topic)) {}
		nearest_resource_ref(const std::string& topic)    : shared_ptr(find_nearest_resource(topic)) {}
		nearest_resource_ref(std::string_view   topic)    : shared_ptr(find_nearest_resource(topic)) {}
		nearest_resource_ref(const char*        topic)    : shared_ptr(find_nearest_resource(topic)) {}
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
