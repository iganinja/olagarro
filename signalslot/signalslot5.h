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

#ifndef SIGNAL_SLOT5_H
#define SIGNAL_SLOT5_H

namespace Olagarro
{

namespace SignalSlot
{

template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
class Slot5;

template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
class Signal5
{
public:
	~Signal5()
	{
		disconnectAll();
	}

	void connect(Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>* slot)
	{
		if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
		{
			mSlots.push_back(slot);
		}
	}

	void disconnect(Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>* slot)
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

	void fire(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5)
	{
		for(std::size_t i = 0; i < mSlots.size(); ++ i)
		{
			mSlots[i]->call(param1, param2, param3, param4, param5);
		}
	}

private:
	typedef std::vector<Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>*> SlotVector;
	SlotVector mSlots;
};




template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
class Slot5
{
public:
	Slot5()
	{
	}

	Slot5(Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& signal)
	{
		connect(signal);
	}

	virtual ~Slot5()
	{
		disconnectAll();
	}

	virtual void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5) = 0;

	virtual void connect(Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& signal)
	{
		if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
		{
			mSignals.push_back(&signal);

			signal.connect(this);
		}
	}

	virtual void disconnect(Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& signal)
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
	Slot5(const Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& other);

	std::vector<Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>*> mSignals;
};

template<typename InstanceType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
class MethodSlot5 : public Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>
{
public:
	typedef void(InstanceType::*MethodType)(ParamType1, ParamType2, ParamType3, ParamType4, ParamType5);

	MethodSlot5(InstanceType& instance, MethodType method) :
		mInstance(instance),
		mMethod(method)
	{
	}

	MethodSlot5(InstanceType& instance, MethodType method, Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& signal) :
		Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>(signal),
		mInstance(instance),
		mMethod(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5)
	{
		(mInstance.*mMethod)(param1, param2, param3, param4, param5);
	}

private:
	MethodSlot5(const MethodSlot5<InstanceType, ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& other);
	MethodSlot5<InstanceType, ParamType1, ParamType2, ParamType3, ParamType4, ParamType5> operator = (const MethodSlot5<InstanceType, ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& other);

	InstanceType& mInstance;
	MethodType mMethod;
};

template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
class FunctionSlot5 : public Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>
{
public:
	typedef void(*FunctionType)(ParamType1, ParamType2, ParamType3, ParamType4, ParamType5);

	FunctionSlot5(FunctionType method) :
		mFunction(method)
	{
	}

	FunctionSlot5(FunctionType method, Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& signal) :
		Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>(signal),
		mFunction(method)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5)
	{
		(mFunction)(param1, param2, param3, param4, param5);
	}

private:
	FunctionSlot5(const FunctionSlot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& other);
	FunctionSlot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5> operator = (const FunctionSlot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& other);

	FunctionType mFunction;
};

template<typename CallableType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
class FunctorSlot5 : public Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>
{
public:
	FunctorSlot5(CallableType callable) :
		mCallable(callable)
	{
	}

	FunctorSlot5(CallableType callable, Signal5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>& signal) :
		Slot5<ParamType1, ParamType2, ParamType3, ParamType4, ParamType5>(signal),
		mCallable(callable)
	{
	}

	void call(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4, ParamType5 param5)
	{
		mCallable(param1, param2, param3, param4, param5);
	}

private:
	  CallableType mCallable;
};

}

}

#endif // SIGNAL_SLOT5_H

