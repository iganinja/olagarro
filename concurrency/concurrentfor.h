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

#ifndef CONCURRENTFOR_H
#define CONCURRENTFOR_H

#include "job.h"

namespace Olagarro
{

namespace Concurrency
{

/**
 * @brief Job used by concurrentFor
 */
template<typename InputIterator, typename Functor>
class ConcurrentForSliceJob : public Job
{
public:
	ConcurrentForSliceJob(int baseIndex, InputIterator begin, InputIterator end, Functor functor) :
		mBaseIndex(baseIndex),
		mBegin(begin),
		mEnd(end),
		mFunctor(functor),
		mJobDone(false)
	{
	}

	~ConcurrentForSliceJob()
	{
		if(!mJobDone)
		{
			result();
		}
	}

	Functor result()
	{
		tthread::lock_guard<tthread::mutex> guard(mMutex);

		if(mJobDone)
		{
			return mFunctor;
		}

		mCondVariable.wait(mMutex);

		return mFunctor;
	}

	std::string name() const
	{
		return "ConcurrentForSliceJob";
	}

private:
	void executeJob()
	{
		tthread::lock_guard<tthread::mutex> guard(mMutex);

		int index = mBaseIndex;
		for(InputIterator it = mBegin; it != mEnd; ++ it, ++ index)
		{
			mFunctor(index, *it);
		}

		mJobDone = true;

		mCondVariable.notify_all();
	}

	mutable tthread::mutex mMutex;
	mutable tthread::condition_variable mCondVariable;

	int mBaseIndex;
	InputIterator mBegin;
	InputIterator mEnd;
	Functor mFunctor;
	bool mJobDone;
};

// An utility function: creates the ConcurrentForSliceJobs needed by concurrentFor
template<typename InputIterator, typename Functor>
std::vector< Shared<Job, MutexMTPolicy> > createJobs(InputIterator begin, InputIterator end, Functor functor)
{
	typedef ConcurrentForSliceJob<InputIterator, Functor> JobType;
	std::vector< Shared<Job, MutexMTPolicy> > jobs(HardwareThreadNumber);

	const unsigned TotalRange = end - begin;
	unsigned currentRange = TotalRange;
	const int ChunkSize = TotalRange / HardwareThreadNumber;

	for(std::size_t i = 0; i < jobs.size(); ++ i)
	{
		unsigned currentChunk = HardwareThreadNumber - 1 == i? ChunkSize + TotalRange % ChunkSize : ChunkSize;

		unsigned currentIndex = i * ChunkSize;

		Shared<Job, MutexMTPolicy> job(new JobType(currentIndex, begin + currentIndex, begin + currentIndex + currentChunk, functor));

		jobs[i] = job;

		currentRange -= ChunkSize;
	}

	return jobs;
}
/**
 * @brief Job that groups all ConcurrentForSliceJob objects
 */
template<typename InputIterator, typename Functor>
class ConcurrentForJob : public CallerJob<void>
{
public:
	ConcurrentForJob(InputIterator begin, InputIterator end, Functor functor) :
		CallerJob<void>(0),
		mBegin(begin),
		mEnd(end),
		mFunctor(functor)
	{
	}

	~ConcurrentForJob()
	{

	}

	std::string name() const
	{
		return "ConcurrentForJob";
	}

private:

	typedef ConcurrentForSliceJob<InputIterator, Functor> JobType;

	void executeJob()
	{
		std::vector< Shared<Job, MutexMTPolicy> > jobs = createJobs(mBegin, mEnd, mFunctor);

		// Launch jobs except last one
		ThreadPool& pool = ThreadPool::instance();

		for(std::size_t i = 0; i < jobs.size() - 1; ++ i)
		{
			pool.enqueueJob(jobs[i]);
		}

		// Now execute last job in our own thread
		jobs.back()->execute();

		// Wait for them all (except last one as it has finished its job when we reach that point)
		for(std::size_t i = 0; i < jobs.size() - 1; ++ i)
		{
			JobType* job = static_cast<JobType*>(jobs[i].get());
			job->result();
		}

		jobs.clear();

		tthread::lock_guard<tthread::mutex> guard(mMutex);

		mResultCalculated = true;

		mCondVariable.notify_all();
	}

	InputIterator mBegin;
	InputIterator mEnd;
	Functor mFunctor;
};

/**
 * @brief Job that groups all ConcurrentForSliceJob objects for concurrentReductorFor function
 */
template<typename InputIterator, typename Functor>
class ConcurrentReductorForJob : public CallerJob<Functor>
{
public:
	ConcurrentReductorForJob(InputIterator begin, InputIterator end, Functor functor) :
		CallerJob<Functor>(0),
		mBegin(begin),
		mEnd(end),
		mFunctor(functor)
	{
	}

	~ConcurrentReductorForJob()
	{
	}

	std::string name() const
	{
		return "ConcurrentReductorForJob";
	}

protected:
	void executeJob()
	{
		mJobs = createJobs(mBegin, mEnd, mFunctor);

		// Launch jobs except last one
		ThreadPool& pool = ThreadPool::instance();

		for(std::size_t i = 0; i < mJobs.size() - 1; ++ i)
		{
			pool.enqueueJob(mJobs[i]);
		}

		// Now execute last job in our own thread
		JobType* job = static_cast<JobType*>(mJobs.back().get());
		job->execute();
		mFunctor.merge(job->result());

		// Wait for them all (except last one as it has finished its job when we reach that point)
		for(std::size_t i = 0; i < mJobs.size() - 1; ++ i)
		{
			JobType* job = static_cast<JobType*>(mJobs[i].get());
			mFunctor.merge(job->result());
		}

		// Using "this->" as otherwise compiler cannot see mMutex and such variables
		tthread::lock_guard<tthread::mutex> guard(this->mMutex);

		this->mResult = mFunctor;
		this->mResultCalculated = true;

		this->mCondVariable.notify_all();
	}

	typedef ConcurrentForSliceJob<InputIterator, Functor> JobType;
	std::vector< Shared<Job, MutexMTPolicy> > mJobs;

	InputIterator mBegin;
	InputIterator mEnd;
	Functor mFunctor;
};

}

}

#endif // CONCURRENTFOR_H
