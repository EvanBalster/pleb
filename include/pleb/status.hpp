#pragma once

#include <iosfwd>
#include <string>
#include <exception>
#include "util/HttpStatusCodes_C++11.h"


namespace pleb
{
	using status_enum = HttpStatus::Code;

	// alias for easier typing:  eg, statuses::OK
	using statuses = HttpStatus::Code;


	class status
	{
	public:
		status_enum code;

	public:
		status()                 : code(status_enum(0)) {}
		status(status_enum c)    : code(c) {}

		// Check status validity -- DOES NOT DISTINGUISH SUCCESS FROM ERROR
		explicit operator bool() const noexcept    {return toInt() > 0;}

		// Convert implicitly to code
		operator status_enum() const noexcept    {return code;}

		// Convert to integer
		int  toInt          () const noexcept    {return HttpStatus::toInt(code);}

		// String conversion
		static status    Parse(std::string_view) noexcept;
		std::string      toString()        const noexcept;

		// Categorize the status according to HTTP conventions.
		bool isInformational() const noexcept    {return HttpStatus::isInformational(code);}
		bool isSuccessful   () const noexcept    {return HttpStatus::isSuccessful(code);}
		bool isRedirection  () const noexcept    {return HttpStatus::isRedirection(code);}
		bool isClientError  () const noexcept    {return HttpStatus::isClientError(code);}
		bool isServerError  () const noexcept    {return HttpStatus::isServerError(code);}
		bool isError        () const noexcept    {return HttpStatus::isError(code);}

		// Shorthand methods
		bool isInfo         () const noexcept    {return isInformational();}
		bool isSuccess      () const noexcept    {return isSuccessful();}
		bool isRedirect     () const noexcept    {return isRedirection();}
		
		// Get reason-phrase string
		std::string_view reasonPhrase() const noexcept
		{
			auto rp = HttpStatus::reasonPhrase(code);
			if (rp.length() == 0) rp = "(Undefined Status)";
			return rp;
		}
	};


	inline std::string status::toString() const noexcept
	{
		int n = toInt();
		if (n <= 0 || n > 999) return std::string("N/A");

		std::string res; res.resize(3);
		res[0] = '0' + ((n/100)%10);
		res[1] = '0' + ((n/ 10)%10);
		res[2] = '0' + ((n    )%10);
		return res;
	}

	inline status status::Parse(std::string_view s) noexcept
	{
		if (s.size() != 3
			|| s[0] < '0' || s[0] > '9'
			|| s[1] < '0' || s[1] > '9'
			|| s[2] < '0' || s[2] > '9')
		{
			return status();
		}
		return status_enum
			(       int(s[2]-'0')
			+  10 * int(s[1]-'0')
			+ 100 * int(s[0]-'0'));
	}


	/*
		Throwing this exception from a request handler will
			generate a response with the given status.
	*/
	class status_exception : public std::exception
	{
	public:
		status status;

	public:
		status_exception(pleb::status _status)        : status(_status) {}
		~status_exception() override                  {}

		const char *what() const noexcept override    {return status.reasonPhrase().data();}
	};
}

template<class CharT, class Traits>
inline std::basic_ostream<CharT,Traits> &operator<<(std::basic_ostream<CharT,Traits> &out, const pleb::status s)
{
	return out << s.toString();
}
