add_library(Common
        TcpFrame.h
        TcpFrameCodec.h TcpFrameCodec.cpp
        SignalingMessage.h
        SignalingCodec.h SignalingCodec.cpp
        SignalingConnection.h SignalingConnection.cpp
        )

find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Common PRIVATE Qt5::Core Qt5::Network)
