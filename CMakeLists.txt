cmake_minimum_required(VERSION 3.15)
project(P2Pplay)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_PREFIX_PATH "D:/Qt5/5.14.0/mingw73_64")
find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network)
add_subdirectory(Common)
add_subdirectory(Tcp)
add_subdirectory(Udp)
add_subdirectory(Server)
add_subdirectory(Client)
add_subdirectory(Media)
add_subdirectory(Config)
add_subdirectory(Logger)