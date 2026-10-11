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

#include "bytestream.h"
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <system_error>
#include <utility>

namespace olagarro
{

ByteStream::ByteStream() : m_read_index{0}
{
}

ByteStream::ByteStream(const std::vector<char>& buffer) : m_buffer(buffer), m_read_index{0}
{
}

ByteStream::ByteStream(const ByteStream& other) : m_buffer(other.m_buffer), m_read_index{0}
{
}

ByteStream& ByteStream::operator=(const ByteStream& other)
{
    if(this != &other)
    {
        m_buffer = other.m_buffer;
        m_read_index = 0;
    }
    return *this;
}

ByteStream::ByteStream(ByteStream&& other) noexcept : m_buffer(std::move(other.m_buffer)), m_read_index{0}
{
    other.m_buffer.clear();
    other.m_read_index = 0;
}

ByteStream& ByteStream::operator=(ByteStream&& other) noexcept
{
    if(this != &other)
    {
        m_buffer = std::move(other.m_buffer);
        m_read_index = 0;
        other.m_buffer.clear();
        other.m_read_index = 0;
    }
    return *this;
}

std::size_t ByteStream::size() const
{
    return m_buffer.size();
}

const char* ByteStream::data() const
{
    return m_buffer.empty() ? nullptr : m_buffer.data();
}

std::size_t ByteStream::read_index() const
{
    return m_read_index;
}

std::size_t ByteStream::bytes_remaining() const
{
    return size() - m_read_index;
}

void ByteStream::reset_read_index()
{
    m_read_index = 0;
}

void ByteStream::read_data(char* data, std::size_t size)
{
    if(size > bytes_remaining())
    {
        throw std::out_of_range("ByteStream::read_data(): requested bytes exceed the remaining buffer");
    }

    if(size == 0)
    {
        return;
    }

    if(!data)
    {
        throw std::invalid_argument("ByteStream::read_data(): data must not be null when size is non-zero");
    }

    std::copy_n(m_buffer.data() + m_read_index, size, data);
    m_read_index += size;
}

bool ByteStream::can_read_more() const
{
    return m_buffer.size() > m_read_index;
}

bool ByteStream::load_from_file(ByteStream& byte_stream, const std::string& file_name)
{
    // A folder opens as a file on some systems, and says it is all but endless.
    std::error_code error;
    if(!std::filesystem::is_regular_file(file_name, error))
    {
        return false;
    }

    std::ifstream file{file_name.c_str(), std::ios_base::in | std::ios_base::binary};

    if(!file.is_open())
    {
        return false;
    }

    // Get file size
    file.seekg(0, std::ios_base::end);
    const std::streamoff file_size{file.tellg()};
    if(file_size < 0 || static_cast<unsigned long long>(file_size) > std::numeric_limits<std::size_t>::max())
    {
        return false;
    }
    file.seekg(0, std::ios_base::beg);

    // Read aside first: a file that cannot be read whole leaves the stream as it was.
    const std::size_t size{static_cast<std::size_t>(file_size)};
    std::vector<char> bytes(size);

    if(size > 0)
    {
        file.read(bytes.data(), static_cast<std::streamsize>(size));
    }

    if(!file && size > 0)
    {
        return false;
    }

    byte_stream.m_buffer = std::move(bytes);
    byte_stream.m_read_index = 0;

    return true;
}

bool ByteStream::save_to_file(const ByteStream& byte_stream, const std::string& file_name)
{
    std::ofstream file{file_name.c_str(), std::ios_base::out | std::ios_base::binary};

    if(!file.is_open())
    {
        return false;
    }

    if(byte_stream.size() > 0)
    {
        file.write(byte_stream.data(), static_cast<std::streamsize>(byte_stream.size()));
    }

    file.flush();

    return static_cast<bool>(file);
}

void ByteStream::add_data(const char* data, std::size_t size)
{
    if(size == 0)
    {
        return;
    }
    if(!data)
    {
        throw std::invalid_argument("ByteStream::add_data(): data must not be null when size is non-zero");
    }
    const std::size_t original_size{m_buffer.size()};
    m_buffer.resize(original_size + size);
    std::copy(data, data + size, m_buffer.begin() + original_size);
}

} // namespace olagarro
