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

#ifndef BLOCKINGTHREAD_H
#define BLOCKINGTHREAD_H

#include <string>
#include "thread.h"
#include "../common/shared.h"
#include <cassert>

namespace Olagarro
{

namespace Concurrency
{

/**
 * @brief A thread which its blocked by default, does its job and then blocks itself again
 */
template<typename HostClass>
class BlockingThread : public Thread<HostClass>
{
public:
	BlockingThread(const std::string& name) :
		Thread<HostClass>(name),
		mFinishThread(false),
		mWorking(false)
	{
	}

	~BlockingThread()
	{
		finish();
	}

	/**
	 * @brief launch Creates a new thread which will be blocked immediately. After that call use "resumeJob()" to start thread's job
	 */
	void launch()
	{
		Thread<HostClass>::start();
	}

	void run()
	{
		preJobTasks();

		while(!mFinishThread)
		{
			tthread::lock_guard<tthread::mutex> guard(mJobMutex);
			mJobStartCondVar.wait(mJobMutex);

			if(mFinishThread)
			{
				break; // If finish() has been called while we were blocked
			}

			performJob();

			performBeforeBlocking();

			mWorking = false;
		}

		postJobTasks();
	}

	void resumeJob()
	{
		assert(Thread<HostClass>::isRunning() && "BlockingThread::resumeJob(): thread is not running");

		mWorking = true;

		mJobStartCondVar.notify_all();
	}

	/**
	 * @brief waitForJob Calling thread blocks until current job is finished. Then job's thread gets blocked to next resume() or finish() call but calling thread remains unblocked
	 */
	void waitForJob()
	{
		if(!Thread<HostClass>::isRunning())
		{
			return; // Thread already finished, just return
		}

		// Just try to lock mJobMutex. If thread is working then we get blocked until it finishes, otherwise thread is waiting and we can exit that method
		tthread::lock_guard<tthread::mutex> guard(mJobMutex);
	}

	/**
	 * @brief finish Blocks caling thread until BlockingThread finishes everything and destroys previouls created thread with launch()
	 */
	void finish()
	{
		if(!Thread<HostClass>::isRunning())
		{
			return;
		}

		mFinishThread = true;

		mJobStartCondVar.notify_all();

		Thread<HostClass>::join();
	}

	bool isWorking() const
	{
		return mWorking;
	}

protected:
	/**
	 * @brief preJobTasks Called before thread gets blocked in its job loop (in same thread)
	 */
	virtual void preJobTasks() {}
	/**
	 * @brief performJob Called each time resumeJob() gets called
	 */
	virtual void performJob() = 0;
	/**
	 * @brief performBeforeBlocking Called just before job loop is about to block again. It is not called the first time that loop gets blocked: it gets
	 * called first time after first perfomJob() have finished
	 */
	virtual void performBeforeBlocking() {}
	/**
	 * @brief postJobTasks Called after thread leaves its job loop (in same thread)
	 */
	virtual void postJobTasks() {}

private:
	tthread::mutex mJobMutex;
	tthread::mutex mWaitMutex;
	mutable tthread::mutex mWorkingMutex;
	tthread::condition_variable mJobStartCondVar;
	tthread::condition_variable mWaitCondVar;
	bool mFinishThread;
	bool mWorking;
};

}

}

#endif // BLOCKINGTHREAD_H
