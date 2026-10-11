# ByteStream

`olagarro::ByteStream` (`bytestream/bytestream.h`, `bytestream/bytestream.cpp`) is a growable buffer of bytes with a
read position: values are written at its end and read back from the front in the same order, and the whole buffer
can be saved to a file and loaded from one. It is meant for binary data a program writes and reads itself, such as
saved games or caches.

- [Writing and reading](#writing-and-reading)
- [What it can hold](#what-it-can-hold)
- [Your own types](#your-own-types)
- [The format](#the-format)
- [Files](#files)
- [API](#api)

The CMake target is `olagarro::ByteStream`; with the module adapters, `import olagarro.bytestream;`.

## Writing and reading

```cpp
#include <bytestream/bytestream.h>

olagarro::ByteStream stream;
stream.write(42).write(3.5f);           // or: stream << 42 << 3.5f;

const int answer{stream.read<int>()};   // 42
float value{};
stream.read(value);                     // 3.5f; or: stream >> value;
```

Writing appends `sizeof(T)` bytes to the end of the buffer. Reading copies `sizeof(T)` bytes from the read position
and moves it forward, so values come back in the order they were written. Writing never moves the read position, and
reading never removes anything: `reset_read_index()` goes back to the start and reads it all again.

A read that asks for more bytes than remain throws `std::out_of_range` and reads nothing; the read position stays
where it was. `can_read_more()` and `bytes_remaining()` say whether there is anything left before trying.

## What it can hold

`write`, `read` and the `<<` and `>>` operators take any *trivially copyable* type: numbers, `enum`s, plain structs
of them, and fixed-size arrays of them. Anything else, `std::string`, `std::vector`, a class with pointers, fails to
compile with:

```
ByteStream only supports trivially copyable types; provide a dedicated overload for this type
```

That is on purpose: copying the bytes of a `std::string` would copy a pointer into memory that will not be there when
the file is loaded.

`read<T>()` returns a `T` built with `T{}` and then filled, so it needs `T` to be default-constructible; `read(value)`
fills an existing variable and does not.

## Your own types

A type that is not trivially copyable is written by an overload of `<<` and `>>` that writes its parts. The usual
shape stores a size first and then the elements:

```cpp
namespace olagarro
{

ByteStream& operator<<(ByteStream& stream, const std::string& text)
{
    stream << static_cast<std::uint64_t>(text.size());
    stream.add_data(text.data(), text.size());
    return stream;
}

ByteStream& operator>>(ByteStream& stream, std::string& text)
{
    std::uint64_t size{};
    stream >> size;
    text.resize(static_cast<std::size_t>(size));
    stream.read_data(text.data(), text.size());
    return stream;
}

template <typename T> ByteStream& operator<<(ByteStream& stream, const std::vector<T>& values)
{
    stream << static_cast<std::uint64_t>(values.size());
    for(const auto& value : values)
    {
        stream << value;
    }
    return stream;
}

} // namespace olagarro
```

Overload resolution prefers these to the generic template, so the `static_assert` no longer fires for them, and they
compose: a `std::vector<std::string>` goes through both. `tests/bytestream/main.cpp` writes strings and vectors this
way.

Where the overload is declared matters. `stream << text` written in your own code finds an overload anywhere ordinary
lookup sees it. But `stream.write(text)` and `stream.read(text)` call `<<` and `>>` from inside the library, where only
the namespaces of the arguments are searched: `olagarro`, for `ByteStream`, and the namespace of your type. Declare the
overloads for your own types in your type's namespace, and those for standard types, which you cannot add to `std`,
in `olagarro`, as above; declared at global scope they work with the operators but not with `write` and `read`.

`add_data(pointer, size)` and `read_data(pointer, size)` move raw bytes, for contents whose size is only known at run
time. `read_data` throws `std::invalid_argument` when given a null pointer for a non-zero size; `add_data` does not
check its pointer.

## The format

There is none beyond the bytes themselves. A value is stored as it is in memory: `sizeof(T)` bytes, in the machine's
byte order, structs with whatever padding the compiler put in them. Nothing records types, sizes or a version; the
reader must read exactly what the writer wrote, in the same order.

That makes the data as fast to write and read as a copy, and as portable as the types in it:

- A file read on a machine of the other byte order, or built by a compiler that lays a struct out differently, reads
  wrong values without any error.
- Types whose size changes between platforms (`long`, `std::size_t`, `wchar_t`) change the format. Fixed-width types,
  `std::uint32_t` and `std::uint64_t`, keep it the same wherever the byte order is.
- A format that must change over time needs a version number written first, by the program, and checked on reading.

## Files

```cpp
olagarro::ByteStream save;
save << player.position << player.health;
if(!olagarro::ByteStream::save_to_file(save, "save.bin"))
{
    // could not be written
}

olagarro::ByteStream loaded;
if(olagarro::ByteStream::load_from_file(loaded, "save.bin"))
{
    loaded >> player.position >> player.health;
}
```

`save_to_file` writes the whole buffer, creating the file or replacing what it had, and returns whether the write
succeeded. `load_from_file` replaces the stream's contents with the whole file and resets its read position; it
returns `false` when the file cannot be opened or read. On failure the stream may be left partly changed, so a stream
whose load failed should not be read. Both are static, and both use C++'s file streams, so their errors follow
`std::ifstream` and `std::ofstream`.

## API

| Member | What it does |
|--------|--------------|
| `ByteStream()` | An empty stream. |
| `ByteStream(const std::vector<char>& buffer)` | A stream holding a copy of the bytes, read position at the start. |
| copy constructor and copy assignment | Copy the bytes; the copy's read position starts at 0, whatever the original's was. |
| `write(value)`, `operator<<` | Append a value's bytes. Return the stream, so calls chain. |
| `read(value)`, `operator>>` | Fill a value from the next bytes. Return the stream. |
| `read<T>()` | The next `T`. |
| `add_data(data, size)` | Append `size` raw bytes. |
| `read_data(data, size)` | Copy the next `size` bytes into `data`. |
| `size()` | Bytes held. |
| `data()` | The bytes, read only; `nullptr` when empty. |
| `read_index()` | Bytes read so far. |
| `bytes_remaining()` | Bytes after the read position. |
| `can_read_more()` | Whether any bytes remain to read. |
| `reset_read_index()` | Back to the start. |
| `load_from_file(stream, path)` | Static: the whole file into `stream`. |
| `save_to_file(stream, path)` | Static: `stream` into the file. |

There is no move constructor: moving a `ByteStream` copies it, which matters only for large buffers.
