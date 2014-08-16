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


#ifndef OLAGARRO_H
#define OLAGARRO_H

#include "job.h"
#include "concurrentfor.h"
#include "threadpool.h"
#include "future.h"

//! Olagarro namespace: It contains Olagarro's all classes, functions, etc.
namespace Olagarro
{

/** @brief Concurrency module's namespace: Not all the classes that appear in this documentation are meant to be used by client code. In fact, only Future, launchJob and the two versions of concurrentFor are meant to be used
 * by library's client code, all other classes are for internal use.
 */
namespace Concurrency
{

//! Launches a function asynchronously
/*!
	\param function A free function which takes no arguments and returns a value (void return type is also valid)
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType>
Future<ReturnType> launchJob(ReturnType (*function)(void))
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new Function0ParamCaller<ReturnType>(function)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a function asynchronously accepting a parameter
/*!
	\param function A free function which takes an argument and returns a value (void return type is also valid)
	\param param1 function's parameter
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename Param1Type>
Future<ReturnType> launchJob(ReturnType (*function)(Param1Type), Param1Type param1)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new CopyFunction1ParamCaller<ReturnType, Param1Type>(function, param1)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a function asynchronously accepting a parameter
/*!
	\param function A free function which takes an argument and returns a value (void return type is also valid)
	\param param1 function's parameter
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename Param1Type>
Future<ReturnType> launchJob(ReturnType (*function)(const Param1Type&), const Param1Type& param1)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new ConstFunction1ParamCaller<ReturnType, Param1Type>(function, param1)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a function asynchronously accepting a parameter
/*!
	\param function A free function which takes an argument and returns a value (void return type is also valid).
	This version of launchJob accepts a reference of an object so function can modify that value but launchJob does not deal with synchronization issues, it's something user must provide (if needed)
	\param param1 function's parameter
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename Param1Type>
Future<ReturnType> launchJob(ReturnType (*function)(Param1Type&), Param1Type& param1)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new Function1ParamCaller<ReturnType, Param1Type>(function, param1)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}


//! Launches a method asynchronously
/*!
	\param instance An instance of a class which method belongs to
	\param method A method which takes no arguments and returns a value (void return type is also valid)
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename ClassType>
Future<ReturnType> launchJob(ClassType& instance, ReturnType (ClassType::*method)())
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new Method0ParamCaller<ClassType, ReturnType>(instance, method)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a method asynchronously accepting a parameter
/*!
	\param instance An instance of a class which method belongs to
	\param method A method which takes an argument and returns a value (void return type is also valid)
	\param param1 methods's parameter
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename ClassType, typename Param1Type>
Future<ReturnType> launchJob(ClassType& instance, ReturnType (ClassType::*method)(Param1Type), Param1Type param1)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new CopyMethod1ParamCaller<ClassType, ReturnType, Param1Type>(instance, method, param1)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a method asynchronously accepting a parameter
/*!
	\param instance An instance of a class which method belongs to
	\param method A method which takes an argument and returns a value (void return type is also valid)
	\param param1 methods's parameter
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename ClassType, typename Param1Type>
Future<ReturnType> launchJob(ClassType& instance, ReturnType (ClassType::*method)(const Param1Type&), const Param1Type& param1)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new ConstMethod1ParamCaller<ClassType, ReturnType, Param1Type>(instance, method, param1)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a method asynchronously accepting a parameter
/*!
	\param instance An instance of a class which method belongs to
	\param method A method which takes an argument and returns a value (void return type is also valid). Like in function version, method can modify parameter's object and its up to method's implementator the synchronization issues
	\param param1 methods's parameter
	\return A Future which will contain function's result once it finishes its job
*/
template<typename ReturnType, typename ClassType, typename Param1Type>
Future<ReturnType> launchJob(ClassType& instance, ReturnType (ClassType::*method)(Param1Type&), Param1Type& param1)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new Method1ParamCaller<ClassType, ReturnType, Param1Type>(instance, method, param1)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}


//! Launches a functor's operator () asynchronously
/*!
	As concurrency module only offers 0 or 1 parameter function/method versions, functors versions allow to encapsulate more elaborated calls.
	For example if we have this function:

	\code
	float getSomeValue(const std::vector<float>& data, const OtherData& otherData, float thirdValue, std::string& lastData);
	\endcode

	We can wrap it in a functor and launch it concurrently:

	\code
	struct CallWrapper
	{
		CallWrapper(const std::vector<float>& data, const OtherData& otherData, float thirdValue, std::string& lastData) :
			mData(data),
			mOtherData(otherData),
			mThirdValue(thirdValue),
			mLastData(lastData)
			{}

		float operator () () const
		{
			return getSomeValue(mData, mOtherData, mThirdValue, mLastData);
		}

		const std::vector<float>& mData;
		const OtherData& mOtherData;
		float mThirdValue;
		std::string& mLastData;
	};

	Future<float> myValue = Olagarro::launchJob(CallWrapper(...The 4 parameters...));

	\endcode

	\param functor A functor with defined const operator () taking 0 parameters. This operator must return same type of value as Future is expecting (void also accepted)
	\return A Future which will contain functor's operator () result once it finishes its job
*/
template<typename ReturnType, typename Functor>
Future<ReturnType> launchJob(const Functor& functor)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new ConstFunctor0ParamCaller<ReturnType, Functor>(functor)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}

//! Launches a functor's operator () asynchronously
/*! Same as the other functor version but accepting a non-const functor
*/
template<typename ReturnType, typename Functor>
Future<ReturnType> launchJob(Functor& functor)
{
	Shared<Job, MutexMTPolicy> job(new CallerJob<ReturnType>(new Functor0ParamCaller<ReturnType, Functor>(functor)));

	ThreadPool::instance().enqueueJob(job);

	return Future<ReturnType>(job);
}


template<typename ParamType>
class Param1WrapperFunctor
{
public:
	Param1WrapperFunctor(void (*function)(ParamType&)) :
		mFunction(function)
	{
	}
	void operator () (int, ParamType& value)
	{
		(*mFunction)(value);
	}

private:
	void (*mFunction)(ParamType&);
};

//! Launches a function concurrently taking into account passed element number and host's CPUs
/*! Launches function function in as much batches as CPUs has host machine. For example, if host has 4 CPUs it will divide [begin, end) range in 4 parts and
	launch a concurrent task for each part. As other concurrency module facilities user is responsible to avoiding race conditions and such issues.
	\param begin Range's start position iterator
	\param end Range's end position iterator
	\param function A function which takes range's type's reference as parameter. This reference will be used to change range's current element
	\return A Future<void>. This object is used to know when concurrentFor has finished its job
*/
template<typename InputIterator, typename ParamType>
Future<void> concurrentFor(InputIterator begin, InputIterator end, void (*function)(ParamType&))
{
	return concurrentFor(begin, end, Param1WrapperFunctor<ParamType>(function));
}

template<typename ParamType>
class Param2WrapperFunctor
{
public:
	Param2WrapperFunctor(void (*function)(int index, ParamType&)) :
		mFunction(function)
	{
	}
	void operator () (int index, ParamType& value)
	{
		(*mFunction)(index, value);
	}

private:
	void (*mFunction)(int index, ParamType&);
};

//! Same as previous concurrentFor but expecting a function which takes 2 parameters: first one a current's element index and the second one the element's reference
/*!
	\param begin Range's start position iterator
	\param end Range's end position iterator
	\param function A function which takes element's index and element's reference as parameters
	\return A Future<void>. This object is used to know when concurrentFor has finished its job
*/
template<typename InputIterator, typename ParamType>
Future<void> concurrentFor(InputIterator begin, InputIterator end, void (*function)(int index, ParamType&))
{
	return concurrentFor(begin, end, Param2WrapperFunctor<ParamType>(function));
}


//! This version expects a functor which defines operator() accepting 2 parameters: first one a current's element index and the second one the element's reference
/*!
\param begin Range's start position iterator
\param end Range's end position iterator
\param functor A functor which operator () takes element's index and element's reference as parameters
\return A Future<void>. This object is used to know when concurrentFor has finished its job
*/
template<typename InputIterator, typename Functor>
Future<void> concurrentFor(InputIterator begin, InputIterator end, const Functor& functor)
{
	Shared<Job, MutexMTPolicy> job(new ConcurrentForJob<InputIterator, Functor>(begin, end, functor));

	ThreadPool::instance().enqueueJob(job);

	return Future<void>(job);
}

//! This version executes for concurrently and also applies a merging or reducing method.
/*! This version of concurrentFor functions like previous functor version but also expects the functor has defiend a method called "merge", which accepts an object of functor's
		own type. The following is an example to find the maximum value of a vector<int>:
		\code
		struct MaximumFinder
		{
			MaximumFinder() :
				maximum(std::numeric_limits<int>::min())
			{}

			// This member function will be called for every element of vector<int>
			void operator () (int elementIndex, int element)
			{
				maximum = std::max(element, maximum);
			}

			void merge(const MaximumFinder& otherMF)
			{
				maximum = std::max(otherMF.maximum, maximum);
			}

			int maximum;
		};

		std::vector<int> v(getVectorDataFromSomewhere());

		// Notice how Future is not void but MaximumFinder, same type as our functor
		Olagarro::Future<MaximumFinder> forFuture = Olagarro::concurrentReductorFor(v.begin(), v.end(), MaximumFinder());

		std::cout << "v's maximum value is = " << forFuture.result() << std::endl;

		\endcode

		\param begin Range's start position iterator
		\param end Range's end position iterator
		\param functor A functor with operator () and merge() methods defined (see example). Be aware that functor will be copied many times (each thread has its own copy and so does Future return object)
		\return A Future<FunctorType> which holds functor's copy
*/
template<typename InputIterator, typename Functor>
Future<Functor> concurrentReductorFor(InputIterator begin, InputIterator end, Functor functor)
{
	Shared<Job, MutexMTPolicy> job(new ConcurrentReductorForJob<InputIterator, Functor>(begin, end, functor));

	ThreadPool::instance().enqueueJob(job);

	return Future<Functor>(job);
}

}

}

#endif // OLAGARRO_H
