#pragma once


// Type erasure and runtime type information.
#include <any>
#include <typeinfo>
#include <typeindex>

// Compile-time type inference.
#include <functional>
#include <type_traits>

// Concurrent table.
#include <memory>
#include "coop/locking_weak_table.hpp"


/*
	This header provides a concurrent table of conversion rules,
		allowing values to be copy-converted to other types by
		looking up previously registered functors.
		PLEB uses a global instance of this rulebook.

	A hierarchy of conversion_rule classes is used:
	conversion
		-> convertion_to<R>
			-> conversion_to_from<R, I>
				-> conversion_impl<R Func(I), I>
	
	FUTURE: implement move semantics for conversions.
*/


namespace pleb
{
	class conversion_table;


	namespace detail
	{
		// Implementation: detect parameter type for non-overloaded unary functors
		template<typename F>             struct detect_parameter;
		template<typename R, typename T> struct detect_parameter<std::function<R(T)>> {using type = T;};

		template<typename F> using detect_parameter_t     = typename detect_parameter<decltype(std::function{std::declval<F>()})>::type;
	}


	/*
		Base type for conversion rules.
			The base class allows for type-erased conversion of std::any.
	*/
	class conversion_rule
	{
	public:
		template<typename Result>                  class to;      // to<R>
		template<typename Result,  typename Input> class to_from; // to_from<Result, Input>

		template<typename Functor, typename Input = detail::detect_parameter_t<Functor>>
		class impl;

		// The conversion table in which this conversion is registered.
		std::shared_ptr<conversion_table> table;

	public:
		virtual ~conversion_rule() {}

		virtual const std::type_info &typeid_input () const noexcept = 0;
		virtual const std::type_info &typeid_result() const noexcept = 0;

		/*
			Convert a std::any containing the input type to one containing the result type.
			This is rarely useful.
		*/
		virtual std::any convert_any(const std::any&) const = 0;
	};

	/*
		Base type for conversion functions with a specific result type.
	*/
	template<typename Result>
	class conversion_rule::to : public conversion_rule
	{
	public:
		using result_type = Result;

		const std::type_info &typeid_result() const noexcept final    {return typeid(result_type);}


	public:
		~to() override = default;

		/*
			Convert a std::any to the result type.
			May throw std::bad_any_cast if the parameter contains an unexpected type.
		*/
		virtual result_type convert(const std::any&) const = 0;


		// Implementation of base class
		std::any convert_any(const std::any &x) const override    {return std::any(convert(x));}
	};

	/*
		Base type for conversion functions with a specific result and input type.
	*/
	template<typename Result, typename Input>
	class conversion_rule::to_from : public conversion_rule::to<Result>
	{
	public:
		using result_type = Result;
		using input_type = Input;

		const std::type_info &typeid_input() const noexcept final    {return typeid(input_type);}


	public:
		~to_from() override = default;

		/*
			Convert input type to result type.
		*/
		virtual result_type convert(const input_type&) const = 0;


		// Implementation of base class
		result_type convert(const std::any &x) const override    {return convert(std::any_cast<const input_type&>(x));}
	};

	/*
		Implementation of a conversion rule, based on some functor.
			Usually the parameter type for the functor can be auto-detected.
			Overloaded or generic functors may need the input type specified.
	*/
	template<typename Functor, typename Input /* usually auto-detected */>
	class conversion_rule::impl :
		public conversion_rule::to_from<std::invoke_result_t<Functor, const Input&>, Input>
	{
	private:
		using result_type = std::invoke_result_t<Functor, const Input&>;
		using input_type = Input;

		Functor _f;

	public:
		impl(Functor &&func)    : _f{std::forward<Functor>(func)} {}

		// Implementation
		result_type  convert(const input_type &x) const final    {return _f(x);}
		result_type  convert(const std::any   &x) const final    {return _f(std::any_cast<const input_type&>(x));}
		std::any convert_any(const std::any   &x) const final    {return _f(std::any_cast<const input_type&>(x));}
	};



	/*
		This exception is thrown when conversion_table::get() fails.
	*/
	class no_conversion_rule : public std::runtime_error
	{
	public:
		std::type_index result_type, input_type;

	public:
		no_conversion_rule(std::type_index result_t, std::type_index input_t)
			:
			runtime_error(std::string("No rule to convert from `")
				+ input_t.name() + "' to `" + result_t.name() + "'."),
			result_type(result_t), input_type(input_t) {}
	};


	/*
		A concurrent table of conversion rules.
			Contained rules are weakly referenced and can expire.
	*/
	class conversion_table :
		public std::enable_shared_from_this<conversion_table>
	{
	public:
		using value_type = std::shared_ptr<const conversion_rule>;
		using rule_ptr   = std::shared_ptr<const conversion_rule>;


	public:
		using rule = conversion_rule;

		using pair_type = std::pair<std::type_index, std::type_index>;

		struct pair_hash
		{
			size_t operator()(pair_type k) const noexcept
			{
				auto a = k.first.hash_code(), b = k.second.hash_code();
				return b ^ (a + 0x9e3779b9 + (b<<6) + (b>>2));
			}
		};


	public:
		/*
			Add a type conversion to the table by copy or move.
		*/
		template<typename ConversionFunctor, typename Input = detail::detect_parameter_t<ConversionFunctor>>
		rule_ptr set(ConversionFunctor &&func)
		{
			using impl_type = conversion_rule::impl<std::decay_t<ConversionFunctor>, Input>;
			std::shared_ptr<impl_type> p = std::make_shared<impl_type>(std::forward<ConversionFunctor>(func));
			p->conversion_rule::table = shared_from_this();
			_table.set({p->typeid_result(), p->typeid_input()}, p);
			return p;
		}


		/*
			Perform a type conversion.  You may use std::any or direct values.
				convert(...) throws no_conversion_rule if no rule is defined.
				try_convert(...) returns a default value if no rule is defined.
		*/
		std::any convert(const std::any &x, std::type_index to_type) const                                       {return get(to_type, x.type())->convert_any(x);}
		template<typename To>
		To       convert(const std::any &x) const                                                                {return get<To>(x.type())->convert(x);}
		template<typename To, typename From>
		To       convert(const From     &x) const                                                                {return get<To, From>()->convert(x);}

		std::any try_convert(const std::any &x, std::type_index to_type, const std::any &on_error = {}) const    {auto r=find(to_type,x.type()); if (!r) return on_error; else return r->convert_any(x);}
		std::any try_convert(const std::any &x, const std::any &on_error) const                                  {return try_convert(x, on_error.type(), on_error);}
		template<typename To>
		To       try_convert(const std::any &x, const To  &on_error = {}) const                                  {auto r=find<To>(x.type()); if (!r) return on_error; else return r->convert(x);}
		template<typename To, typename From>
		To       try_convert(const From     &x, const To  &on_error = {}) const                                  {auto r=find<To, From>(); if (!r) return on_error; else return r->convert(x);}
		


		/*
			Look up a conversion rule, supplying types via argument or template.
				find() returns a null pointer on failure.
				get () throws no_conversion_rule on failure.
				Templated calls yield a rule that can work directly with the type.
		*/
		rule_ptr find(std::type_index to, std::type_index from) const noexcept    {return _table.find({to, from});}

		template<typename To> std::shared_ptr<const rule::to<To>>
			find(std::type_index from)                          const noexcept    {return std::static_pointer_cast<const rule::to<To>>(find(typeid(To), from));}

		template<typename To, typename From> std::shared_ptr<const rule::to_from<To, From>>
			find()                                              const noexcept    {return std::static_pointer_cast<const rule::to_from<To, From>>(find(typeid(To), typeid(From)));}


		rule_ptr get (std::type_index to, std::type_index from) const             {auto p=_table.find({to,from}); if (!p) throw no_conversion_rule(to,from); return p;}

		template<typename To> std::shared_ptr<const rule::to<To>>
			get (std::type_index from)                          const             {return std::static_pointer_cast<const rule::to<To>>(get(typeid(To), from));}

		template<typename To, typename From> std::shared_ptr<const rule::to_from<To, From>>
			get ()                                              const             {return std::static_pointer_cast<const rule::to_from<To, From>>(get(typeid(To), typeid(From)));}




	private:
		using _table_t = coop::locking_weak_table<pair_type, const conversion_rule, pair_hash>;

		_table_t _table;
	};
}
