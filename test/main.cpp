#include <iostream>
#include <thread>
#include <variant>

#include <pleb/bind.hpp>
#include <pleb/conversion_map.hpp>
#include <pleb/pleb.hpp>
//#include <pleb/resource.h>

#if 0
class pool_tester;
using test_pool_t = aro::pool<pool_tester>;

static std::shared_ptr<test_pool_t> test_pool;


struct pool_tester
{
	static std::atomic<int> count;
	
	pool_tester()
	{
		++count; for (size_t i = 0; i < 1024; ++i) tedium[i] = i;
	}
	~pool_tester() noexcept
	{
		--count;
	}
	
	size_t tedium[1024];
};

std::atomic<int> pool_tester::count = 0;
#endif


class test_service
{
public:
	void post_void()
	{
		std::cout << "POSTed void to test service!" << std::endl;
	}
	void post_method(pleb::method arg)
	{
		std::cout << "Requested with method: " << int(arg.code) << std::endl;
	}
	void post_int(int arg)
	{
		std::cout << "POSTed int to test service: " << arg << std::endl;
	}
	pleb::status post_method_int(pleb::method arg, int value)
	{
		std::cout << "Requested with method: " << int(arg.code) << ", int value " << value << std::endl;
		return pleb::statuses::NoContent;
	}
};

void test_response_function(pleb::response &response)
{
	std::cout << "\tResponse: " << int(response.status().code)
		<< " " << response.status().reasonPhrase();
	if (response.value().type() != typeid(void))
		std::cout << " with " << response.value().type().name();
	std::cout << std::endl;
}


void printString(const pleb::event &event)
{
	auto *str = event.get<const std::string>();

	if (str) std::cout << event.topic.path() << ": " << *str << std::endl;
	else     std::cout << event.topic.path() << "printString: not a string" << std::endl;
}

namespace
{
	static auto
		int2str = pleb::conversion_define([=](int x) -> std::string {return std::to_string(x);}),
		str2int = pleb::conversion_define([=](const std::string x) -> int {return std::stoi(x);});
}


int main(int argc, char **argv)
{
	try
	{
		const std::string hi_string = "hi";
		auto exampleLambda = [=](int x) {return hi_string;};
		std::cout << "Inferred signature: " << typeid(pleb::detail::detect_parameter_t<decltype(exampleLambda)>).name() << std::endl;


		//std::cout << "Registered conversion function: " << typeid(*stored.get()).name() << std::endl;

		std::cout << "Converts int(5) to: `" << pleb::convert<std::string>(5) << "'" << std::endl;
		std::cout << "Converts any(5) to: `" << pleb::convert<std::string>(std::any(5)) << "'" << std::endl;

		std::cout << "Converts \"5\"s to: `" << pleb::convert<int>(std::string("5")) << "'" << std::endl;

		std::cout << "Attempting a bogus conversion rule..." << std::endl;
		auto x = pleb::convert<int>(nullptr);
	}
	catch (pleb::no_conversion_rule &e)
	{
		std::cout << "pleb::convert failure: " << e.what() << std::endl;
	}

	{
		auto svc = std::make_shared<test_service>();

		auto client = std::make_shared<pleb::client>(&test_response_function);

		pleb::request req(client, "/", pleb::method::POST);

		pleb::bind_service(
			svc, &test_service::post_void, pleb::method::POST
		)(req);
		pleb::bind_service(
			svc, &test_service::post_method, pleb::method::POST
		)(req);
		for (int tries = 0; tries < 2; ++tries)
		{
			std::cout << "Requesting post_int" << std::endl;
			pleb::bind_service(
				svc, &test_service::post_method_int, pleb::method::POST
			)(req);
			req.value().emplace<int>(13);
			std::cout << "\tRequest now holds " << req.value().type().name()
				<< ": " << std::any_cast<int>(req.value()) << std::endl;
		}
		

	}

	//test_pool = test_pool_t::create();
	std::string_view test_strings[] =
	{
		"tetrahedron",
		"tetra/hedron",
		"midi/in_11//sx7/beg/",
		"///bug/in//code?///"
	};

	for (auto str : test_strings)
	{
		std::cout << "\tPath: `" << str << "':" << std::endl;
		for (auto part : pleb::topic_view(str))
		{
			std::cout << "\t\t* `" << part << "'" << std::endl;
		}
	}

	//if (false)
	{
		auto pool = coop::unmanaged::pool<std::string>();

		std::shared_ptr<std::string> strings[] =
		{
			pool.emplace("string1"),
			pool.emplace("string2 boogaloo"),
			pool.emplace("string3 extra fancy"),
			pool.emplace("string4 the inevitable end of all things"),
		};

		std::cout << "String unmanaged pool test:" << std::endl;
		for (auto &i : strings)
			std::cout << "\t`" << *i << "'" << std::endl;
	}

	{
		auto canPrintString = pleb::subscribe("print/string", &printString);

		pleb::publish("print/string", pleb::statuses::OK, std::string("this is a fancy string"));
	}
}
