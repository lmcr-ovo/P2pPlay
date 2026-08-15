add_library(Udp
        UdpPacket.h
        P2pUdpTransport.h P2pUdpTransport.cpp
        UdpFrameReassembler.h UdpFrameReassembler.cpp
        UdpPacketCodec.h UdpPacketCodec.cpp
        UdpFragmenter.h UdpFragmenter.cpp
        UdpPacketQueue.h UdpPacketQueue.cpp
        UdpSocketUtil.h UdpSocketUtil.cpp)

target_include_directories(Udp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Udp PUBLIC Qt5::Core Qt5::Network Common)
if(WIN32)
    # UdpSocketUtil.h 里用了 WSAIoctl，它位于 ws2_32
    target_link_libraries(Udp PUBLIC ws2_32)
endif()
add_subdirectory(test)
