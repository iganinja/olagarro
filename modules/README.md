# Experimental module adapters

This directory provides optional C++20 module adapters for Olagarro's C++17
headers. The library itself remains C++17; adapters are never enabled by the
default build.

Enable them with a module-aware CMake and Ninja toolchain:

```sh
cmake -S . -B build -G Ninja -DOLAGARRO_BUILD_MODULES=ON
cmake --build build
```

The root adapter re-exports every public Olagarro component:

```cpp
import olagarro;
```

Individual adapters can be imported independently:

```cpp
import olagarro.bytestream;
import olagarro.signalslot;
import olagarro.tasks;
import olagarro.cttasks;
import olagarro.concurrency;
import olagarro.oui;
```

When consuming an installed package with CMake, link the root adapter target:

```cmake
find_package(Olagarro CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE olagarro::Module)
```

The individual CMake targets are `olagarro::ModuleByteStream`,
`olagarro::ModuleSignalSlot`, `olagarro::ModuleTasks`,
`olagarro::ModuleCTTasks`, `olagarro::ModuleConcurrency`, and
`olagarro::ModuleOUI`.

BMI files are compiler- and configuration-specific. Build them through the
exported CMake target instead of attempting to distribute prebuilt BMI files.
