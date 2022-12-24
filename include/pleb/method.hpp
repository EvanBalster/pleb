#pragma once


#include <iosfwd>
#include <string_view>


namespace pleb
{
	enum class method_enum
	{
		Unknown = -1,
		None    = 0,
		GET,
		HEAD,
		POST,
		PUT,
		DELETE,
		PATCH,
		OPTIONS,
		CONNECT,
		TRACE,
		EndOfValidMethods
	};


	/*
		Represents an HTTP method.
	*/
	class method
	{
	public:
		static constexpr method_enum
			GET = method_enum::GET,
			HEAD = method_enum::HEAD,
			POST = method_enum::POST,
			PUT = method_enum::PUT,
			DELETE = method_enum::DELETE,
			PATCH = method_enum::PATCH,
			OPTIONS = method_enum::OPTIONS,
			CONNECT = method_enum::CONNECT,
			TRACE = method_enum::TRACE,
			Unknown = method_enum::Unknown,
			None = method_enum::None;

	public:
		method_enum code;

	public:
		constexpr method()              noexcept                  : code(method_enum::None) {}
		constexpr method(method_enum c) noexcept                  : code(c)                {}

		// String conversion
		static method    Parse(std::string_view) noexcept;
		std::string_view toString()        const noexcept;

		constexpr operator method_enum() const noexcept    {return code;}

		// Categorize the method according to HTTP conventions.
		bool isSafe()        const;
		bool isNullipotent() const    {return isSafe();}
		bool isIdempotent()  const;
		bool isCacheable()   const;

		// Is a body expected with this method?
		bool allowRequestBody()  const;
		bool allowResponseBody() const;

		// Does it make sense to use this method without a response?
		bool allowNoResponse() const;

		// Check method validity
		explicit operator bool() const noexcept    {return code > method_enum::None && code < method_enum::EndOfValidMethods;}
	};


	/*
		Represents a set of HTTP methods.
	*/
	class method_set
	{
	public:
		using mask_t = uint32_t;
		mask_t mask = 0;

	public:
		method_set()              noexcept    {}
		method_set(method_enum m) noexcept    {insert(m);}
		method_set(method      m) noexcept    {insert(m);}

		static method_set All() noexcept    {method_set m; m.mask = ~mask_t(1u); return m;}

		void     clear     ()               noexcept    {mask = 0;}
		void     insert    (method m)       noexcept    {if (m.code > method_enum::None) mask |=  (mask_t(1u) << mask_t(m.code));}
		void     erase     (method m)       noexcept    {mask &= ~(mask_t(1u) << mask_t(m.code));}
		bool     contains  (method m) const noexcept    {return (mask >> mask_t(m.code)) & mask_t(1u);}

		method_set  operator+ (method m) noexcept    {method_set r(*this); r.insert(m); return r;}
		method_set  operator- (method m) noexcept    {method_set r(*this); r.erase (m); return r;}

		method_set& operator+=(method m) noexcept    {insert(m); return *this;}
		method_set& operator-=(method m) noexcept    {erase (m); return *this;}
	};


	inline method_set operator+(method      a, method      b) noexcept    {method_set m(a); return m+=b;}
	inline method_set operator+(method_enum a, method      b) noexcept    {method_set m(a); return m+=b;}
	inline method_set operator+(method      a, method_enum b) noexcept    {method_set m(a); return m+=b;}
	inline method_set operator+(method_enum a, method_enum b) noexcept    {method_set m(a); return m+=b;}


	/*
		IMPLEMENTATION FOLLOWS
	*/

	inline bool method::isSafe()        const
	{
		switch (code)
		{
		case method_enum::GET:
		case method_enum::HEAD:
		case method_enum::OPTIONS:
		case method_enum::TRACE:
			return true;
		default:
			return false;
		}
	}
	inline bool method::isIdempotent()  const
	{
		switch (code)
		{
		case method_enum::GET:
		case method_enum::HEAD:
		case method_enum::PUT:
		case method_enum::DELETE:
		case method_enum::OPTIONS:
		case method_enum::TRACE:
			return true;
		default:
			return false;
		}
	}
	inline bool method::isCacheable()   const
	{
		switch (code)
		{
		case method_enum::GET:
		case method_enum::HEAD:
		case method_enum::POST:
			return true;
		default:
			return false;
		}
	}
	inline bool method::allowRequestBody() const
	{
		switch (code)
		{
		case method_enum::HEAD:
		case method_enum::DELETE:
		case method_enum::TRACE:
			return false;
		default:
			return true;
		}
	}
	inline bool method::allowResponseBody() const
	{
		switch (code)
		{
		case method_enum::HEAD:
			return false;
		default:
			return true;
		}
	}

	inline bool method::allowNoResponse() const
	{
		switch (code)
		{
		case method_enum::GET:
		case method_enum::HEAD:
		case method_enum::OPTIONS:
		case method_enum::CONNECT:
		case method_enum::TRACE:
			return false;
		default:
			return true;
		}
	}

	inline std::string_view method::toString() const noexcept
	{
		#define PLEB_METHOD_STRING_CASE(NAME) case method_enum::NAME: return #NAME

		switch (code)
		{
			PLEB_METHOD_STRING_CASE(GET);
			PLEB_METHOD_STRING_CASE(HEAD);
			PLEB_METHOD_STRING_CASE(POST);
			PLEB_METHOD_STRING_CASE(PUT);
			PLEB_METHOD_STRING_CASE(DELETE);
			PLEB_METHOD_STRING_CASE(PATCH);
			PLEB_METHOD_STRING_CASE(OPTIONS);
			PLEB_METHOD_STRING_CASE(TRACE);
			PLEB_METHOD_STRING_CASE(CONNECT);

			case method::None:    return "NoMethod";
			default:
			case method::Unknown: return "UnknownMethod";
		}

		#undef PLEB_METHOD_STRING_CASE
	}

	namespace detail
	{
		constexpr unsigned CC2(char a, char b)    {return (unsigned(a)<<8u) | unsigned(b);}
	}

	inline method method::Parse(std::string_view v) noexcept
	{
		using namespace detail;
		#define PLEB_METHOD_PARSE_CASE(NAME) \
			case detail::CC2(#NAME[0], #NAME[1]): return (v == #NAME) ? method_enum::NAME : method_enum::Unknown

		switch (v.length())
		{
		case 0:
			return method_enum::None;
		case 1:
			// TODO consider nonstandard single-byte method codes for internal use
			return method_enum::Unknown;
		default:
			switch (detail::CC2(v[0], v[1]))
			{
				PLEB_METHOD_PARSE_CASE(GET);
				PLEB_METHOD_PARSE_CASE(HEAD);
				PLEB_METHOD_PARSE_CASE(POST);
				PLEB_METHOD_PARSE_CASE(PUT);
				PLEB_METHOD_PARSE_CASE(DELETE);
				PLEB_METHOD_PARSE_CASE(PATCH);
				PLEB_METHOD_PARSE_CASE(OPTIONS);
				PLEB_METHOD_PARSE_CASE(TRACE);
				PLEB_METHOD_PARSE_CASE(CONNECT);
				default: return method_enum::Unknown;
			}
		}

		#undef PLEB_METHOD_PARSE_CASE
	}
}

template<class CharT, class Traits>
inline std::basic_ostream<CharT,Traits> &operator<<(std::basic_ostream<CharT,Traits> &out, const pleb::method m)
{
	return out << m.toString();
}
