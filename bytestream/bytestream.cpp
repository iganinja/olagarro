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

#include "bytestream.h"
#include <cassert>
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace Olagarro
{

ByteStream::ByteStream() :
	mReadIndex(0)
{
}

ByteStream::ByteStream(const std::vector<char>& buffer) :
	mBuffer(buffer),
	mReadIndex(0)
{
}

ByteStream::ByteStream(const ByteStream& other) :
	mBuffer(other.mBuffer),
	mReadIndex(0)
{
}

ByteStream& ByteStream::operator = (const ByteStream& other)
{
	if(this != &other)
	{
		mBuffer = other.mBuffer;
		mReadIndex = 0;
	}
	return *this;
}

std::size_t ByteStream::size() const
{
	return mBuffer.size();
}

const char* ByteStream::data() const
{
	return &mBuffer[0];
}

std::size_t ByteStream::readIndex() const
{
	return mReadIndex;
}

std::size_t ByteStream::bytesRemaining() const
{
	return size() - mReadIndex;
}

void ByteStream::resetReadIndex()
{
	mReadIndex = 0;
}

void ByteStream::readData(char* data, std::size_t size)
{
	std::vector<char>::iterator begin = mBuffer.begin() + mReadIndex;
	std::vector<char>::iterator end = begin + size;
	std::copy(begin, end, data);
	mReadIndex += size;

	assert(mReadIndex <= mBuffer.size() && "Serializer::readData() out of index");
}

bool ByteStream::canReadMore() const
{
	return mBuffer.size() > mReadIndex;
}

bool ByteStream::loadFromFile(ByteStream& byteStream, const std::string& fileName)
{
	std::ifstream file(fileName.c_str(), std::ios_base::in | std::ios_base::binary);

	if(!file.is_open())
	{
		return false;
	}

	// Get file size
	file.seekg(0, std::ios_base::end);
	unsigned long size = file.tellg();
	file.seekg(0, std::ios_base::beg);

	byteStream.mBuffer.resize(size);

	file.read(&byteStream.mBuffer[0], size);

	byteStream.mReadIndex = 0;

	return true;
}

bool ByteStream::saveToFile(const ByteStream& byteStream, const std::string& fileName)
{
	std::ofstream file(fileName.c_str(), std::ios_base::out | std::ios_base::binary);

	if(!file.is_open())
	{
		return false;
	}

	file.write(byteStream.data(), byteStream.size());

	file.flush();

	return true;
}

void ByteStream::addData(const char* data, std::size_t size)
{
	std::size_t originalSize = mBuffer.size();
	mBuffer.resize(originalSize + size);
	std::copy(data, data + size, mBuffer.begin() + originalSize);
}

}
