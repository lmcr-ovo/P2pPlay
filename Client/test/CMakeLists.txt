add_executable(testV
        testV.cpp)

target_link_libraries(testV PRIVATE
        Qt5::Core
        Qt5::Network
        Client)