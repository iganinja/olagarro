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

#ifndef SIGNAL_SLOT4_H
#define SIGNAL_SLOT4_H


#include <vector>
#include <algorithm>
#include <cassert>

namespace Olagarro
{

namespace SignalSlot
{

template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
class Slot4;

template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
class Signal4
{
public:
	~Signal4()
	{
		disconnectAll();
	}

	void connect(Slot4<ParamType1, ParamType2, ParamType3, ParamType4>* slot)
	{
		if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
		{
			mSlots.push_back(slot);
		}
	}

	void disconnect(Slot4<ParamType1, ParamType2, ParamType3, ParamType4>* slot)
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

	void fire(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
	{
		for(std::size_t i = 0; i < mSlots.size(); ++ i)
		{
			mSlots[i]->call(param1, param2, param3, param4);
		}
	}

private:
	typedef std::vector<Slot4<ParamType1, ParamType2, ParamType3, ParamType4>*> SlotVector;
	SlotVector mSlots;
};




template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
class Slot4
{
public:
	Slot4()
	{
	}

	Slot4(Signal4<ParamType1, ParamType2, ParamType3, ParamType4>& signal)
	{
		connect(signal);
	}

	virtual ~Slot4()
	{
		disconnectAll();
	}

	virtual void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4) = 0;

	virtual void connect(Signal4<ParamType1, ParamType2, ParamType3, ParamType4>& signal)
	{
		if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
		{
			mSignals.push_back(&signal);

			signal.connect(this);
		}
	}

	virtual void disconnect(Signal4<ParamType1, ParamType2, ParamType3, ParamType4>& signal)
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
	Slot4(const Slot4<ParamType1, ParamType2, ParamType3, ParamType4>& other);

	std::vector<Signal4<ParamType1, ParamType2, ParamType3, ParamType4>*> mSignals;
};

template<typename InstanceType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
class MethodSlot4 : public Slot4<ParamType1, ParamType2, ParamType3, ParamType4>
{
public:
	typedef void(InstanceType::*MethodType)(ParamType1, ParamType2, ParamType3, ParamType4);

	MethodSlot4(InstanceType& instance, MethodType method) :
		mInstance(instance),
		mMethod(method)
	{
	}

	MethodSlot4(InstanceType& instance, MethodType method, Signal4<ParamType1, ParamType2, ParamType3, ParamType4>& signal) :
		Slot4<ParamType1, ParamType2, ParamType3, ParamType4>(signal),
		mInstance(instance),
		mMethod(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
	{
		(mInstance.*mMethod)(param1, param2, param3, param4);
	}

private:
	MethodSlot4(const MethodSlot4<InstanceType, ParamType1, ParamType2, ParamType3, ParamType4>& other);
	MethodSlot4<InstanceType, ParamType1, ParamType2, ParamType3, ParamType4> operator = (const MethodSlot4<InstanceType, ParamType1, ParamType2, ParamType3, ParamType4>& other);

	InstanceType& mInstance;
	MethodType mMethod;
};

template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
class FunctionSlot4 : public Slot4<ParamType1, ParamType2, ParamType3, ParamType4>
{
public:
	typedef void(*FunctionType)(ParamType1, ParamType2, ParamType3, ParamType4);

	FunctionSlot4(FunctionType method) :
		mFunction(method)
	{
	}

	FunctionSlot4(FunctionType method, Signal4<ParamType1, ParamType2, ParamType3, ParamType4>& signal) :
		Slot4<ParamType1, ParamType2, ParamType3, ParamType4>(signal),
		mFunction(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
	{
		(mFunction)(param1, param2, param3, param4);
	}

private:
	FunctionSlot4(const FunctionSlot4<ParamType1, ParamType2, ParamType3, ParamType4>& other);
	FunctionSlot4<ParamType1, ParamType2, ParamType3, ParamType4> operator = (const FunctionSlot4<ParamType1, ParamType2, ParamType3, ParamType4>& other);

	FunctionType mFunction;
};


template<typename CallableType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
class FunctorSlot4 : public Slot4<ParamType1, ParamType2, ParamType3, ParamType4>
{
public:
	FunctorSlot4(CallableType callable) :
		mCallable(callable)
	{
	}

	FunctorSlot4(CallableType callable, Signal4<ParamType1, ParamType2, ParamType3, ParamType4>& signal) :
		Slot4<ParamType1, ParamType2, ParamType3, ParamType4>(signal),
		mCallable(callable)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
	{
		mCallable(param1, param2, param3, param4);
	}

private:
	  CallableType mCallable;
};

}

}

#endif // SIGNAL_SLOT4_H

