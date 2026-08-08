add_library(Media
        VideoSample.h
        VideoSampleCodec.h VideoSampleCodec.cpp
        ScreenVideoSource.h ScreenVideoSource.cpp
        VideoWidget.h VideoWidget.cpp
        VideoEncoder.h VideoEncoder.cpp
        VideoSenderPipeline.h VideoSenderPipeline.cpp
        VideoDecoder.h VideoDecoder.cpp
        VideoReceiverPipline.h VideoReceiverPipline.cpp
        )

target_include_directories(Media PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Media PUBLIC
        Qt5::Core
        Qt5::Network
        Qt5::Gui
        Qt5::Widgets
        Tcp
        Config
        Logger)
