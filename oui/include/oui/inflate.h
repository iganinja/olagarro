#pragma once

#include <string>
#include <string_view>

namespace olagarro::oui
{

// What a gzip file holds, as it was before it was compressed: deflate, read here so the files programs carry inside
// them, compressed as they are built, need nothing else. One that is not whole, or not gzip, throws
// std::runtime_error saying so; its check, a CRC-32 and its size, are compared too.
[[nodiscard]] std::string inflate_gzip(std::string_view compressed);

} // namespace olagarro::oui
