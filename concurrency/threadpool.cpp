#include "threadpool.h"
#include "job.h"

#include <iostream>
#include <typeinfo>

extern tthread::mutex logMutex;

namespace Olagarro
{

namespace Concurrency
{

static const int HardwareThreadMultFactor = 2;

const unsigned HardwareThreadNumber = tthread::thread::hardware_concurrency();

ThreadPool& ThreadPool::instance()
{
	static ThreadPool threadPool;

	if(!threadPool.isRunning())
	{
		threadPool.launch();
	}

	return threadPool;
}

ThreadPool::~ThreadPool()
{
	for(std::size_t i = 0; i < mJobThreads.size(); ++ i)
	{
		mJobThreads[i]->finish();
	}

	while(0 < mPendingJobs.size())
	{
		mPendingJobs.pop();
	}

	finish();
}

ThreadPool::ThreadPool() :
	BlockingThread<ThreadPool>("ThreadPool")
{
	for(unsigned i = 0; i < HardwareThreadNumber * HardwareThreadMultFactor; ++ i)
	{
		Shared<JobThread> jobThread(new JobThread());
		jobThread->launch();
		mJobThreads.push_back(jobThread);
	}
}

void ThreadPool::performJob()
{
	std::size_t i = 0;

	tthread::lock_guard<tthread::mutex> guard(mJobQueueMutex);

	while(0 < mPendingJobs.size() && i < mJobThreads.size())
	{
		if(!mJobThreads[i]->isWorking())
		{
			mJobThreads[i]->setJob(mPendingJobs.front());
			mPendingJobs.pop();
		}

		++ i;
	}
}

void ThreadPool::enqueueJob(Shared<Job, MutexMTPolicy> job)
{
	tthread::lock_guard<tthread::mutex> guard(mJobQueueMutex);

	mPendingJobs.push(job);

	resumeJob();
}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////


ThreadPool::JobThread::JobThread() :
	BlockingThread<JobThread>("JobThread"),
	mJob(0)
{
}

ThreadPool::JobThread::~JobThread()
{
}

void ThreadPool::JobThread::setJob(Shared<Job, MutexMTPolicy> job)
{
	assert(!isWorking() && "ThreadPool::JobThread::setJob(): it is still working!");

	mJob = job;

	resumeJob();
}

void ThreadPool::JobThread::performJob()
{
	assert(!mJob.isNull() && "ThreadPool::JobThread::performJob(): job not set");

	mJob->execute();
}

void ThreadPool::JobThread::performBeforeBlocking()
{
	ThreadPool::instance().resumeJob();
}

}

}
