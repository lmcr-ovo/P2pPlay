add_executable(testUdp
        testUdp.cpp)
target_link_libraries(testUdp PRIVATE Qt5::Core Qt5::Network Udp)

add_executable(testUdp1
        testUdp1.cpp)
target_link_libraries(testUdp1 PRIVATE Qt5::Core Qt5::Network Udp)
