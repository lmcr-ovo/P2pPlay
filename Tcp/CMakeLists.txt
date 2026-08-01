add_library(Tcp
        TcpFrame.h
        TcpFrameCodec.h TcpFrameCodec.cpp
        )

target_include_directories(Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Core Network)
target_link_libraries(Tcp PUBLIC
        Qt5::Core
        Qt5::Network)
