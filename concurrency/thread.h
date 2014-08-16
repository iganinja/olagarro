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

#ifndef THREAD_H
#define THREAD_H

#include <string>
#include "tinythread/tinythread.h"
#include "../common/shared.h"

#include <iostream>


namespace Olagarro
{

namespace Concurrency
{

/**
 * @brief Basic thread class built on top of tinythread++
 */
template<typename HostClass>
class Thread
{
public:
	Thread(const std::string& name) :
		mName(name),
		mRunning(false)
	{
	}

	virtual ~Thread()
	{
		join();
	}

	void start()
	{
#ifdef OLAGARRO_LOG
		std::cout << "[" << name() << "]" << "Thread: Creating thread" << std::endl;
#endif
		mTThread.reset(new tthread::thread(threadMethod, this));

		// Wait until threadMethod is running in its own thread
		while(!isRunning())
		{
#ifdef OLAGARRO_LOG
			std::cout << "[" << name() << "]" << "Thread: Waiting for thread creation" << std::endl;
#endif
			tthread::this_thread::sleep_for(tthread::chrono::milliseconds(10));
		}

#ifdef OLAGARRO_LOG
		std::cout << "[" << name() << "]" << "Thread: Thread created" << std::endl;
#endif
	}

	bool isRunning()
	{
		return mRunning;
	}

	void join()
	{
		if(!mTThread.isNull())
		{
			mTThread->join();
		}
	}

	const std::string& name() const
	{
		return mName;
	}

protected:
	virtual void run() = 0;

private:
	static void threadMethod(void* param)
	{
		Thread<HostClass>* instance = reinterpret_cast<Thread<HostClass>*>(param);

		instance->mRunning = true;

		instance->run();

		instance->mRunning = false;
	}

	Shared<tthread::thread> mTThread;
	std::string mName;
	bool mRunning;
};

}

}

#endif // THREAD_H
