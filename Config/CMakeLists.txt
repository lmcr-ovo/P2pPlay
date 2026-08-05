add_library(Config
            AppConfig.h AppConfig.cpp
        )

target_include_directories(Config PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Config PUBLIC
        Qt5::Core
        Qt5::Network
        Media)
