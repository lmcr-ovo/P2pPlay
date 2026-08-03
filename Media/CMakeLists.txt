add_library(Media
        VideoCapturer.h VideoCapturer.cpp
        VideoRender.h VideoRender.cpp)

target_include_directories(Media PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Media PUBLIC
        Qt5::Core
        Qt5::Network
        Tcp)
