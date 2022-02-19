#pragma once



#include <string_view>
#include <functional>
#include <any>
#include "coop_pool.h"
#include "coop_trie.h"


namespace pleb
{
	/*
		PLEB uses these generic function wrappers for all event handling.

		handler_function may modify its argument or overwrite it to return a value.
		observer_function accepts an immutable argument.
	*/
	using handler_function   = std::function<void(      std::any&)>;
	using observer_function  = std::function<void(const std::any&)>;


	template<char Delimiter> class path_view_;
	using path_view = path_view_<'/'>;

	/*
		This class facilitates iterating over the elements of a path delimited by slashes.
			Leading, trailing and consecutive slashes will be ignored when iterating.
	*/
	template<char Delimiter>
	class path_view_
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
		path_view_(std::string_view s)   noexcept    : string(s) {}
		path_view_(const std::string &s) noexcept    : string(s) {}
		path_view_(const char *cstr)     noexcept    : string(cstr) {}

		iterator begin() const noexcept    {return iterator(string);}
		iterator end  () const noexcept    {return iterator(string, end_tag {});}

		// Returns true if the string begins with a delimiter.
		bool is_absolute() const noexcept    {return string.length() && string[0] == Delimiter;}


	public:
		std::string_view string;
	};


	/*
		Exception thrown when a service or other path is missing.
	*/
	class no_such_path : public std::runtime_error
	{
	public:
		no_such_path(std::string_view preamble, std::string_view path)
			:
			runtime_error(std::string(preamble) + ": " + std::string(path)) {}

		no_such_path(std::string_view preamble, path_view path)
			:
			no_such_path(preamble, path.string) {}
	};


	/*
		Exception thrown when a function
	*/
	class incompatible_type : public std::logic_error
	{
	public:
		incompatible_type(std::string_view preamble, const std::string& path)    : logic_error(preamble.data() + (": " + path)) {}
		incompatible_type(std::string_view preamble, path_view path)             : incompatible_type(preamble, path.string) {}
		incompatible_type(std::string_view preamble, std::string_view path)      : incompatible_type(preamble, std::string(path)) {}
		incompatible_type(std::string_view preamble, const char* path)           : incompatible_type(preamble, std::string(path)) {}
	};
}
