set(FFMPEG_ROOT "D:/ffmpeg-dev/ffmpeg-8.1.2-full_build-shared" CACHE PATH
        "Path to an FFmpeg shared/dev build that contains include and lib directories")

set(FFMPEG_DEFAULT_ROOT "D:/ffmpeg-dev/ffmpeg-8.1.2-full_build-shared")
if(NOT EXISTS "${FFMPEG_ROOT}/include/libavcodec/avcodec.h"
        AND EXISTS "${FFMPEG_DEFAULT_ROOT}/include/libavcodec/avcodec.h")
    set(FFMPEG_ROOT "${FFMPEG_DEFAULT_ROOT}" CACHE PATH
            "Path to an FFmpeg shared/dev build that contains include and lib directories"
            FORCE)
endif()

if(NOT FFMPEG_ROOT)
    find_program(FFMPEG_EXECUTABLE ffmpeg)
    if(FFMPEG_EXECUTABLE)
        get_filename_component(FFMPEG_BIN_DIR "${FFMPEG_EXECUTABLE}" DIRECTORY)
        get_filename_component(FFMPEG_ROOT "${FFMPEG_BIN_DIR}" DIRECTORY)
    endif()
endif()

find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h libswscale/swscale.h
        PATHS "${FFMPEG_ROOT}/include"
        NO_DEFAULT_PATH)

find_library(FFMPEG_AVCODEC_LIBRARY
        NAMES libavcodec.dll.a avcodec
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH)

find_library(FFMPEG_SWSCALE_LIBRARY
        NAMES libswscale.dll.a swscale
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH)

find_library(FFMPEG_AVUTIL_LIBRARY
        NAMES libavutil.dll.a avutil
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH)

if(NOT FFMPEG_INCLUDE_DIR
        OR NOT FFMPEG_AVCODEC_LIBRARY
        OR NOT FFMPEG_SWSCALE_LIBRARY
        OR NOT FFMPEG_AVUTIL_LIBRARY)
    message(FATAL_ERROR
            "FFmpeg development files were not found. "
            "Set FFMPEG_ROOT to a shared/dev FFmpeg build directory containing include/ and lib/.")
endif()



#----------------------------------------------------------------------------------------
add_library(Media
        RateController/RateController.h RateController/RateController.cpp
        RateController/ReceiverReportSample.h RateController/ReceiverReportSample.cpp
        RateController/ReceiverMonitor.h RateController/ReceiverMonitor.cpp
        RateController/ReceiverMonitorWorker.h RateController/ReceiverMonitorWorker.cpp

        VideoWidget.h VideoWidget.cpp

        Video/VideoSample.h
        Video/VideoSampleCodec.h Video/VideoSampleCodec.cpp
        Video/ScreenVideoSource.h Video/ScreenVideoSource.cpp
        Video/VideoEncoder.h Video/VideoEncoder.cpp
        Video/VideoSenderPipeline.h Video/VideoSenderPipeline.cpp
        Video/VideoDecoder.h Video/VideoDecoder.cpp
        Video/VideoDecoderWorker.h Video/VideoDecoderWorker.cpp
        Video/VideoReceiverPipline.h Video/VideoReceiverPipline.cpp

        #-----------------------------按键----------------------------------
        Input/InputSample.h
        Input/InputSampleCodec.h Input/InputSampleCodec.cpp
        Input/InputCapture.h Input/InputCapture.cpp
        Input/InputCaptureWorker.h Input/InputCaptureWorker.cpp
        Input/InputSender.h Input/InputSender.cpp
        Input/InputSenderWorker.h Input/InputSenderWorker.cpp
        Input/InputReceiver.h Input/InputReceiver.cpp
        Input/InputReceiverWorker.h Input/InputReceiverWorker.cpp
        Input/InputInjector.h Input/InputInjector.cpp
        )


target_include_directories(Media PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${FFMPEG_INCLUDE_DIR}")

find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Media PUBLIC
        Qt5::Core
        Qt5::Network
        Qt5::Gui
        Qt5::Widgets
        Tcp
        Config
        Logger
        "${FFMPEG_AVCODEC_LIBRARY}"
        "${FFMPEG_SWSCALE_LIBRARY}"
        "${FFMPEG_AVUTIL_LIBRARY}")
