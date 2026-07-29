add_executable(testTcpSignaling
        testTcpSignaling.cpp)

target_link_libraries(testTcpSignaling PRIVATE
        Qt5::Core
        Qt5::Network
        Common)

