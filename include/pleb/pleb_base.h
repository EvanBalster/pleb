#pragma once



#include <string_view>
#include <functional>
#include <any>
#include "coop_pool.h"
#include "coop_trie.h"


namespace pleb
{
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
		path_view_(std::string_view s) noexcept    : string(s) {}

		iterator begin() const noexcept    {return iterator(string);}
		iterator end  () const noexcept    {return iterator(string, end_tag {});}

		// Returns true if the string begins with a delimiter.
		bool is_absolute() const noexcept    {return string.length() && string[0] == Delimiter;}


	public:
		std::string_view string;
	};

	/*
		This generic function wrapper is used for all calls through pleb.
	*/
	using function_any_ref   = std::function<void(      std::any&)>;
	using function_any_const = std::function<void(const std::any&)>;
}
