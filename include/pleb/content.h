#pragma once


#ifdef PLEB_REPLACEMENT_ANY_HEADER
	#include PLEB_REPLACEMENT_ANY_HEADER
#else
	#include <any>
#endif


namespace pleb
{
#ifdef PLEB_REPLACEMENT_ANY_NAMESPACE
	namespace std_any = ::PLEB_REPLACEMENT_ANY_NAMESPACE;
#else
	namespace std_any = ::std;
#endif

	/*
		Functions which attempt to derive a pointer to T from std::any.
			(note std_any namespace alias for substitute implementations of std::any)
			These allow T to be supplied by value or a shared_ptr.
	*/
	template<typename T>
	T *any_ptr(const std_any::any &value)
	{
		if (auto t = std_any::any_cast<std::shared_ptr<T>>(&value)) return &**t;
		//if (auto t = std_any::any_cast<T*>                (&value)) return *t;
		return nullptr;
	}
	template<typename T>
	T *any_ptr(std_any::any &value)
	{
		if (auto t = std_any::any_cast<T>(&value)) return t;
		return any_ptr<T>((const std_any::any&) value);
	}
	template<typename T>
	const T *any_const_ptr(const std_any::any &value)
	{
		if (auto t = std_any::any_cast<T>                       (&value)) return t;
		if (auto t = std_any::any_cast<std::shared_ptr<const T>>(&value)) return &**t;
		//if (auto t = std_any::any_cast<const T*>                (&value)) return &**t;
		return any_ptr<T>(value);
	}

	/*
		Value conversion functions.
	*/
	template<typename T>
	T copy_as(const std_any::any &source)
	{
		// Try a direct copy from a view of the value.
		if (auto *ptr = any_const_ptr<T>(source)) return *ptr;
		throw std_any::bad_any_cast();
	}

	template<typename T>
	bool try_copy_into(const std_any::any &source, T &destination)
	{
		// Try a direct view of the value
		if (auto *v = any_const_ptr<T>(source)) {destination = *v; return true;}
		return false;
	}


	/*
		This class represents the content of a message.
	*/
	class content
	{
	private:
		std_any::any _value;


	public:
		content(
			std::any &&value)
			:
			_value(std::move(value)) {}


		// Access the value's generic container.
		std::any       &value()       noexcept    {return _value;}
		const std::any &value() const noexcept    {return _value;}


		// Attempt to move the contained value.  Throws any_cast on failure.
		template<class T> T        move_as()                       {return std::any_cast<T>(std::move(_value));}


		// Access value as a specific type.  Only succeeds if the type is an exact match.
		template<class T> const T *value_cast()  const noexcept    {return std_any::any_cast<T>(&_value);}
		template<class T> T       *value_cast()        noexcept    {return std_any::any_cast<T>(&value);}

		// Get a constant pointer to the value.
		//  This method automatically deals with indirect values.
		template<class T> const T *get()         const noexcept    {return pleb::any_const_ptr<T>(_value);}

		// Access a mutable pointer to the value.
		//  This method automatically deals with indirect values.
		//  This will fail when a const request holds a value directly.
		template<class T> T       *get_mutable() const noexcept    {return pleb::any_ptr<T>(_value);}
		template<class T> T       *get_mutable()       noexcept    {return pleb::any_ptr<T>(_value);}
	};
}
