project(test_mic)

find_package(Qt5 REQUIRED COMPONENTS Core Multimedia)

set(WINSDK_10_22621 "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0")
set(WINSDK_10_26100 "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0")
set(MSVC_INCLUDE_ROOT "D:/visual_studio/vs/VC/Tools/MSVC/14.44.35207/include")

if(EXISTS "${WINSDK_10_22621}/um/audioclientactivationparams.h")
    set(WINSDK_ROOT "${WINSDK_10_22621}")
elseif(EXISTS "${WINSDK_10_26100}/um/audioclientactivationparams.h")
    set(WINSDK_ROOT "${WINSDK_10_26100}")
else()
    message(FATAL_ERROR "Windows SDK header audioclientactivationparams.h not found")
endif()

if(NOT EXISTS "${MSVC_INCLUDE_ROOT}/vcruntime.h")
    message(FATAL_ERROR "MSVC vcruntime.h not found")
endif()

add_executable(test_mic test_microphone.cpp)
add_executable(test_desktop test_desktop_loopback.cpp)
target_link_libraries(test_mic Qt5::Core Qt5::Multimedia)
target_include_directories(test_desktop PRIVATE
        "${WINSDK_ROOT}/um"
        "${WINSDK_ROOT}/shared"
        "${WINSDK_ROOT}/ucrt"
        "${MSVC_INCLUDE_ROOT}")
target_link_libraries(test_desktop Qt5::Core Qt5::Multimedia ole32 uuid avrt)
