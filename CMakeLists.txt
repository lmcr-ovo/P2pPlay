cmake_minimum_required(VERSION 3.15)
project(P2Pplay)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(P2Pplay
        Common/TcpFrame.h
        Common/TcpFrameCodec.h Common/TcpFrameCodec.cpp
        Common/SignalingMessage.h
        Common/SignalingCodec.h Common/SignalingCodec.cpp
        Common/SignalingConnection.h Common/SignalingConnection.cpp

        Udp/UdpPacket.h
        Udp/P2pUdpTransport.h Udp/P2pUdpTransport.cpp
        Udp/UdpFrameReassembler.h Udp/UdpFrameReassembler.cpp
        Udp/UdpPacketCodec.h Udp/UdpPacketCodec.cpp
        Udp/UdpFragmenter.h Udp/UdpFragmenter.cpp)

set(CMAKE_PREFIX_PATH "D:/Qt5/5.14.0/mingw73_64")
find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(P2Pplay PRIVATE Qt5::Core Qt5::Network)
