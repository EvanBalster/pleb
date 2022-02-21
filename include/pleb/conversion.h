#pragma once


#include "pleb_base.h"


namespace pleb
{
	/*
		Functions which attempt to derive a pointer to T from std::any.
			These allow T to be supplied by value or a shared_ptr.
	*/
	template<typename T>
	T *any_ptr(const std::any &value)
	{
		if (auto t = std::any_cast<std::shared_ptr<T>>(&value)) return &**t;
		//if (auto t = std::any_cast<T*>                (&value)) return *t;
		return nullptr;
	}
	template<typename T>
	T *any_ptr(std::any &value)
	{
		if (auto t = std::any_cast<T>(&value)) return t;
		return any_ptr<T>((const std::any&) value);
	}
	template<typename T>
	const T *any_const_ptr(const std::any &value)
	{
		if (auto t = std::any_cast<T>                       (&value)) return t;
		if (auto t = std::any_cast<std::shared_ptr<const T>>(&value)) return &**t;
		//if (auto t = std::any_cast<const T*>                (&value)) return &**t;
		return any_ptr<T>(value);
	}

	/*
		Value conversion functions.
	*/
	template<typename T>
	T convert(const std::any &source)
	{
		// Try a direct copy from a view of the value.
		if (auto *ptr = any_const_ptr(source)) return *ptr;
		throw std::bad_any_cast();
	}

	template<typename T>
	bool try_convert(const std::any &source, T &destination)
	{
		// Try a direct view of the value
		if (auto *v = any_const_ptr(source)) destination = *v;
		return false;
	}

	template<typename T>
	bool try_update(const std::any &source, T &destination)
	{
		// Attempt overwrite
		if (try_convert(source, destination)) return true;
		return false;
	}
}
