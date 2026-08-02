add_executable(testP2pClient
        testP2pClient.cpp)

target_link_libraries(testP2pClient PRIVATE
        Qt5::Core
        Qt5::Network
        Client)
