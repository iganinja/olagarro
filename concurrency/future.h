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

#ifndef FUTURE_H
#define FUTURE_H

#include "tinythread/tinythread.h"
#include "job.h"
#include "../common/shared.h"
#include "mutexmtpolicy.h"

namespace Olagarro
{

namespace Concurrency
{

/**
	* @brief Represents a value in the future.
	*
	* This class stores a concurrent operation's value, allowing its user to safely access it. This class is no meant to be created directly but to get
	* from concurrence module's operations such launchJob() or concurrentFor(). For example:
	*
	* \code
	* // Somewhere we have that function:
	*	float veryHeavyOperation();
	*
	*  // We launch that function on its own thread and then do other work
	*  Future<float> heavyOpValue = Olagarro::launchJob(veryHeavyOperation);
	*
	*  float value1 = doSomething();
	*  float value2 = doSomeOtherThing();
	*
	*  // Finally we need veryHeavyOperation()'s result
	*  float total = value1 + value2 + heavyOpValue.result();
	* \endcode
	*
	* If veryHeavyOperation() has finished when call to heavyOpValue.result() is done, result() method will return its value immediately. If in the other hand
	* function is still working, calling result() will block that thread until its value is calculated. That is, calling result() can block the thread which calls it.
	*
	* We can use Future objects with containers, like the following example:
	*
	* \code
	* // Same operation but some configuring parameter
	* float veryHeavyOperation(int someConfigParam);
	*
	* std::vector< Future<float> > heavyOpValues(10);
	*
	* for(std::size_t i = 0; i < heavyOpValues.size(); ++ i)
	* {
	*   heavyOpValues[i] = Olagarro::launchJob(veryHeavyOperation, i);
	* }
	*
	* // All operations are running at this point, lets calculate their average
	* float sum = 0;
	*
	* for(std::size_t i = 0; i < heavyOpValues.size(); ++ i)
	* {
	*   sum += heavyOpValues[i].result(); // Block until value is calculated if neccesary
	* }
	*
	* float average = sum / static_cast<float>(heavyOpValues.size());
	* \endcode
	*
	* Same as with launchJob() function, Future works with concurrentFor() and concurrentReductorFor() functions. First one only returns Future<void>
	* (just to wait for it) but second function returns Future<OperationResultType> object.
	*
*/
template<typename T>
class Future
{
public:
	Future()
	{
	}

	Future(Shared<Job, MutexMTPolicy> job) :
		mJob(job)
	{
	}

	/**
	 * @brief Returns operation's result or blocks until it is calculated.
	 * @return Operation's result
	 */
	T result() const
	{
		const CallerJob<T>* job = static_cast<const CallerJob<T>*>(mJob.get());
		return job->result();
	}

private:
	Shared<Job, MutexMTPolicy> mJob;
};

}

}

#endif // FUTURE_H
