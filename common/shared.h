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

#ifndef Shared_H
#define Shared_H

#include <cassert>

#include <iostream>

namespace Olagarro
{


template<typename T>
class Guard
{
public:
	Guard(T** lockerObj) :
		mLockerObj(lockerObj)
	{
		assert(*lockerObj && "Guard::Guard(): Passed object is not a valid one");
		(*mLockerObj)->lock();
	}
	~Guard()
	{
		if(*mLockerObj)
		{
			(*mLockerObj)->unlock();
		}
	}

private:
	T** mLockerObj;
};

struct VoidMTPolicy
{
public:
	VoidMTPolicy()
	{
	}
	inline void lock(){}
	inline void unlock(){}
};


template<typename T, typename MTPolicy = VoidMTPolicy>
class Shared
{
public:
	Shared() :
		mMTPolicyObj(0),
		mInstance(0),
		mCounter(0)
	{
	}

	Shared(T* instance) :
		mMTPolicyObj(instance? new MTPolicy() : 0),
		mInstance(instance),
		mCounter(instance? new int(1) : 0)
	{
	}

	Shared(const Shared<T, MTPolicy>& other) :
		mMTPolicyObj(other.mMTPolicyObj),
		mInstance(other.mInstance),
		mCounter(other.mCounter)
	{
		increaseReference();
	}

	Shared<T, MTPolicy>& operator = (const Shared<T, MTPolicy>& other)
	{
		if(this != &other)
		{
			// Get rid of our old reference
			decreaseReference();

			mMTPolicyObj = other.mMTPolicyObj;
			mInstance = other.mInstance;
			mCounter = other.mCounter;

			increaseReference();
		}

		return *this;
	}

	~Shared()
	{
		decreaseReference();
	}

	void reset(T* instance)
	{
		decreaseReference();

		mMTPolicyObj = new MTPolicy();
		mInstance = instance;
		mCounter = new int(1);
	}

	const T* get() const
	{
		return mInstance;
	}

	T* get()
	{
		return mInstance;
	}

	T& operator * ()
	{
		return *mInstance;
	}

	const T& operator * () const
	{
		return *mInstance;
	}

	T* operator -> ()
	{
		return mInstance;
	}

	const T* operator -> () const
	{
		return mInstance;
	}

	bool isNull() const
	{
		return 0 == mCounter;
	}

private:
	void decreaseReference()
	{
		if(!mMTPolicyObj)
		{
			return;
		}

		Guard<MTPolicy> guard(&mMTPolicyObj);

		// If mMTPolicyObj has been deleted while we were blocked
		if(!mMTPolicyObj)
		{
			return;
		}

		if(0 >= --(*mCounter))
		{
			delete mInstance;
			mInstance = 0;

			delete mCounter;
			mCounter = 0;

			mMTPolicyObj->unlock();
			delete mMTPolicyObj;
			mMTPolicyObj = 0;
		}
	}

	void increaseReference()
	{
		if(!mMTPolicyObj)
		{
			return;
		}

		Guard<MTPolicy> guard(&mMTPolicyObj);

		++ (*mCounter);
	}


	MTPolicy* mMTPolicyObj;
	T* mInstance;
	int* mCounter;
};

}

#endif // Shared_H
