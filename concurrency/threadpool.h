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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include "blockingthread.h"
#include "mutexmtpolicy.h"


namespace Olagarro
{

namespace Concurrency
{

class Job;

/**
 * @brief Core class for concurrency module: it enqueues Job objects and execute them as soon as it is possible, using all the physical CPU cores available in the system
 */
class ThreadPool : public BlockingThread<ThreadPool>
{
public:
	static ThreadPool &instance();
	~ThreadPool();
	void enqueueJob(Shared<Job, MutexMTPolicy> job);

private:

	class JobThread : public BlockingThread<JobThread>
	{
	public:
		JobThread();
		~JobThread();
		void setJob(Shared<Job, MutexMTPolicy> job);
	private:
		void performJob();
		void performBeforeBlocking();

		Shared<Job, MutexMTPolicy> mJob;
	};

	ThreadPool();
	void performJob();

	std::vector< Shared<JobThread> > mJobThreads;

	tthread::mutex mJobQueueMutex;
	std::queue< Shared<Job, MutexMTPolicy> > mPendingJobs;
};

extern const unsigned HardwareThreadNumber;

}

}

#endif // THREADPOOL_H
