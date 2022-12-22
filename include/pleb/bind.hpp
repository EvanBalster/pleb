#pragma once

#include <typeindex>
#include <tuple>

#include "bind_traits.hpp"


/*
	This header generates binding code for services and subscribers.
		Where a standard PLEB function takes a message reference, bound
		methods may have signatures like:
			status my_service(method m, my_container &&data);
		or
			void on_POST_timecode(std::time_t time);
	
	Binding code handles various common concerns:
	* Cast message content, responding "Unsupported Media Type" on failure.
	* Implement the OPTIONS method and "Method Not Allowed" replies.
	* Weak pointers ensure called objects have not been destroyed.
	
	The binding code is fashioned to impose as few stack frames as possible,
		in order to simplify debugging of PLEB calls.
		This accounts for some code duplication in this header file.
*/


namespace pleb
{
	/*
		Implement a single service method using a method of some class.
			This is commonly used for GET and POST.
			The object must be supplied as a weak reference.
			Non-heap-allocated objects can provide this with weak_anchor.

		class_instance          -- weak ptr to called object.  Use weak_anchor for non-heap objects.
		allowed_request_methods 
	*/
	template<class T, typename Return, typename... Params>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		Return      (T::*class_method)(Params...),
		method_set       allowed_request_methods,
		status           default_response_status = statuses::OK);

	template<class T, typename... Params>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		void        (T::*class_method)(Params...),
		method_set       allowed_request_methods,
		status           default_response_status = statuses::OK);

	template<class T, typename Return>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		Return      (T::*class_method)(request&),
		method_set       allowed_request_methods,
		status           default_response_status = statuses::OK);

	template<class T>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		void        (T::*class_method)(request&),
		method_set       allowed_request_methods,
		status           default_response_status = statuses::NoContent);

	// Allow the above methods to be called with a shared_ptr.
	template<class T, typename ... Args,    typename Valid = std::void_t<decltype(bind_service(std::weak_ptr<T>(), std::declval<Args>()...))>>
	inline auto bind_service(const std::shared_ptr<T> &class_instance, Args&& ... other_args)
	{
		// A compile
		return bind_service(std::weak_ptr(class_instance), std::forward<Args>(other_args)...);
	}



	namespace detail
	{
		// Helper method for implementing OPTIONS
		void respond_to_misc_method(request &r, method_set allowed)
		{
			switch (r.method())
			{
			case method::OPTIONS: r.respond_OK(allowed + method::OPTIONS); break;

			default:
				if (allowed.contains(r.method())) r.respond_NotImplemented();
				else                              r.respond_MethodNotAllowed();
			}
		}

		template<typename T>
		std::shared_ptr<T> svc_lock_(
			std::weak_ptr<T> w, request &r, method_set implemented)
		{
			auto s = w.lock();
			if (!s) r.respond_Gone();
			else if (!implemented.contains(r.method()))
			{
				respond_to_misc_method(r, implemented);
				s.reset();
			}
			return s;
		}

		template<typename ResponseValue>
		void respond_with(pleb::request &r, ResponseValue &&v, status default_status)
		{
			using type = std::decay_t<ResponseValue>;
			if      constexpr (std::is_same_v<type, status  >) r.respond(v);
			else if constexpr (std::is_same_v<type, response>) r.respond(v.status(), std::move(v.value()));
			else                                               r.respond(default_status, std::forward<ResponseValue>(v));
		}
	}


	template<class T, typename Return, typename... Params>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		Return      (T::*class_method)(Params...),
		method_set       allowed_request_methods,
		status           default_response_status)
	{
		return [allowed_request_methods, default_response_status,
			class_instance=std::move(class_instance), class_method](request &request)
		{
			// Check the method and secure the service class_instance.
			bool issuing_response = false;
			if (auto s = detail::svc_lock_(class_instance, request, allowed_request_methods))
				try
			{
				// Call the served method.
				Return v = (s.get()->*class_method) (detail::msg_decompose<Params>::pass(request)...);
				issuing_response = true;
				detail::respond_with(request, std::forward<Return>(v), default_response_status);
			}
			catch (std::bad_any_cast&)
			{
				if (issuing_response) throw; // (rethrow)
				else request.respond_UnsupportedMediaType();
			}
		};
	}

	template<class T, typename... Params>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		void        (T::*class_method)(Params...),
		method_set       allowed_request_methods,
		status           default_response_status)
	{
		return [allowed_request_methods, default_response_status,
			class_instance=std::move(class_instance), class_method](request &request)
		{
			// Check the method and secure the service class_instance.
			if (auto s = detail::svc_lock_(class_instance, request, allowed_request_methods))
				try
			{
				// Call the served method.
				(s.get()->*class_method) (detail::msg_decompose<Params>::pass(request)...);
				request.respond(default_response_status);
			}
			catch (std::bad_any_cast&)
			{
				request.respond_UnsupportedMediaType();
			}
		};
	}

	template<class T, typename Return>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		Return      (T::*class_method)(request&),
		method_set       allowed_request_methods,
		status           default_response_status)
	{
		return [allowed_request_methods, default_response_status,
			class_instance=std::move(class_instance), class_method](request &request)
		{
			// Check the method and secure the service class_instance.
			bool issuing_response = false;
			if (auto s = detail::svc_lock_(class_instance, request, allowed_request_methods))
				try
			{
				// Call the served method.
				Return v = (s.get()->*class_method) (request);
				issuing_response = true;
				detail::respond_with(request, std::forward<Return>(v), default_response_status);
			}
			catch (std::bad_any_cast&)
			{
				if (issuing_response) throw; // (rethrow)
				else request.respond_UnsupportedMediaType();
			}
		};
	}

	template<class T>
	inline auto bind_service(
		std::weak_ptr<T> class_instance,
		void        (T::*class_method)(request&),
		method_set       allowed_request_methods,
		status           default_response_status)
	{
		return [allowed_request_methods, default_response_status,
			class_instance=std::move(class_instance), class_method](request &request)
		{
			// Check the method and secure the service class_instance.
			if (auto s = detail::svc_lock_(class_instance, request, allowed_request_methods))
				try
			{
				// Call the served method.
				(s.get()->*class_method) (request);

				// Default response
				if (!(request.features & flags::did_respond))
					request.respond(default_response_status);
			}
			catch (std::bad_any_cast&)
			{
				request.respond_UnsupportedMediaType();
			}
		};
	}
}
