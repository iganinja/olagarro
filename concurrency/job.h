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

#ifndef JOB_H
#define JOB_H

#include "caller.h"
#include "threadpool.h"
#include <memory>
#include "mutexmtpolicy.h"

#include <iostream>
#include <sstream>

namespace Olagarro
{

namespace Concurrency
{

/**
 * @brief Interface for handling jobs or tasks by concurrency module. It encapsulates operations using the Command design pattern
 */
class Job
{
public:
	Job() {}
	virtual ~Job() {}
	virtual std::string name() const = 0;

	void execute()
	{
		executeJob();
	}

private:
	virtual void executeJob() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief The general case of a Job which contains a call to some callable entity (free function, functor or member function)
 */
template<typename ReturnType>
class CallerJob : public Job
{
public:
	CallerJob(Caller<ReturnType>* caller) :
		mCaller(caller),
		mResultCalculated(false)
	{
	}

	~CallerJob()
	{
		if(!mResultCalculated)
		{
			result();
		}
	}

	ReturnType result() const
	{
		tthread::lock_guard<tthread::mutex> guard(mMutex);

		if(mResultCalculated)
		{
			return mResult;
		}

		mCondVariable.wait(mMutex);

		return mResult;
	}

	 std::string name() const
	 {
		 std::stringstream text;
		 text << "CallerJob " << reinterpret_cast<int>(&mCondVariable);
		 return text.str();
	 }

protected:
	void executeJob()
	{
		tthread::lock_guard<tthread::mutex> guard(mMutex);

		mResult = mCaller->performCall();

		mResultCalculated = true;

		mCondVariable.notify_all();
	}

	std::auto_ptr< Caller<ReturnType> > mCaller;
	ReturnType mResult;
	bool mResultCalculated;
	mutable tthread::mutex mMutex;
	mutable tthread::condition_variable mCondVariable;
};

// void return type specialisation
template<>
class CallerJob<void> : public Job
{
public:
	CallerJob(Caller<void>* caller) :
		mCaller(caller),
		mResultCalculated(false)
	{
	}

	~CallerJob()
	{
		if(!mResultCalculated)
		{
			result();
		}
	}

	virtual void result() const
	{
		tthread::lock_guard<tthread::mutex> guard(mMutex);

		if(mResultCalculated)
		{
			return;
		}

		mCondVariable.wait(mMutex);
	}

	std::string name() const
	{
		return "CallerJob (void)";
	}

protected:
	void executeJob()
	{
		mCaller->performCall();

		tthread::lock_guard<tthread::mutex> guard(mMutex);

		mResultCalculated = true;

		mCondVariable.notify_all();
	}

	std::auto_ptr< Caller<void> > mCaller;
	bool mResultCalculated;
	mutable tthread::mutex mMutex;
	mutable tthread::condition_variable mCondVariable;
};

}

}

#endif // JOB_H
