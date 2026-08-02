add_library(Server
        Room.h
        RoomService.h RoomService.cpp
        ServerApp.h ServerApp.cpp
        NatProbeService.h NatProbeService.cpp
        SignalingServer.h SignalingServer.cpp
        SignalingDispatcher.h SignalingDispatcher.cpp)

target_include_directories(Server PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Server PUBLIC
        Qt5::Core
        Qt5::Network
        Common
        Tcp
        Udp)

add_subdirectory(test)
