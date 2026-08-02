add_executable(testP2pServer
        testP2pServer.cpp)

target_link_libraries(testP2pServer PRIVATE
        Qt5::Core
        Qt5::Network
        Server)
