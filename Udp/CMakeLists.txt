add_library(Udp
        UdpPacket.h
        P2pUdpTransport.h P2pUdpTransport.cpp
        UdpFrameReassembler.h UdpFrameReassembler.cpp
        UdpPacketCodec.h UdpPacketCodec.cpp
        UdpFragmenter.h UdpFragmenter.cpp
        UdpPacketQueue.h UdpPacketQueue.cpp)

target_include_directories(Udp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Udp PUBLIC Qt5::Core Qt5::Network Common)
add_subdirectory(test)
