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
#include <../../bytestream/bytestream.h>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;
using namespace Olagarro;

// A simple testing function
void test(bool condition, const string& testName)
{
	cout << (condition? "PASSED" : "\n## FAILED ##") << " TEST " << testName << (condition? "" : "\n") << endl;
}

const size_t ArrayLength = 10;

struct FooData
{
	// Constructor fills data with random values
	FooData()
	{
		a = rand();
		b = static_cast<size_t>(rand());
		c = rand() % 255;
		d = (static_cast<double>(rand()) / RAND_MAX) * 5000;

		for(size_t i = 0; i < ArrayLength; ++ i)
		{
			e[i] = rand() % 255;
		}
	}

	int a;
	size_t b;
	char c;
	double d;
	char e[ArrayLength];

	bool operator == (const FooData& other) const
	{
		return a == other.a &&
							b == other.b &&
							c == other.c &&
							d == other.d &&
							memcmp(e, other.e, ArrayLength) == 0;
	}
};


const string FileName = "test.bin";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void testSizeManaging()
{
	const char buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

	ByteStream inMemorydata;

	inMemorydata << buffer;

	test(inMemorydata.readIndex() == 0, "Beginning: Read index == 0");
	test(inMemorydata.size() == 10, "Beginning: Size == 10");

	char recoveredData;

	inMemorydata >> recoveredData;

	test(inMemorydata.readIndex() == 1, "After 1 reading: Read index == 1");
	test(inMemorydata.bytesRemaining() == 9, "After 1 reading: Bytes ramaining == 9");
	test(inMemorydata.canReadMore(), "After 1 reading: Can read more data == true");

	for(size_t i = 0; i < 9; ++ i)
	{
		inMemorydata >> recoveredData;
	}

	test(inMemorydata.readIndex() == 10, "After 10 readings: Read index == 10");
	test(inMemorydata.bytesRemaining() == 0, "After 10 readings: Bytes ramaining == 0");
	test(!inMemorydata.canReadMore(), "After 10 readings: Can read more data == false");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void testDataInsertionAndRecovery()
{
	// Create some data
	const FooData ReferenceData;

	// Add it to a ByteStream object
	ByteStream inMemoryData;

	inMemoryData << ReferenceData;

	// Check if we can recover it with the correct data
	FooData recoveredData;

	inMemoryData >> recoveredData;

	test(ReferenceData == recoveredData, "Data insertion and recovery");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
	ByteStream needs to know how to deal with string objects so we provide the neccesary operators. In this case
	we will first store the size of the string in a size_t and then the string's characters.
*/
ByteStream& operator << (ByteStream& stream, const string& aString)
{
	stream << static_cast<size_t>(aString.size());
	stream.addData(&aString[0], aString.size());

	return stream;
}

ByteStream& operator >> (ByteStream& stream, string& aString)
{
	// Get the size and then the characters
	size_t charNumber;

	stream >> charNumber;

	aString.resize(charNumber);

	// This implementation replaces all content "aString" had before with the new data
	stream.readData(&aString[0], charNumber);

	return stream;
}

void testStrings()
{
	const string ReferenceValue("I'm a string");

	ByteStream inMemoryData;

	inMemoryData << ReferenceValue;

	string recoveredData;

	inMemoryData >> recoveredData;

	test(ReferenceValue == recoveredData, "String");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Like in the strings test we need to tell ByteStream how to deal with vector.
template<typename T>
ByteStream& operator << (ByteStream& stream, const vector<T>& aVector)
{
	// Like with the string we save first the size and the the contents
	stream << static_cast<size_t>(aVector.size());

	for(size_t i = 0; i < aVector.size(); ++ i)
	{
		stream << aVector[i];
	}

	return stream;
}

template<typename T>
ByteStream& operator >> (ByteStream& stream, vector<T>& aVector)
{
	size_t elementNumber;

	stream >> elementNumber;

	aVector.resize(elementNumber);

	for(size_t i = 0; i < aVector.size(); ++ i)
	{
		stream >> aVector[i];
	}

	return stream;
}

string generateString()
{
	stringstream str;

	str << "I'm string " << rand();

	return str.str();
}

void testVectors()
{
	// vector<int>
	{
		vector<int> referenceData(1024);

		generate(referenceData.begin(), referenceData.end(), rand);

		ByteStream inMemoryData;

		inMemoryData << referenceData;

		vector<int> recoveredData;

		inMemoryData >> recoveredData;

		test(referenceData == recoveredData, "vector<int>");
	}

	// vector<string>
	{
		vector<string> referenceData(1024);

		generate(referenceData.begin(), referenceData.end(), generateString);

		ByteStream inMemoryData;

		inMemoryData << referenceData;

		vector<string> recoveredData;

		inMemoryData >> recoveredData;

		test(referenceData == recoveredData, "vector<string>");
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void testFileFunctions()
{
	// Create some data
	const FooData Foo;

	vector<string> strings(1024);
	generate(strings.begin(), strings.end(), generateString);

	// Write it to a binary file
	{
		ByteStream output;

		output << Foo;
		output << strings;

		ByteStream::saveToFile(output, FileName);
	}

	// Load the file and check if it has the correct data
	{
		ByteStream input;

		if(ByteStream::loadFromFile(input, FileName))
		{
			FooData loadedFoo;
			vector<string> loadedStrings;

			input >> loadedFoo;
			input >> loadedStrings;

			test(Foo == loadedFoo && strings == loadedStrings, "Data saving and loading from a file");
		}
		else
		{
			cout << "Cannot load from " << FileName << " file\n";
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	srand(time(0));

	testSizeManaging();

	testDataInsertionAndRecovery();

	testStrings();

	testVectors();

	testFileFunctions();

	return 0;
}

