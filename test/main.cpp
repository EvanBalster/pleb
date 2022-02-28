#include <iostream>
#include <thread>
#include <variant>

//#include <pleb/aro_pool.h>
#include <pleb/pleb.h>
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


void printString(const pleb::event &event)
{
	auto *str = event.get<const std::string>();

	if (str) std::cout << event.resource.topic() << ": " << *str << std::endl;
	else     std::cout << event.resource.topic() << "printString: not a string" << std::endl;
}


int main(int argc, char **argv)
{
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

		pleb::publish("print/string", std::string("this is a fancy string"));
	}
}
