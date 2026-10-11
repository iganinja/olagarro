#include <oui/inflate.h>

#include <array>
#include <cstdint>
#include <stdexcept>

namespace olagarro::oui
{

namespace
{

[[noreturn]] void broken(const char* why)
{
    throw std::runtime_error{std::string{"Not a whole gzip file: "} + why};
}

// The bits of a deflate stream, from the lowest of each byte up, as deflate writes them.
class Bits
{
  public:
    Bits(std::string_view data, std::size_t start) : m_data{data}, m_next{start} {}

    [[nodiscard]] std::uint32_t take(int count)
    {
        while(m_count < count)
        {
            if(m_next >= m_data.size())
            {
                broken("it ends in the middle");
            }
            m_buffer |= static_cast<std::uint32_t>(static_cast<unsigned char>(m_data[m_next++])) << m_count;
            m_count += 8;
        }
        const auto value{m_buffer & ((1u << count) - 1u)};
        m_buffer >>= count;
        m_count -= count;
        return value;
    }

    // To the next whole byte, as a stored block starts.
    void align()
    {
        m_buffer = 0;
        m_count = 0;
    }

    [[nodiscard]] std::size_t position() const { return m_next; }
    void skip(std::size_t bytes) { m_next += bytes; }
    [[nodiscard]] std::string_view data() const { return m_data; }

  private:
    std::string_view m_data;
    std::size_t m_next;
    std::uint32_t m_buffer{};
    int m_count{};
};

// A canonical Huffman code: how many codes there are of each length, and the symbols in the order of their codes.
struct Huffman
{
    std::array<std::uint16_t, 16> count{};
    std::array<std::uint16_t, 288> symbol{};
};

// The code whose lengths these are, a length for each symbol in order, 0 for one that has none.
[[nodiscard]] Huffman huffman(const std::uint8_t* lengths, int symbols)
{
    Huffman code;
    for(int index{}; index < symbols; ++ index)
    {
        ++ code.count[lengths[index]];
    }
    std::array<std::uint16_t, 16> offsets{};
    for(int length{1}; length < 15; ++ length)
    {
        offsets[static_cast<std::size_t>(length) + 1] = static_cast<std::uint16_t>(
            offsets[static_cast<std::size_t>(length)] + code.count[static_cast<std::size_t>(length)]);
    }
    for(int index{}; index < symbols; ++ index)
    {
        if(lengths[index] != 0)
        {
            code.symbol[offsets[lengths[index]]++] = static_cast<std::uint16_t>(index);
        }
    }
    return code;
}

// The next symbol, read a bit at a time: codes of each length come after all the shorter ones.
[[nodiscard]] int decode(Bits& bits, const Huffman& code)
{
    int first{};
    int index{};
    int value{};
    for(std::size_t length{1}; length < 16; ++ length)
    {
        value |= static_cast<int>(bits.take(1));
        const int count{code.count[length]};
        if(value - count < first)
        {
            return code.symbol[static_cast<std::size_t>(index + (value - first))];
        }
        index += count;
        first += count;
        first <<= 1;
        value <<= 1;
    }
    broken("a code that is no code");
}

constexpr std::array<std::uint16_t, 29> length_base{3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> length_extra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> distance_base{1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                                      33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                                      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> distance_extra{0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// A block's literals and copies, until its end.
void codes(Bits& bits, std::string& out, const Huffman& lengths, const Huffman& distances)
{
    while(true)
    {
        const auto symbol{decode(bits, lengths)};
        if(symbol < 256)
        {
            out += static_cast<char>(symbol);
            continue;
        }
        if(symbol == 256)
        {
            return;
        }
        const auto which{static_cast<std::size_t>(symbol - 257)};
        if(which >= length_base.size())
        {
            broken("a length that is no length");
        }
        const auto length{length_base[which] + bits.take(length_extra[which])};
        const auto code{static_cast<std::size_t>(decode(bits, distances))};
        if(code >= distance_base.size())
        {
            broken("a distance that is no distance");
        }
        const auto distance{distance_base[code] + bits.take(distance_extra[code])};
        if(distance > out.size())
        {
            broken("a copy from before the start");
        }
        // One at a time: what is copied may be what is being written, as a run of one letter is.
        for(std::uint32_t copied{}; copied < length; ++ copied)
        {
            out += out[out.size() - distance];
        }
    }
}

[[nodiscard]] std::uint32_t crc32(std::string_view data)
{
    std::uint32_t crc{0xFFFFFFFFu};
    for(const auto character : data)
    {
        crc ^= static_cast<unsigned char>(character);
        for(int bit{}; bit < 8; ++ bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

[[nodiscard]] std::uint32_t little_endian(std::string_view data, std::size_t at)
{
    std::uint32_t value{};
    for(std::size_t index{}; index < 4; ++ index)
    {
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + index])) << (8 * index);
    }
    return value;
}

} // namespace

std::string inflate_gzip(std::string_view compressed)
{
    // The header: the magic number, deflate, its flags, and what they say follows: extra fields, a name, a comment,
    // a check of the header.
    if(compressed.size() < 18 || static_cast<unsigned char>(compressed[0]) != 0x1F ||
       static_cast<unsigned char>(compressed[1]) != 0x8B || compressed[2] != 8)
    {
        broken("no gzip header");
    }
    const auto flags{static_cast<unsigned char>(compressed[3])};
    std::size_t start{10};
    if((flags & 0x04) != 0)
    {
        start += 2 + (static_cast<unsigned char>(compressed[start]) |
                      (static_cast<unsigned char>(compressed[start + 1]) << 8));
    }
    for(const auto text : {0x08, 0x10})
    {
        if((flags & text) != 0)
        {
            while(start < compressed.size() && compressed[start] != '\0')
            {
                ++ start;
            }
            ++ start;
        }
    }
    if((flags & 0x02) != 0)
    {
        start += 2;
    }

    Bits bits{compressed, start};
    std::string out;
    auto last{false};
    while(!last)
    {
        last = bits.take(1) == 1;
        const auto type{bits.take(2)};
        if(type == 0)
        {
            // Stored: its length and that length's complement, then the bytes as they are.
            bits.align();
            const auto at{bits.position()};
            if(at + 4 > compressed.size())
            {
                broken("it ends in the middle");
            }
            const auto length{static_cast<unsigned char>(compressed[at]) |
                              (static_cast<unsigned char>(compressed[at + 1]) << 8)};
            const auto check{static_cast<unsigned char>(compressed[at + 2]) |
                             (static_cast<unsigned char>(compressed[at + 3]) << 8)};
            if((length ^ 0xFFFF) != check || at + 4 + static_cast<std::size_t>(length) > compressed.size())
            {
                broken("a stored block that does not add up");
            }
            out.append(compressed.substr(at + 4, static_cast<std::size_t>(length)));
            bits.skip(4 + static_cast<std::size_t>(length));
        }
        else if(type == 1)
        {
            // Fixed codes, which deflate defines once for all.
            static const auto fixed{
                []
                {
                    std::array<std::uint8_t, 288> lengths{};
                    for(std::size_t symbol{}; symbol < 288; ++ symbol)
                    {
                        lengths[symbol] = symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
                    }
                    std::array<std::uint8_t, 30> distances{};
                    distances.fill(5);
                    return std::pair{huffman(lengths.data(), 288), huffman(distances.data(), 30)};
                }()};
            codes(bits, out, fixed.first, fixed.second);
        }
        else if(type == 2)
        {
            // Codes of the block's own, their lengths given by a code of its own too.
            const auto literals{static_cast<int>(bits.take(5)) + 257};
            const auto distances{static_cast<int>(bits.take(5)) + 1};
            const auto code_lengths{static_cast<int>(bits.take(4)) + 4};
            static constexpr std::array<std::uint8_t, 19> order{16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                                11, 4,  12, 3, 13, 2, 14, 1, 15};
            std::array<std::uint8_t, 19> lengths_of_lengths{};
            for(int index{}; index < code_lengths; ++ index)
            {
                lengths_of_lengths[order[static_cast<std::size_t>(index)]] = static_cast<std::uint8_t>(bits.take(3));
            }
            const auto length_code{huffman(lengths_of_lengths.data(), 19)};
            std::array<std::uint8_t, 320> lengths{};
            for(int index{}; index < literals + distances;)
            {
                const auto symbol{decode(bits, length_code)};
                if(symbol < 16)
                {
                    lengths[static_cast<std::size_t>(index++)] = static_cast<std::uint8_t>(symbol);
                    continue;
                }
                std::uint8_t repeated{};
                std::uint32_t times{};
                if(symbol == 16)
                {
                    if(index == 0)
                    {
                        broken("a length repeated before any");
                    }
                    repeated = lengths[static_cast<std::size_t>(index - 1)];
                    times = 3 + bits.take(2);
                }
                else
                {
                    times = symbol == 17 ? 3 + bits.take(3) : 11 + bits.take(7);
                }
                if(index + static_cast<int>(times) > literals + distances)
                {
                    broken("more lengths than codes");
                }
                for(std::uint32_t count{}; count < times; ++ count)
                {
                    lengths[static_cast<std::size_t>(index++)] = repeated;
                }
            }
            codes(bits, out, huffman(lengths.data(), literals), huffman(lengths.data() + literals, distances));
        }
        else
        {
            broken("a block of no kind");
        }
    }
    // The check after the data: its CRC-32 and its size.
    bits.align();
    const auto end{bits.position()};
    if(end + 8 > compressed.size())
    {
        broken("it ends before its check");
    }
    if(little_endian(compressed, end) != crc32(out) ||
       little_endian(compressed, end + 4) != static_cast<std::uint32_t>(out.size()))
    {
        broken("what it holds does not match its check");
    }
    return out;
}

} // namespace olagarro::oui
