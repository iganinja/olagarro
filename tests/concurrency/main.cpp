/*
Copyright (c) 2014 Inaki Griego

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.

		2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		distribution.
*/


#include <iostream>
#include <vector>
#include <cstdlib>

#include <limits>

#include "../../concurrency/concurrency.h"

#include <stdexcept>

// Class and functions used by test code

struct Multiply
{
	Multiply(float factor = 5) :
		factor(factor) {}

	void operator()(int /*index*/, float& value)
	{
		value *= factor;
	}

	float factor;
};

struct FindMax
{
	FindMax() : maxValue(-std::numeric_limits<float>::max()) {}

	void operator()(int /*index*/, float& value)
	{
		maxValue = std::max(value, maxValue);
	}

	void merge(const FindMax& other)
	{
		maxValue = std::max(other.maxValue, maxValue);
	}

	float maxValue;
};


float test()
{
	const int ElementNumber = 10000000;

	std::vector<float> numbers;

	for(int i = 0; i < ElementNumber; ++ i)
	{
		numbers.push_back(i / 2.01 + 4.45 * i);
		for(int j = 0; j < 1; ++ j)
		{
			numbers[i] /= 2.01 + (i + 1) / 4.45;
			numbers[i] *= 10;
		}
	}

	float maxValue = -std::numeric_limits<float>::max();

	for(std::size_t i = 0; i < numbers.size(); ++ i)
	{
		maxValue = std::max(maxValue, numbers[i]);
	}

	return maxValue;
}

void testVoid()
{
	test();
}

float testParam(int /*param*/)
{
	return test();
}

float testParamVector(const std::vector<int>& /*param*/)
{
	return test();
}

float testParamNoConst(std::vector<int>& param)
{
	param.resize(0);

	return test();
}

struct ConstFunctor
{
	float operator () () const
	{
		return test();
	}
};

struct NoConstFunctor
{
	NoConstFunctor(int param) : param(param) {}

	float operator () ()
	{
		return test();
	}

	int param;
};

struct Test
{
	float operator () () const
	{
		return test();
	}
};

class TestClass
{
public:
	float ZeroParam()
	{
		return test();
	}

	float OneParamCopy(int /*a*/)
	{
		return test();
	}

	float OneParamConst(const std::string& /*a*/)
	{
		return test();
	}

	float OneParamNoConst(std::string& /*a*/)
	{
		return test();
	}

};


int main(int /*argc*/, char** /*argv*/)
{
	using namespace Olagarro::Concurrency;

	// Some tests are doable comparing values, others not, like the testVoid() call. Also if the app finishes (it takes less than 20 seconds in my machine)
	// it passes the thread managing test by not getting blocked forever

	const float testResult = test();

	/////////////////////////////////////////////////////////////////
	// FREE FUNCTIONS CALLS
	/////////////////////////////////////////////////////////////////

	std::cout << "--------------------------------------------------------\n";
	std::cout << "Free functions tests\n";
	std::cout << "--------------------------------------------------------\n";

	// Test 1: float test() call
	Future<float> test1 = launchJob(test);
	assert(testResult == test1.result() && "Invalid returned value in test1");

	// Test 2: void testVoid() call
	Future<void> test2 = launchJob(testVoid);
	test2.result();

	// Test 3: float testParam(int param) call. It uses the copy version
	Future<float> test3 = launchJob(testParam, 666);
	test3.result();

	assert(testResult == test3.result() && "Invalid returned value in test3");

	// Test 4: float testParamVector(const std::vector<int>& param) call. It uses the const& version
	Future<float> test4 = launchJob(testParamVector, std::vector<int>(10));
	test4.result();

	assert(testResult == test4.result() && "Invalid returned value in test4");

	// Test 5: float testParamNoConst(std::vector<int>& param) call. It uses the no const& version
	std::vector<int> testVector(10);
	Future<float> test5 = launchJob(testParamNoConst, testVector);
	test5.result();

	assert(testResult == test5.result() && "Invalid returned value in test5");
	assert(testVector.empty() && "Invalid testVector size in test5");

	std::cout << "OK" << std::endl;


	/////////////////////////////////////////////////////////////////
	// FUNCTOR TEST
	/////////////////////////////////////////////////////////////////

	std::cout << "--------------------------------------------------------\n";
	std::cout << "Functor tests\n";
	std::cout << "--------------------------------------------------------\n";

	// Test 6: Call constant functor
	Future<float> test6 = launchJob<float>(ConstFunctor());

	assert(testResult == test6.result() && "Invalid returned value in test6");

	// Test 7: Call no constant functor
	NoConstFunctor noConstFunctor(200);
	Future<float> test7 = launchJob<float>(noConstFunctor);

	assert(testResult == test7.result() && "Invalid returned value in test7");

	std::cout << "OK" << std::endl;


	/////////////////////////////////////////////////////////////////
	// MEMBER FUNCTIONS CALLS
	/////////////////////////////////////////////////////////////////

	std::cout << "--------------------------------------------------------\n";
	std::cout << "Member function tests\n";
	std::cout << "--------------------------------------------------------\n";

	TestClass testClass;

	Future<float> test8 = launchJob(testClass, &TestClass::ZeroParam);
	test8.result();

	assert(testResult == test8.result() && "Invalid returned value in test8");

	Future<float> test9 = launchJob(testClass, &TestClass::OneParamCopy, 10);
	test9.result();

	assert(testResult == test9.result() && "Invalid returned value in test9");

	Future<float> test10 = launchJob(testClass, &TestClass::OneParamConst, std::string("testString const&"));
	test10.result();

	assert(testResult == test10.result() && "Invalid returned value in test10");

	std::string b("testString &");
	Future<float> test11 = launchJob(testClass, &TestClass::OneParamNoConst, b);
	test11.result();

	assert(testResult == test11.result() && "Invalid returned value in test11");

	// Test 12: Futures in a container
	std::vector< Future<float> > resultsTest12(10);

	for(std::size_t i = 0; i < resultsTest12.size(); ++ i)
	{
		resultsTest12[i] = launchJob(test);
	}

	// Jobs launched, wait for and check them
	for(std::size_t i = 0; i < resultsTest12.size(); ++ i)
	{
		assert(testResult == resultsTest12[i].result() && "Invalid returned value in test12");
	}

	std::cout << "OK" << std::endl;

	/////////////////////////////////////////////////////////////////
	// CONCURRENT FOR CALLS
	/////////////////////////////////////////////////////////////////

	const int ValueNumber = 1024 * 1024;
	std::vector<float> values;
	values.reserve(ValueNumber);

	std::cout << "--------------------------------------------------------\n";
	std::cout << "concurrentFor() tests\n";
	std::cout << "--------------------------------------------------------\n";

	for(int i = 0; i < ValueNumber; ++ i)
	{
		values.push_back(i);
	}

	// Test 13: concurrentFor call
	Future<void> test13 = concurrentFor(values.begin(), values.end(), Multiply(10));
	test13.result();

	for(std::size_t i = 0; i < values.size(); ++ i)
	{
		assert(values[i] == i * 10 && "Invalid value in test13");
	}

	// Test 13: concurrentReductor call
	Future<FindMax> test14 = concurrentReductorFor(values.begin(), values.end(), FindMax());

	assert(test14.result().maxValue == values.back() && "Invalid value in test14");

	std::cout << "OK" << std::endl;

	return 0;
}
