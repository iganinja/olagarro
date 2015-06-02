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

#ifndef SIGNAL_SLOT_H
#define SIGNAL_SLOT_H

#include <vector>
#include <algorithm>
#include <cassert>
#include "signalslot1.h"
#include "signalslot2.h"
#include "signalslot3.h"
#include "signalslot4.h"
#include "signalslot5.h"

namespace Olagarro
{

namespace SignalSlot
{

class Slot0;

class Signal0
{
public:
	~Signal0()
	{
		disconnectAll();
	}

	void connect(Slot0* slot)
	{
		if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
		{
			mSlots.push_back(slot);
		}
	}

	void disconnectAll();

	void fire();

	void disconnect(Slot0* slot)
	{
		mSlots.erase(std::remove(mSlots.begin(), mSlots.end(), slot));
	}

private:
	typedef std::vector<Slot0*> SlotVector;
	SlotVector mSlots;
};




class Slot0
{
public:
	Slot0()
	{
	}

	Slot0(Signal0& signal)
	{
		connect(signal);
	}

	virtual ~Slot0()
	{
		disconnectAll();
	}

	virtual void call() = 0;

	virtual void connect(Signal0& signal)
	{
		if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
		{
			mSignals.push_back(&signal);

			signal.connect(this);
		}
	}

	virtual void disconnect(Signal0& signal)
	{
		assert(std::find(mSignals.begin(), mSignals.end(), &signal) != mSignals.end());

		mSignals.erase(std::remove(mSignals.begin(), mSignals.end(), &signal));

		signal.disconnect(this);
	}

	void disconnectAll()
	{
		for(std::size_t i = 0; i < mSignals.size(); ++ i)
		{
			mSignals[i]->disconnect(this);
		}
	}

private:
	Slot0(const Slot0& other);

	std::vector<Signal0*> mSignals;
};


void Signal0::disconnectAll()
{
	SlotVector v = mSlots;
	for(std::size_t i = 0; i < v.size(); ++ i)
	{
		v[i]->disconnect(*this);
	}
}

void Signal0::fire()
{
	for(std::size_t i = 0; i < mSlots.size(); ++ i)
	{
		mSlots[i]->call();
	}
}


template<typename InstanceType>
class MethodSlot0 : public Slot0
{
public:
	typedef void(InstanceType::*MethodType)();

	MethodSlot0(InstanceType& instance, MethodType method) :
		mInstance(instance),
		mMethod(method)
	{
	}

	MethodSlot0(InstanceType& instance, MethodType method, Signal0& signal) :
		Slot0(signal),
		mInstance(instance),
		mMethod(method)
	{
	}

	void call()
	{
		(mInstance.*mMethod)();
	}

private:
	MethodSlot0(const MethodSlot0<InstanceType>& other);
	MethodSlot0<InstanceType> operator = (const MethodSlot0<InstanceType>& other);

	InstanceType& mInstance;
	MethodType mMethod;
};

class FunctionSlot0 : public Slot0
{
public:
	typedef void(*FunctionType)();

	FunctionSlot0(FunctionType method) :
		mFunction(method)
	{
	}

	FunctionSlot0(FunctionType method, Signal0& signal) :
		Slot0(signal),
		mFunction(method)
	{
	}

	void call()
	{
		(mFunction)();
	}

private:
	FunctionSlot0(const FunctionSlot0& other);
	FunctionSlot0 operator = (const FunctionSlot0& other);

	FunctionType mFunction;
};

template<typename CallableType>
class FunctorSlot0 : public Slot0
{
public:
	FunctorSlot0(CallableType callable) :
		mCallable(callable)
	{
	}

	FunctorSlot0(CallableType callable, Signal0& signal) :
		Slot0(signal),
		mCallable(callable)
	{
	}

	void call()
	{
		mCallable();
	}

private:
	  CallableType mCallable;
};

}

}

#endif // SIGNAL_SLOT_H

