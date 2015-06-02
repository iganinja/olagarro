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

#ifndef SIGNAL_SLOT3_H
#define SIGNAL_SLOT3_H

#include <vector>
#include <algorithm>
#include <cassert>

namespace Olagarro
{

namespace SignalSlot
{

template<typename ParamType1, typename ParamType2, typename ParamType3>
class Slot3;

template<typename ParamType1, typename ParamType2, typename ParamType3>
class Signal3
{
public:
	~Signal3()
	{
		disconnectAll();
	}

	void connect(Slot3<ParamType1, ParamType2, ParamType3>* slot)
	{
		if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
		{
			mSlots.push_back(slot);
		}
	}

	void disconnect(Slot3<ParamType1, ParamType2, ParamType3>* slot)
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

	void fire(ParamType1 param1, ParamType2 param2, ParamType3 param3)
	{
		for(std::size_t i = 0; i < mSlots.size(); ++ i)
		{
			mSlots[i]->call(param1, param2, param3);
		}
	}

private:
	typedef std::vector<Slot3<ParamType1, ParamType2, ParamType3>*> SlotVector;
	SlotVector mSlots;
};




template<typename ParamType1, typename ParamType2, typename ParamType3>
class Slot3
{
public:
	Slot3()
	{
	}

	Slot3(Signal3<ParamType1, ParamType2, ParamType3>& signal)
	{
		connect(signal);
	}

	virtual ~Slot3()
	{
		disconnectAll();
	}

	virtual void call(ParamType1 param1, ParamType2 param2, ParamType3 param3) = 0;

	virtual void connect(Signal3<ParamType1, ParamType2, ParamType3>& signal)
	{
		if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
		{
			mSignals.push_back(&signal);

			signal.connect(this);
		}
	}

	virtual void disconnect(Signal3<ParamType1, ParamType2, ParamType3>& signal)
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
	Slot3(const Slot3<ParamType1, ParamType2, ParamType3>& other);

	std::vector<Signal3<ParamType1, ParamType2, ParamType3>*> mSignals;
};

template<typename InstanceType, typename ParamType1, typename ParamType2, typename ParamType3>
class MethodSlot3 : public Slot3<ParamType1, ParamType2, ParamType3>
{
public:
	typedef void(InstanceType::*MethodType)(ParamType1, ParamType2, ParamType3);

	MethodSlot3(InstanceType& instance, MethodType method) :
		mInstance(instance),
		mMethod(method)
	{
	}

	MethodSlot3(InstanceType& instance, MethodType method, Signal3<ParamType1, ParamType2, ParamType3>& signal) :
		Slot3<ParamType1, ParamType2, ParamType3>(signal),
		mInstance(instance),
		mMethod(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3)
	{
		(mInstance.*mMethod)(param1, param2, param3);
	}

private:
	MethodSlot3(const MethodSlot3<InstanceType, ParamType1, ParamType2, ParamType3>& other);
	MethodSlot3<InstanceType, ParamType1, ParamType2, ParamType3> operator = (const MethodSlot3<InstanceType, ParamType1, ParamType2, ParamType3>& other);

	InstanceType& mInstance;
	MethodType mMethod;
};

template<typename ParamType1, typename ParamType2, typename ParamType3>
class FunctionSlot3 : public Slot3<ParamType1, ParamType2, ParamType3>
{
public:
	typedef void(*FunctionType)(ParamType1, ParamType2, ParamType3);

	FunctionSlot3(FunctionType method) :
		mFunction(method)
	{
	}

	FunctionSlot3(FunctionType method, Signal3<ParamType1, ParamType2, ParamType3>& signal) :
		Slot3<ParamType1, ParamType2, ParamType3>(signal),
		mFunction(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3)
	{
		(mFunction)(param1, param2, param3);
	}

private:
	FunctionSlot3(const FunctionSlot3<ParamType1, ParamType2, ParamType3>& other);
	FunctionSlot3<ParamType1, ParamType2, ParamType3> operator = (const FunctionSlot3<ParamType1, ParamType2, ParamType3>& other);

	FunctionType mFunction;
};


template<typename CallableType, typename ParamType1, typename ParamType2, typename ParamType3>
class FunctorSlot3 : public Slot3<ParamType1, ParamType2, ParamType3>
{
public:
	FunctorSlot3(CallableType callable) :
		mCallable(callable)
	{
	}

	FunctorSlot3(CallableType callable, Signal3<ParamType1, ParamType2, ParamType3>& signal) :
		Slot3<ParamType1, ParamType2, ParamType3>(signal),
		mCallable(callable)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3)
	{
		mCallable(param1, param2, param3);
	}

private:
	  CallableType mCallable;
};

}

}

#endif // SIGNAL_SLOT3_H

