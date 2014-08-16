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

#ifndef CALLER_H
#define CALLER_H

namespace Olagarro
{

namespace Concurrency
{

/**
 * @brief Base class to encapsulate a callable entity (free functions, functors or member functions)
 */
template<typename ReturnType>
class Caller
{
public:
	virtual ~Caller() {}
	virtual ReturnType performCall() = 0;
};

template<typename ReturnType>
class Function0ParamCaller : public Caller<ReturnType>
{
public:
	Function0ParamCaller(ReturnType (*function)()) : mFunction(function) {}

	ReturnType performCall()
	{
		return mFunction();
	}

private:
	ReturnType (*mFunction)();
};

template<typename ReturnType, typename Param1Type>
class CopyFunction1ParamCaller : public Caller<ReturnType>
{
public:
	CopyFunction1ParamCaller(ReturnType (*function)(Param1Type), Param1Type param1) : mFunction(function), mParam1(param1) {}

	ReturnType performCall()
	{
		return mFunction(mParam1);
	}

private:
	ReturnType (*mFunction)(Param1Type);
	Param1Type mParam1;
};


template<typename ReturnType, typename Param1Type>
class ConstFunction1ParamCaller : public Caller<ReturnType>
{
public:
	ConstFunction1ParamCaller(ReturnType (*function)(const Param1Type&), const Param1Type& param1) : mFunction(function), mParam1(param1) {}

	ReturnType performCall()
	{
		return mFunction(mParam1);
	}

private:
	ReturnType (*mFunction)(const Param1Type&);
	const Param1Type& mParam1;
};


template<typename ReturnType, typename Param1Type>
class Function1ParamCaller : public Caller<ReturnType>
{
public:
	Function1ParamCaller(ReturnType (*function)(Param1Type&), Param1Type& param1) : mFunction(function), mParam1(param1) {}

	ReturnType performCall()
	{
		return mFunction(mParam1);
	}

private:
	ReturnType (*mFunction)(Param1Type&);
	Param1Type& mParam1;
};


template<typename ClassType, typename ReturnType>
class Method0ParamCaller : public Caller<ReturnType>
{
public:
	Method0ParamCaller(ClassType& instance, ReturnType (ClassType::*method)()) :
		mInstance(instance),
		mMethod(method) {}

	ReturnType performCall()
	{
		return (mInstance.*mMethod)();
	}

private:
	ClassType& mInstance;
	ReturnType (ClassType::*mMethod)();
};

template<typename ClassType, typename ReturnType, typename Param1Type>
class CopyMethod1ParamCaller : public Caller<ReturnType>
{
public:
	CopyMethod1ParamCaller(ClassType& instance, ReturnType (ClassType::*method)(Param1Type), Param1Type param1) :
		mInstance(instance),
		mMethod(method),
		mParam1(param1) {}

	ReturnType performCall()
	{
		return (mInstance.*mMethod)(mParam1);
	}

private:
	ClassType& mInstance;
	ReturnType (ClassType::*mMethod)(Param1Type);
	Param1Type mParam1;
};

template<typename ClassType, typename ReturnType, typename Param1Type>
class ConstMethod1ParamCaller : public Caller<ReturnType>
{
public:
	ConstMethod1ParamCaller(ClassType& instance, ReturnType (ClassType::*method)(const Param1Type&), const Param1Type& param1) :
		mInstance(instance),
		mMethod(method),
		mParam1(param1) {}

	ReturnType performCall()
	{
		return (mInstance.*mMethod)(mParam1);
	}

private:
	ClassType& mInstance;
	ReturnType (ClassType::*mMethod)(const Param1Type&);
	const Param1Type& mParam1;
};

template<typename ClassType, typename ReturnType, typename Param1Type>
class Method1ParamCaller : public Caller<ReturnType>
{
public:
	Method1ParamCaller(ClassType& instance, ReturnType (ClassType::*method)(Param1Type&), Param1Type& param1) :
		mInstance(instance),
		mMethod(method),
		mParam1(param1) {}

	ReturnType performCall()
	{
		return (mInstance.*mMethod)(mParam1);
	}

private:
	ClassType& mInstance;
	ReturnType (ClassType::*mMethod)(Param1Type&);
	Param1Type& mParam1;
};


template<typename ReturnType, typename Functor>
class ConstFunctor0ParamCaller : public Caller<ReturnType>
{
public:
	ConstFunctor0ParamCaller(const Functor& functor) : mFunctor(functor) {}

	ReturnType performCall()
	{
		return mFunctor();
	}

private:
	const Functor& mFunctor;
};

template<typename ReturnType, typename Functor>
class Functor0ParamCaller : public Caller<ReturnType>
{
public:
	Functor0ParamCaller(Functor& functor) : mFunctor(functor) {}

	ReturnType performCall()
	{
		return mFunctor();
	}

private:
	Functor& mFunctor;
};

}

}

#endif // CALLER_H
