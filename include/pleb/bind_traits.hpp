#pragma once


#include <functional>
#include <type_traits>

#include "request.hpp"
#include "event.hpp"



namespace pleb
{
	namespace detail
	{
		template<typename T, typename enable = std::enable_if_t<!std::is_base_of_v<message, T>>>
		struct msg_decompose
		{
			static constexpr bool uses_value = true;

			static T        pass(      message &m)    {return m.move_as<T>();}
			static const T& pass(const message &m)    {auto p=m.get<T>();         if (!p) throw std::bad_any_cast(); return p;}

			static T&       view(      message &m)    {auto p=m.get_mutable<T>(); if (!p) throw std::bad_any_cast(); return p;}
		};

		template<typename T>
		struct msg_decompose<T, std::enable_if_t<std::is_same_v<std::any, std::decay_t<T>>>>
		{
			static constexpr bool uses_value = true;

			static T        pass(      message &m)    {return m.move_as<T>();}
			static const T& pass(const message &m)    {auto p=m.get<T>();         if (!p) throw std::bad_any_cast(); return p;}

			static T&       view(      message &m)    {auto p=m.get_mutable<T>(); if (!p) throw std::bad_any_cast(); return p;}
		};

		template<>
		struct msg_decompose<method, void>
		{
			static constexpr bool uses_value = false;

			static const method pass(const request &m)    {return m.method();}
		};

		template<>
		struct msg_decompose<status, void>
		{
			static constexpr bool uses_value = false;

			static const status pass(const event    &m)    {return m.status();}
			static const status pass(const response &m)    {return m.status();}
		};


		template<typename T, typename Msg>
		auto pass_message(Msg &m) -> decltype(msg_decompose<T>::pass(std::declval<Msg>()))
			{return msg_decompose<T>::pass(m);}



		template<typename Message, typename StdFunctionForCallable>
		struct valid_handler_impl
		{
			static constexpr bool value = false;
		};

		template<typename Message, typename Return, typename... Params>
		struct valid_handler_impl<Message, std::function<Return(Params...)>>
		{
			static constexpr bool value =
				((0 + ... + msg_decompose<Params>::uses_value) <= 1);
		};
	}
}
