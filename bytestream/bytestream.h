/*
Copyright (c) 2014 iganinja

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
#include <type_traits>

namespace olagarro
{

/**
 * @brief The ByteStream class is a wrapper around a vector of bytes which eases the manipulation of the data stored on
 * it. It offers convenient methods to insert and extract data with different data types and to save to and load data
 * from files in a very simple way.
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
    ByteStream& operator=(const ByteStream& other);

    /**
     * @brief ByteStream Move constructor: takes the other's bytes, which is left empty, reading from the start
     */
    ByteStream(ByteStream&& other) noexcept;

    /**
     * @brief operator = Move assignment: takes the other's bytes, which is left empty, reading from the start
     */
    ByteStream& operator=(ByteStream&& other) noexcept;

    /**
     * @brief add_data Adds size bytes from data to this ByteStream objet
     * @param data Byte array to load data from; it may be null only when size is 0
     * @param size Number of bytes to be copied
     * @throws std::invalid_argument when data is null and size is not 0
     */
    void add_data(const char* data, std::size_t size);

    /**
     * @brief size Returns the size
     * @return Number of bytes ByteStream object's internal buffer
     */
    std::size_t size() const;

    /**
     * @brief data Returns the pointer of internal buffer
     * @return A pointer to object's internal buffer. It is only for reading purposes, it should not be deleted nor
     * freed
     */
    const char* data() const;

    /**
     * @brief read_index Returns the object's read index value
     * @return Number of bytes loaded up until now
     */
    std::size_t read_index() const;

    /**
     * @brief bytes_remaining Returns the number of bytes to object's internal buffer's ending
     * @return Number of bytes from current read index to buffer's ending
     */
    std::size_t bytes_remaining() const;

    /**
     * @brief reset_read_index Resets this object's reading index. Each time read_data() is called this reading index
     * advances the provided size. This method resets it to 0, that is, the beggining of the internal buffer.
     */
    void reset_read_index();

    /**
     * @brief read_data Copies object's internal buffer's size bytes in data array. After the call object's read index
     * has been moved size bytes forward.
     * @param data The array to copy data to
     * @param size Number of bytes to be copied
     */
    void read_data(char* data, std::size_t size);

    template <typename T> ByteStream& write(const T& value);

    template <typename T> ByteStream& read(T& value);

    template <typename T> T read();

    /**
     * @brief can_read_more Tells if it is possible to keep reading from this object
     * @return true if reading index has not reached the internal buffer's ending, false otherwise
     */
    bool can_read_more() const;

    /**
     * @brief load_from_file Loads all the data from a file and sets it to a ByteStream object. That is, all the content
     * of indicated file is copied to system RAM This function can throw exceptions. See std::ifstream::read()
     * documentation
     * @param byte_stream Object to fill with data obtained from provided file
     * @param file_name Absolute file path to load data from
     * @return true if everything went OK
     */
    static bool load_from_file(ByteStream& byte_stream, const std::string& file_name);

    /**
     * @brief save_to_file Saves the content of provided ByteStream object in a file
     * This function can throw exceptions. See std::ifstream::write() documentation
     * @param byte_stream Object to save data from
     * @param file_name Absolute path of a file to write data to. If it does not exist it is created, otherwise
     * overwritten
     * @return true if everything went OK
     */
    static bool save_to_file(const ByteStream& byte_stream, const std::string& file_name);

  private:
    std::vector<char> m_buffer;
    std::size_t m_read_index;
};

/// @brief Convenience insertion operator to add arbitrary data to a ByteStream object
/// @param s The ByteStream object to append data to
/// @param value The data source object: its bytes (sizeof(T) bytes exactly) would be copied at the end of the
/// ByteStream object
/// @return ByteStream object
template <typename T> ByteStream& operator<<(ByteStream& s, const T& value)
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "ByteStream only supports trivially copyable types; provide a dedicated overload for this type");
    const char* p = reinterpret_cast<const char*>(&value);
    s.add_data(p, sizeof(T));
    return s;
}

/// @brief Convenience extraction operator to load arbitrary data from a ByteStream object
/// @param s The data source ByteStream object
/// @param target Target variable to copy bytes to from ByteStream object. The number of bytes copied will be sizeof(T).
/// ByteStream object's read index will be advanced the same number of bytes.
/// @return ByteStream object
template <typename T> ByteStream& operator>>(ByteStream& s, T& target)
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "ByteStream only supports trivially copyable types; provide a dedicated overload for this type");
    char* p = reinterpret_cast<char*>(&target);
    s.read_data(p, sizeof(T));
    return s;
}

template <typename T> ByteStream& ByteStream::write(const T& value)
{
    return *this << value;
}

template <typename T> ByteStream& ByteStream::read(T& value)
{
    return *this >> value;
}

template <typename T> T ByteStream::read()
{
    T value{};
    read(value);
    return value;
}

} // namespace olagarro

#endif // BYTESTREAM_H
