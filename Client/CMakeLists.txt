add_library(Client
            ClientApp.h ClientApp.cpp
            SignalingClient.h SignalingClient.cpp
            ClientDispatcher.h ClientDispatcher.cpp
            P2pSession.h P2pSession.cpp
            HostRoleService.h HostRoleService.cpp
            GuestRoleService.h GuestRoleService.cpp
            MediaService.h MediaService.cpp)

target_include_directories(Client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})


find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
target_link_libraries(Client PUBLIC
        Qt5::Core
        Qt5::Network
        Config
        Common
        Tcp
        Udp
        Media
        )

add_subdirectory(test)
