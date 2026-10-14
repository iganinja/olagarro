# Licences of what the released programs carry

Programs made with OUI carry these, and their release packages a copy beside the programs, as OUI Code's and OUI Paint's
do in `THIRD_PARTY_NOTICES.txt`, made from the files each kind of package needs. In the source, each third-party piece
keeps its licence beside it: QuickJS-ng in `third_party/quickjs-ng/LICENSE`, stb_truetype at the end of
`third_party/stb/stb_truetype.h`, NanoSVG in `third_party/nanosvg/LICENSE.txt`, Spleen in `oui/fonts/spleen_license.txt`
and Jersey 10 in `oui/fonts/jersey10_ofl.txt`. The files here are the ones the source does not hold:

| File | What it covers | Where it is from |
|------|----------------|------------------|
| `stb_truetype.txt` | stb_truetype, which reads the interface font | the end of `third_party/stb/stb_truetype.h` |
| `SDL3.txt` | SDL3, built into the `gui` programs | SDL 3.4.16's `LICENSE.txt`, the version `CMakeLists.txt` fetches |
| `musl.txt` | musl, the C library built into the Linux terminal programs | musl's `COPYRIGHT` |
| `mingw-w64-runtime.txt` | the mingw-w64 runtime, built into the Windows programs | mingw-w64's `COPYING.MinGW-w64-runtime.txt` |
| `winpthreads.txt` | winpthreads, built into the Windows programs | mingw-w64's `mingw-w64-libraries/winpthreads/COPYING` |

LLVM's libc++, compiler-rt and libunwind, built into the Linux terminal and the Windows programs, are under the
Apache License 2.0 with LLVM Exceptions, whose exception lets binaries carry them without a notice. The Linux `gui`
programs carry GCC's C++ runtime under the GCC Runtime Library Exception, which asks for none either.
