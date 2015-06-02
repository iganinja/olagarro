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

#ifndef SIGNAL_SLOT1_H
#define SIGNAL_SLOT1_H

#include <vector>
#include <algorithm>
#include <cassert>

namespace Olagarro
{

namespace SignalSlot
{

template<typename ParamType1>
class Slot1;

template<typename ParamType1>
class Signal1
{
public:
	~Signal1()
	{
		disconnectAll();
	}

	void connect(Slot1<ParamType1>* slot)
	{
		if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
		{
			mSlots.push_back(slot);
		}
	}

	void disconnect(Slot1<ParamType1>* slot)
	{
		mSlots.erase(std::remove(mSlots.begin(), mSlots.end(), slot));
	}

	void disconnectAll()
	{
		SlotVector v = mSlots;
		for(std::size_t i = 0; i < v.size(); ++ i)
		{
			v[i]->disconnect(*this);
		}
	}

	void fire(ParamType1 param1)
	{
		for(std::size_t i = 0; i < mSlots.size(); ++ i)
		{
			mSlots[i]->call(param1);
		}
	}

private:
	typedef std::vector<Slot1<ParamType1>*> SlotVector;
	SlotVector mSlots;
};




template<typename ParamType1>
class Slot1
{
public:
	Slot1()
	{
	}

	Slot1(Signal1<ParamType1>& signal)
	{
		connect(signal);
	}

	virtual ~Slot1()
	{
		disconnectAll();
	}

	virtual void call(ParamType1 param1) = 0;

	virtual void connect(Signal1<ParamType1>& signal)
	{
		if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
		{
			mSignals.push_back(&signal);

			signal.connect(this);
		}
	}

	virtual void disconnect(Signal1<ParamType1>& signal)
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
	Slot1(const Slot1<ParamType1>& other);

	std::vector<Signal1<ParamType1>*> mSignals;
};

template<typename InstanceType, typename ParamType1>
class MethodSlot1 : public Slot1<ParamType1>
{
public:
	typedef void(InstanceType::*MethodType)(ParamType1);

	MethodSlot1(InstanceType& instance, MethodType method) :
		mInstance(instance),
		mMethod(method)
	{
	}

	MethodSlot1(InstanceType& instance, MethodType method, Signal1<ParamType1>& signal) :
		Slot1<ParamType1>(signal),
		mInstance(instance),
		mMethod(method)
	{
	}

	void call(ParamType1 param1)
	{
		(mInstance.*mMethod)(param1);
	}

private:
	MethodSlot1(const MethodSlot1<InstanceType, ParamType1>& other);
	MethodSlot1<InstanceType, ParamType1> operator = (const MethodSlot1<InstanceType, ParamType1>& other);

	InstanceType& mInstance;
	MethodType mMethod;
};

template<typename ParamType1>
class FunctionSlot1 : public Slot1<ParamType1>
{
public:
	typedef void(*FunctionType)(ParamType1);

	FunctionSlot1(FunctionType method) :
		mFunction(method)
	{
	}

	FunctionSlot1(FunctionType method, Signal1<ParamType1>& signal) :
		Slot1<ParamType1>(signal),
		mFunction(method)
	{
	}

	void call(ParamType1 param1)
	{
		(mFunction)(param1);
	}

private:
	FunctionSlot1(const FunctionSlot1<ParamType1>& other);
	FunctionSlot1<ParamType1> operator = (const FunctionSlot1<ParamType1>& other);

	FunctionType mFunction;
};

template<typename CallableType, typename ParamType1>
class FunctorSlot1 : public Slot1<ParamType1>
{
public:
	FunctorSlot1(CallableType callable) :
		mCallable(callable)
	{
	}

	FunctorSlot1(CallableType callable, Signal1<ParamType1>& signal) :
		Slot1<ParamType1>(signal),
		mCallable(callable)
	{
	}

	void call(ParamType1 param1)
	{
		mCallable(param1);
	}

private:
	  CallableType mCallable;
};

}

}

#endif // SIGNAL_SLOT1_H

