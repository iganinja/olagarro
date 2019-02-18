/*
Copyright (c) 2019 Inaki Griego
This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
		1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgement in the product documentation would be
		appreciated but is not required.
		2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.
		3. This notice may not be removed or altered from any source
		distribution.
*/

#ifndef SIGNALSLOTVT_H
#define SIGNALSLOTVT_H

#include <vector>
#include <algorithm>
#include <cassert>

namespace Olagarro
{

namespace SignalSlot
{

template<typename ... ParamTypes>
class Slot;

template<typename ... ParamTypes>
class Signal
{
public:
    ~Signal()
    {
        disconnectAll();
    }

    void connect(Slot<ParamTypes...>* slot)
    {
        if(std::find(mSlots.begin(), mSlots.end(), slot) == mSlots.end())
        {
            mSlots.push_back(slot);
        }
    }

    void disconnect(Slot<ParamTypes...>* slot)
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

    void fire(ParamTypes ... params)
    {
        for(std::size_t i = 0; i < mSlots.size(); ++ i)
        {
            mSlots[i]->call(params...);
        }
    }

private:
    using SlotVector = std::vector<Slot<ParamTypes...>*>;
    SlotVector mSlots;
};




template<typename ... ParamTypes>
class Slot
{
public:
    Slot()
    {
    }

    Slot(Signal<ParamTypes...>& signal)
    {
        connect(signal);
    }

    virtual ~Slot()
    {
        disconnectAll();
    }

    virtual void call(ParamTypes ... params) = 0;

    virtual void connect(Signal<ParamTypes...>& signal)
    {
        if(std::find(mSignals.begin(), mSignals.end(), &signal) == mSignals.end())
        {
            mSignals.push_back(&signal);

            signal.connect(this);
        }
    }

    virtual void disconnect(Signal<ParamTypes...>& signal)
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
    Slot(const Slot<ParamTypes...>& other);

    std::vector<Signal<ParamTypes...>*> mSignals;
};

template<typename InstanceType, typename ... ParamTypes>
class MethodSlot : public Slot<ParamTypes...>
{
public:
    typedef void(InstanceType::*MethodType)(ParamTypes...);

    MethodSlot(InstanceType& instance, MethodType method) :
        mInstance(instance),
        mMethod(method)
    {
    }

    MethodSlot(InstanceType& instance, MethodType method, Signal<ParamTypes...>& signal) :
        Slot<ParamTypes...>(signal),
        mInstance(instance),
        mMethod(method)
    {
    }

    void call(ParamTypes ... params)
    {
        (mInstance.*mMethod)(params...);
    }

private:
    MethodSlot(const MethodSlot<InstanceType, ParamTypes...>& other);
    MethodSlot<InstanceType, ParamTypes...> operator = (const MethodSlot<InstanceType, ParamTypes...>& other);

    InstanceType& mInstance;
    MethodType mMethod;
};

template<typename ... ParamTypes>
class FunctionSlot : public Slot<ParamTypes...>
{
public:
    typedef void(*FunctionType)(ParamTypes...);

    FunctionSlot(FunctionType method) :
        mFunction(method)
    {
    }

    FunctionSlot(FunctionType method, Signal<ParamTypes...>& signal) :
        Slot<ParamTypes...>(signal),
        mFunction(method)
    {
    }

    void call(ParamTypes ... params)
    {
        (mFunction)(params...);
    }

private:
    FunctionSlot(const FunctionSlot<ParamTypes...>& other);
    FunctionSlot<ParamTypes...> operator = (const FunctionSlot<ParamTypes...>& other);

    FunctionType mFunction;
};

template<typename CallableType, typename ... ParamTypes>
class FunctorSlot : public Slot<ParamTypes...>
{
public:
    FunctorSlot(CallableType callable) :
        mCallable(callable)
    {
    }

    FunctorSlot(CallableType callable, Signal<ParamTypes...>& signal) :
        Slot<ParamTypes...>(signal),
        mCallable(callable)
    {
    }

    void call(ParamTypes ... params)
    {
        mCallable(params...);
    }

private:
      CallableType mCallable;
};

}

}

#endif // SIGNALSLOTVT_H

