/*
Copyright (c) 2015 Inaki Griego

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
#include "../../signalslot/signalslot.h"

using namespace Olagarro;
using namespace SignalSlot;

class Functor
{
public:
	void operator()()
	{
		std::cout << "Functor0()\n";
	}
	void operator()(int param1)
	{
		std::cout << "Functor1(" << param1 << ")\n";
	}
	void operator()(int param1, int param2)
	{
		std::cout << "Functor2(" << param1 << ", " << param2 << ")\n";
	}
	void operator()(int param1, int param2, int param3)
	{
		std::cout << "Functor3(" << param1 << ", " << param2 << ", " << param3 << ")\n";
	}
	void operator()(int param1, int param2, int param3, int param4)
	{
		std::cout << "Functor4(" << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ")\n";
	}
	void operator()(int param1, int param2, int param3, int param4, int param5)
	{
		std::cout << "Functor5(" << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ")\n";
	}
};

class Test
{
public:
	Test() :
		mSlot0(*this, &Test::callMe),
		mSlot1(*this, &Test::callMe),
		mSlot2(*this, &Test::callMe),
		mSlot3(*this, &Test::callMe),
		mSlot4(*this, &Test::callMe),
		mSlot5(*this, &Test::callMe)
	{
	}
	Test(Signal0& signal0,
			Signal1<int>& signal1,
			Signal2<int, int>& signal2,
			Signal3<int, int, int>& signal3,
			Signal4<int, int, int, int>& signal4,
			Signal5<int, int, int, int, int>& signal5) :
		mSlot0(*this, &Test::callMe),
		mSlot1(*this, &Test::callMe),
		mSlot2(*this, &Test::callMe),
		mSlot3(*this, &Test::callMe),
		mSlot4(*this, &Test::callMe),
		mSlot5(*this, &Test::callMe)
	{
		mSlot0.connect(signal0);
		mSlot1.connect(signal1);
		mSlot2.connect(signal2);
		mSlot3.connect(signal3);
		mSlot4.connect(signal4);
		mSlot5.connect(signal5);
	}

	void connect(Signal0& signal0,
						 Signal1<int>& signal1,
						 Signal2<int, int>& signal2,
						 Signal3<int, int, int>& signal3,
						 Signal4<int, int, int, int>& signal4,
						 Signal5<int, int, int, int, int>& signal5)
	{
		mSlot0.connect(signal0);
		mSlot1.connect(signal1);
		mSlot2.connect(signal2);
		mSlot3.connect(signal3);
		mSlot4.connect(signal4);
		mSlot5.connect(signal5);
	}

	void callMe()
	{
		std::cout << "Test::callMe0() no param\n";
	}

	void callMe(int param1)
	{
		std::cout << "Test::callMe1(" << param1 << ")\n";
	}

	void callMe(int param1, int param2)
	{
		std::cout << "Test::callMe2(" << param1 << ", " << param2 << ")\n";
	}

	void callMe(int param1, int param2, int param3)
	{
		std::cout << "Test::callMe3(" << param1 << ", " << param2 << ", " << param3 << ")\n";
	}

	void callMe(int param1, int param2, int param3, int param4)
	{
		std::cout << "Test::callMe4(" << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ")\n";
	}

	void callMe(int param1, int param2, int param3, int param4, int param5)
	{
		std::cout << "Test::callMe5(" << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ")\n";
	}

private:
	MethodSlot0<Test> mSlot0;
	MethodSlot1<Test, int> mSlot1;
	MethodSlot2<Test, int, int> mSlot2;
	MethodSlot3<Test, int, int, int> mSlot3;
	MethodSlot4<Test, int, int, int, int> mSlot4;
	MethodSlot5<Test, int, int, int, int, int> mSlot5;
};

void freeFunction()
{
	std::cout << "freeFunction() no param\n";
}

void freeFunction(int param1)
{
	std::cout << "freeFunction(" << param1 << ")\n";
}

void freeFunction(int param1, int param2)
{
	std::cout << "freeFunction(" << param1 << ", " << param2 << ")\n";
}

void freeFunction(int param1, int param2, int param3)
{
	std::cout << "freeFunction(" << param1 << ", " << param2 << ", " << param3 << ")\n";
}

void freeFunction(int param1, int param2, int param3, int param4)
{
	std::cout << "freeFunction(" << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ")\n";
}

void freeFunction(int param1, int param2, int param3, int param4, int param5)
{
	std::cout << "freeFunction(" << param1 << ", " << param2 << ", " << param3 << ", " << param4 << ", " << param5 << ")\n";
}


int main()
{
	std::cout << "Slot destroyed before signal\n";
	{
		Signal0 signal0;
		Signal1<int> signal1;
		Signal2<int, int> signal2;
		Signal3<int, int, int> signal3;
		Signal4<int, int, int, int> signal4;
		Signal5<int, int, int, int, int> signal5;

		signal0.fire();
		signal1.fire(10);
		signal2.fire(10, 20);
		signal3.fire(10, 20, 30);
		signal4.fire(10, 20, 30, 40);
		signal5.fire(10, 20, 30, 40, 50);

		{
			FunctionSlot0 slot0(freeFunction, signal0);
			FunctionSlot1<int> slot1(freeFunction, signal1);
			FunctionSlot2<int, int> slot2(freeFunction, signal2);
			FunctionSlot3<int, int, int> slot3(freeFunction, signal3);
			FunctionSlot4<int, int, int, int> slot4(freeFunction, signal4);
			FunctionSlot5<int, int, int, int, int> slot5(freeFunction, signal5);

			Test test(signal0, signal1, signal2, signal3, signal4, signal5);

			FunctorSlot0<Functor>functor0(Functor(), signal0);
			FunctorSlot1<Functor, int>functor1(Functor(), signal1);
			FunctorSlot2<Functor, int, int>functor2(Functor(), signal2);
			FunctorSlot3<Functor, int, int, int>functor3(Functor(), signal3);
			FunctorSlot4<Functor, int, int, int, int>functor4(Functor(), signal4);
			FunctorSlot5<Functor, int, int, int, int, int>functor5(Functor(), signal5);


			signal0.fire();
			signal1.fire(10);
			signal2.fire(10, 20);
			signal3.fire(10, 20, 30);
			signal4.fire(10, 20, 30, 40);
			signal5.fire(10, 20, 30, 40, 50);
		}

		signal0.fire();
		signal1.fire(10);
		signal2.fire(10, 20);
		signal3.fire(10, 20, 30);
		signal4.fire(10, 20, 30, 40);
		signal5.fire(10, 20, 30, 40, 50);
	}

	std::cout << "\n\nSignal destroyed before slot\n";
	{
		FunctionSlot0 slot0(freeFunction);
		FunctionSlot1<int> slot1(freeFunction);
		FunctionSlot2<int, int> slot2(freeFunction);
		FunctionSlot3<int, int, int> slot3(freeFunction);
		FunctionSlot4<int, int, int, int> slot4(freeFunction);
		FunctionSlot5<int, int, int, int, int> slot5(freeFunction);

		Functor functor;
		FunctorSlot0<Functor> functor0(functor);
		FunctorSlot1<Functor, int> functor1(functor);
		FunctorSlot2<Functor, int, int> functor2(functor);
		FunctorSlot3<Functor, int, int, int> functor3(functor);
		FunctorSlot4<Functor, int, int, int, int> functor4(functor);
		FunctorSlot5<Functor, int, int, int, int, int> functor5(functor);

		Test test;

		{
			Signal0 signal0;
			Signal1<int> signal1;
			Signal2<int, int> signal2;
			Signal3<int, int, int> signal3;
			Signal4<int, int, int, int> signal4;
			Signal5<int, int, int, int, int> signal5;

			slot0.connect(signal0);
			slot1.connect(signal1);
			slot2.connect(signal2);
			slot3.connect(signal3);
			slot4.connect(signal4);
			slot5.connect(signal5);

			test.connect(signal0, signal1, signal2, signal3, signal4, signal5);

			functor0.connect(signal0);
			functor1.connect(signal1);
			functor2.connect(signal2);
			functor3.connect(signal3);
			functor4.connect(signal4);
			functor5.connect(signal5);

			signal0.fire();
			signal1.fire(10);
			signal2.fire(10, 20);
			signal3.fire(10, 20, 30);
			signal4.fire(10, 20, 30, 40);
			signal5.fire(10, 20, 30, 40, 50);
		}
	}

	return 0;
}

