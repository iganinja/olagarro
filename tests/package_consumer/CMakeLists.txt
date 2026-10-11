cmake_minimum_required(VERSION 3.20)

project(OlagarroPackageConsumer LANGUAGES CXX)

find_package(Olagarro CONFIG REQUIRED COMPONENTS ByteStream SignalSlot Tasks CTTasks Concurrency OUI OUIScript)

add_executable(olagarro_package_consumer main.cpp)
target_link_libraries(olagarro_package_consumer PRIVATE olagarro::ByteStream olagarro::SignalSlot olagarro::Tasks
    olagarro::Concurrency olagarro::OUI olagarro::OUIScript)
