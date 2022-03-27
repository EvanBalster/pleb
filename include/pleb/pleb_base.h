#pragma once



#include <string_view>
#include <functional>
#ifdef PLEB_REPLACEMENT_ANY_HEADER
	#include PLEB_REPLACEMENT_ANY_HEADER
#else
	#include <any>
#endif
#include "coop_pool.h"
#include "coop_trie.h"


namespace pleb
{
#ifdef PLEB_REPLACEMENT_ANY_NAMESPACE
	namespace std_any = ::PLEB_REPLACEMENT_ANY_NAMESPACE;
#else
	namespace std_any = ::std;
#endif

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
	class target_resource
	{
	public:
		resource_ptr resource;

		target_resource(resource_ptr _resource)      : resource(std::move(_resource)) {}

		target_resource(topic_view         topic)    : resource(find_resource(topic)) {}
		target_resource(const std::string& topic)    : resource(find_resource(topic)) {}
		target_resource(std::string_view   topic)    : resource(find_resource(topic)) {}
		target_resource(const char*        topic)    : resource(find_resource(topic)) {}

		// Access the resource
		pleb::resource *operator->() const noexcept    {return  resource.get();}
		pleb::resource &operator* () const noexcept    {return *resource.get();}
		operator resource_ptr()      const noexcept    {return  resource;}
		resource_ptr release()             noexcept    {return std::move(resource);}
	};

	class target_nearest_resource
	{
	public:
		resource_ptr resource;

		target_nearest_resource(resource_ptr _resource)      : resource(std::move(_resource)) {}

		target_nearest_resource(topic_view         topic)    : resource(find_nearest_resource(topic)) {}
		target_nearest_resource(const std::string& topic)    : resource(find_nearest_resource(topic)) {}
		target_nearest_resource(std::string_view   topic)    : resource(find_nearest_resource(topic)) {}
		target_nearest_resource(const char*        topic)    : resource(find_nearest_resource(topic)) {}

		// Access the resource
		pleb::resource *operator->() const noexcept    {return  resource.get();}
		pleb::resource &operator* () const noexcept    {return *resource.get();}
		operator resource_ptr()      const noexcept    {return  resource;}
		resource_ptr release()             noexcept    {return std::move(resource);}
	};



	/*
		Exception thrown when a topic does not exist.
	*/
	class no_such_topic : public std::runtime_error
	{
	public:
		no_such_topic(std::string_view preamble, std::string_view topic)
			:
			runtime_error(std::string(preamble) + ": " + std::string(topic)) {}

		no_such_topic(std::string_view preamble, topic_view topic)
			:
			no_such_topic(preamble, topic.string) {}
	};


	/*
		Exception thrown when a function
	*/
	class incompatible_type : public std::logic_error
	{
	public:
		incompatible_type(std::string_view preamble, const std::string& topic)    : logic_error(preamble.data() + (": " + topic)) {}
		incompatible_type(std::string_view preamble, topic_view topic)            : incompatible_type(preamble, topic.string) {}
		incompatible_type(std::string_view preamble, std::string_view topic)      : incompatible_type(preamble, std::string(topic)) {}
		incompatible_type(std::string_view preamble, const char* topic)           : incompatible_type(preamble, std::string(topic)) {}
	};
}
