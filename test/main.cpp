#include <iostream>
#include <iomanip>
#include <thread>
#include <variant>

#include <pleb/bind.hpp>
#include <pleb/conversion_map.hpp>
#include <pleb/pleb.hpp>
//#include <pleb/resource.h>

using namespace pleb::literals;

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
		int2str = pleb::conversion_define([](int x) -> std::string {return std::to_string(x);}),
		str2int = pleb::conversion_define([](const std::string x) -> int {return std::stoi(x);});
}


template<int I>
struct pseudo_int
{
	constexpr operator int() const    {return I;}
};

constexpr pseudo_int<0> pseudo_ZERO;
constexpr pseudo_int<1> pseudo_ONE;
constexpr pseudo_int<2> pseudo_TWO;


int main(int argc, char **argv)
{
	using pleb::std_any::any;
	using pleb::std_any::any_cast;
	
	switch (argc)
	{
		case pseudo_ZERO: std::cout << "NO ARGS" << std::endl; break;
		case pseudo_ONE: std::cout << "ONE ARG" << std::endl; break;
		case pseudo_TWO: std::cout << "TWO ARGS" << std::endl; break;
		default: std::cout << "MANY ARGS" << std::endl; break;
	}


	try
	{
		const std::string hi_string = "hi";
		auto exampleLambda = [=](int x) {return hi_string;};
		std::cout << "Inferred signature: " << typeid(pleb::detail::detect_parameter_t<decltype(exampleLambda)>).name() << std::endl;


		//std::cout << "Registered conversion function: " << typeid(*stored.get()).name() << std::endl;

		std::cout << "Converts int(5) to: `" << pleb::convert<std::string>(5) << "'" << std::endl;
		std::cout << "Converts any(5) to: `" << pleb::convert<std::string>(any(5)) << "'" << std::endl;

		std::cout << "Converts \"5\"s to: `" << pleb::convert<int>(std::string("5")) << "'" << std::endl;

		std::cout << "Attempting a bogus conversion rule..." << std::endl;
		auto x = pleb::convert<int>(nullptr);
	}
	catch (pleb::no_conversion_rule &e)
	{
		std::cout << "pleb::convert failure: " << e.what() << std::endl;
	}

	auto svc_test = std::make_shared<test_service>();
	auto svc_test_void = pleb::serve("test/void",   {svc_test, &test_service::post_void,   pleb::method::POST});
	auto svc_test_int  = pleb::serve("test/int",    {svc_test, &test_service::post_int,    pleb::method::POST});
	auto svc_test_meth = pleb::serve("test/method", {svc_test, &test_service::post_method, pleb::method::POST});

	auto svc_test_proxy = pleb::forward_requests("test/proxy", "test/method");

	{
		auto client = std::make_shared<pleb::client>(&test_response_function);

		"test/void"_topic  .POST(client);
		"test/method"_topic.POST(client);

		pleb::POST("test/void",             client);
		pleb::POST("test/method"_topic,     client);
		pleb::POST("test/proxy"_topic_path, client);

		pleb::response resp = pleb::POST("test/method");
		pleb::response resp2 = pleb::POST({"test", "method"});

		any int_holder;
		for (int tries = 0; tries < 2; ++tries)
		{
			std::cout << "Requesting post_int (value: "
				<< (int_holder.has_value() ? "13" : "none")
				<< ")" << std::endl;
			pleb::POST("test/int", client, int_holder);
			int_holder = 13;
			std::cout << "\tRequest now holds " << int_holder.type().name()
				<< ": " << any_cast<int>(int_holder) << std::endl;
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
		pleb::topic_path topic_path_pre = str;
		pleb::topic      topic_exact = str;
		pleb::topic_path topic_path_post = str;

		std::cout << "\tPath: `" << str << "':" << std::endl;
		std::cout << "\ttopic_path pre: `" << topic_path_pre.path() << "':" << std::endl;
		std::cout << "\ttopic realized: `" << topic_exact.path() << "':" << std::endl;
		std::cout << "\ttopic_path post:`" << topic_path_post.path() << "':" << std::endl;
		for (auto part : pleb::topic_view(str))
		{
			std::cout << "\t\t* `" << part << "'" << std::endl;
		}
	}


	{
		const char *match_phrases[] =
		{
			"apple/1",
			"apple/1/core",
			"apple/1/core/seed",
			"banana/2",
			"truck/5"
		};

		std::cout << "Topic matching test with pattern [fruit]/*" << std::endl;
		for (auto phrase : match_phrases)
		{
			std::cout << "\t" << std::setw(20) << phrase;
			std::string_view id;
			if (pleb::topic_view(phrase).match({"apple", "banana"}, &id))
				std::cout << " matched with id " << id << std::endl;
			else
				std::cout << " did not match" << std::endl;
		}

		std::cout << "Topic matching test with pattern apple/* ..." << std::endl;
		for (auto phrase : match_phrases)
		{
			std::cout << "\t" << std::setw(20) << phrase;
			std::string_view id, rest;
			if (pleb::topic_view(phrase).match("apple", &id, pleb::etc, &rest))
				std::cout << " matched with id " << id << ", tail " << rest << std::endl;
			else
				std::cout << " did not match" << std::endl;
		}

		std::cout << "Topic matching test with pattern apple/*/core" << std::endl;
		for (auto phrase : match_phrases)
		{
			std::cout << "\t" << std::setw(20) << phrase;
			std::string_view id;
			if (pleb::topic_view(phrase).match("apple", nullptr, "core"))
				std::cout << " matched (ignored the ID)" << std::endl;
			else
				std::cout << " did not match" << std::endl;
		}
	}


	if (false)
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
