add_library(Logger
        Logger.h Logger.cpp
        TraceEvent.h
        TraceManager.h TraceManager.cpp)

target_include_directories(Logger PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Logger PUBLIC
        Qt5::Core)
