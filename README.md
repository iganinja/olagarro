Olagarro C++ utility library
========

![Olagarro logo](https://raw.githubusercontent.com/iganinja/olagarro/master/doc/logo.png)

Olagarro is a C++ utility library which aims to provide small, easily usable, portable and useful utilities.

License is zlib/libpng (http://opensource.org/licenses/zlib-license.php)

# Concurrency module

Concurrency module provides some facilities to take advantage of multiple CPUs of modern machines in a very easy and straightforward way. In some way it can be seen as a C++ 2003 version of C++11's std::async.

Module presents basically 2 facilities: launchJob and concurrentFor in various flavours:

```CPP
#include "olagarro/concurrency/concurrency.h"

using namespace Olagarro;

float calculateSomeValue(const std::vector<float>& someData)
{
	float valueResult = 0.0f;
	// ...
	return valueResult;
}

void otherFunction()
{
	std:vector<float> data(getSomeData());
	Concurrency::Future<float> someValue = Concurrency::launchJob(calculateSomeValue, data);
	std::cout << "Result is = " << someValue.result() << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void addOK(int index, std::string& str)
{
	str += " OK";
}

void otherFunction2()
{
	std::vector<std::string> messages(getMessages());
	Concurrency::Future<void> messagesFuture = Concurrency::concurrentFor(messages.begin(), messages.end(), addOK);
	messagesFuture.result(); // Block until all processing is done
	std::cout << "All strings processed\n";
}
```

For more details take a look at the documentation.

# ByteStream class

The ByteStream class is a wrapper around a vector of bytes which eases the manipulation of the data stored on it. It offers convenient methods to insert and extract data with different data types and to save to and load data from files in a very simple way.

```CPP

#include "olagarro/bytestream/bytestream.h"

using namespace Olagarro;

int a = 10;
float b[128];
SomeStruct c;

Olagarro::ByteStream data;

data << a << b << c;

ByteStream::saveToFile(data, "data.bin");

....

ByteStream fileData;

ByteStream::loadFromFile(fileData, "data.bin");

std::cout << "Loaded bytes = " << fileData.size() << std::endl;

data >> a >> b >> c;

```

# Signal/slot system

A very simple signal/slot implementation in C++2003 where you can connect events with functions, methods or functors:

```CPP

#include "olagarro/signalslot/signalslot.h"

using namespace Olagarro;

class Observer
{
public:
	void somethingHappened(int withThisId, const std::string& withThisMessage)
	{
		std::cout << "Something happened: id = " << withThisId << " message = " << withThisMessage << std::endl;
	}
};

Observer observer;

SignalSlot::Signal2<int, const std::string&> enemyDestroyed;

SignalSlot::Slot2<Observer, int, const std::string&> onSomethingHappened(observer, &Observer::somethingHappened);

onSomethingHappened.connect(enemyDestroyed);

enemyDestroyed.fire(123, "An enemy has been destroyed"); --> Calls Observer::somethingHappened with those parameters

```

It allows to connect N signals with M slots, disconnect/connect them in run time and manages automatic disconnection when a signal or a slot is destroyed.