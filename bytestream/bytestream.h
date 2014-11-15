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

#ifndef BYTESTREAM_H
#define BYTESTREAM_H

#include <vector>
#include <string>

namespace Olagarro
{

/**
 * @brief The ByteStream class is a wrapper around a vector of bytes which eases the manipulation of the data stored on it. It offers convenient methods to insert and extract data with
 * different data types and to save to and load data from files in a very simple way.
 */
class ByteStream
{
public:
	ByteStream();


	/**
	 * @brief ByteStream Creates a ByteStream object with the data provided in buffer parameter
	 * @param buffer A vector of bytes to copy data from
	 */
	ByteStream(const std::vector<char>& buffer);

	/**
	 * @brief ByteStream Copy constructor
	 * @param other Another ByteStream object which data is copied from
	 */
	ByteStream(const ByteStream& other);

	/**
	 * @brief operator = Assigment operator
	 * @param other Another ByteStream object which data is copied from
	 * @return
	 */
	ByteStream& operator = (const ByteStream& other);

	/**
	 * @brief addData Adds size bytes from data to this ByteStream objet
	 * @param data Byte array to load data fro
	 * @param size Number of bytes to be copied
	 */
	void addData(const char* data, std::size_t size);

	/**
	 * @brief size Returns the size
	 * @return Number of bytes ByteStream object's internal buffer
	 */
	std::size_t size() const;

	/**
	 * @brief data Returns the pointer of internal buffer
	 * @return A pointer to object's internal buffer. It is only for reading purposes, it should not be deleted nor freed
	 */
	const char* data() const;

	/**
	 * @brief readIndex Returns the object's read index value
	 * @return Number of bytes loaded up until now
	 */
	std::size_t readIndex() const;

	/**
	 * @brief bytesRemaining Returns the number of bytes to object's internal buffer's ending
	 * @return Number of bytes from current read index to buffer's ending
	 */
	std::size_t bytesRemaining() const;

	/**
	 * @brief resetReadIndex Resets this object's reading index. Each time readData() is called this reading index advances
	 * the provided size. This method resets it to 0, that is, the beggining of the internal buffer.
	 */
	void resetReadIndex();

	/**
	 * @brief readData Copies object's internal buffer's size bytes in data array. After the call object's read index has been
	 * moved size bytes forward.
	 * @param data The array to copy data to
	 * @param size Number of bytes to be copied
	 */
	void readData(char* data, std::size_t size);

	/**
	 * @brief canReadMore Tells if it is possible to keep reading from this object
	 * @return true if reading index has not reached the internal buffer's ending, false otherwise
	 */
	bool canReadMore() const;

	/**
	 * @brief loadFromFile Loads all the data from a file and sets it to a ByteStream object. That is, all the content of indicated file is copied to system RAM
	 * This function can throw exceptions. See std::ifstream::read() documentation
	 * @param byteStream Object to fill with data obtained from provided file
	 * @param fileName Absolute file path to load data from
	 * @return true if everything went OK
	 */
	static bool loadFromFile(ByteStream& byteStream, const std::string& fileName);

	/**
	 * @brief saveToFile Saves the content of provided ByteStream object in a file
	 * This function can throw exceptions. See std::ifstream::write() documentation
	 * @param byteStream Object to save data from
	 * @param fileName Absolute path of a file to write data to. If it does not exist it is created, otherwise overwritten
	 * @return true if everything went OK
	 */
	static bool saveToFile(const ByteStream& byteStream, const std::string& fileName);

private:
	std::vector<char> mBuffer;
	std::size_t mReadIndex;
};


/// @brief Convenience insertion operator to add arbitrary data to a ByteStream object
/// @param s The ByteStream object to append data to
/// @param value The data source object: its bytes (sizeof(T) bytes exactly) would be copied at the end of the ByteStream object
/// @return ByteStream object
template<typename T>
ByteStream& operator << (ByteStream& s, const T& value)
{
	const char* p = reinterpret_cast<const char*>(&value);
	s.addData(p, sizeof(T));
	return s;
}

/// @brief Convenience extraction operator to load arbitrary data from a ByteStream object
/// @param s The data source ByteStream object
/// @param target Target variable to copy bytes to from ByteStream object. The number of bytes copied will be sizeof(T). ByteStream object's read index will be advanced the same number
/// of bytes.
/// @return ByteStream object
template<typename T>
ByteStream& operator >> (ByteStream& s, T& target)
{
	char* p = reinterpret_cast<char*>(&target);
	s.readData(p, sizeof(T));
	return s;
}

}

#endif // BYTESTREAM_H
