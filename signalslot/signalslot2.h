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

#ifndef SIGNAL_SLOT2_H
#define SIGNAL_SLOT2_H

#include <vector>
#include <algorithm>
#include <cassert>

namespace Olagarro
{

namespace SignalSlot
{

template<typename ParamType1, typename ParamType2>
class Slot2;

template<typename ParamType1, typename ParamType2>
class Signal2
{
public:
	~Signal2()
	{
		disconnectAll();
	}

	void connect(Slot2<ParamType1, ParamType2>* slot)
	{
		if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
		{
			mSlots.push_back(slot);
		}
	}

	void disconnect(Slot2<ParamType1, ParamType2>* slot)
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

	void fire(ParamType1 param1, ParamType2 param2)
	{
		for(std::size_t i = 0; i < mSlots.size(); ++ i)
		{
			mSlots[i]->call(param1, param2);
		}
	}

private:
	typedef std::vector<Slot2<ParamType1, ParamType2>*> SlotVector;
	SlotVector mSlots;
};




template<typename ParamType1, typename ParamType2>
class Slot2
{
public:
	Slot2()
	{
	}

	Slot2(Signal2<ParamType1, ParamType2>& signal)
	{
		connect(signal);
	}

	virtual ~Slot2()
	{
		disconnectAll();
	}

	virtual void call(ParamType1 param1, ParamType2 param2) = 0;

	virtual void connect(Signal2<ParamType1, ParamType2>& signal)
	{
		if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
		{
			mSignals.push_back(&signal);

			signal.connect(this);
		}
	}

	virtual void disconnect(Signal2<ParamType1, ParamType2>& signal)
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
	Slot2(const Slot2<ParamType1, ParamType2>& other);

	std::vector<Signal2<ParamType1, ParamType2>*> mSignals;
};

template<typename InstanceType, typename ParamType1, typename ParamType2>
class MethodSlot2 : public Slot2<ParamType1, ParamType2>
{
public:
	typedef void(InstanceType::*MethodType)(ParamType1, ParamType2);

	MethodSlot2(InstanceType& instance, MethodType method) :
		mInstance(instance),
		mMethod(method)
	{
	}

	MethodSlot2(InstanceType& instance, MethodType method, Signal2<ParamType1, ParamType2>& signal) :
		Slot2<ParamType1, ParamType2>(signal),
		mInstance(instance),
		mMethod(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2)
	{
		(mInstance.*mMethod)(param1, param2);
	}

private:
	MethodSlot2(const MethodSlot2<InstanceType, ParamType1, ParamType2>& other);
	MethodSlot2<InstanceType, ParamType1, ParamType2> operator = (const MethodSlot2<InstanceType, ParamType1, ParamType2>& other);

	InstanceType& mInstance;
	MethodType mMethod;
};

template<typename ParamType1, typename ParamType2>
class FunctionSlot2 : public Slot2<ParamType1, ParamType2>
{
public:
	typedef void(*FunctionType)(ParamType1, ParamType2);

	FunctionSlot2(FunctionType method) :
		mFunction(method)
	{
	}

	FunctionSlot2(FunctionType method, Signal2<ParamType1, ParamType2>& signal) :
		Slot2<ParamType1, ParamType2>(signal),
		mFunction(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2)
	{
		(mFunction)(param1, param2);
	}

private:
	FunctionSlot2(const FunctionSlot2<ParamType1, ParamType2>& other);
	FunctionSlot2<ParamType1, ParamType2> operator = (const FunctionSlot2<ParamType1, ParamType2>& other);

	FunctionType mFunction;
};

template<typename CallableType, typename ParamType1, typename ParamType2>
class FunctorSlot2 : public Slot2<ParamType1, ParamType2>
{
public:
	FunctorSlot2(CallableType callable) :
		mCallable(callable)
	{
	}

	FunctorSlot2(CallableType callable, Signal2<ParamType1, ParamType2>& signal) :
		Slot2<ParamType1, ParamType2>(signal),
		mCallable(callable)
	{
	}

	void call(ParamType1 param1, ParamType2 param2)
	{
		mCallable(param1, param2);
	}

private:
	  CallableType mCallable;
};

}

}

#endif // SIGNAL_SLOT2_H

