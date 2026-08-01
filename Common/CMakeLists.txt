add_library(Common
        SignalingMessage.h
        SignalingCodec.h SignalingCodec.cpp
        SignalingConnection.h SignalingConnection.cpp

        UdpControlPayload.h
        UdpControlPayloadCodec.h UdpControlPayloadCodec.cpp
        )

target_include_directories(Common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Common PUBLIC
        Qt5::Core
        Qt5::Network
        Tcp)

add_subdirectory(test)
