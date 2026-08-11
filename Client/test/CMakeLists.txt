add_executable(testV
        testV.cpp)

target_link_libraries(testV PRIVATE
        Qt5::Core
        Qt5::Network
        Client)

set(FFMPEG_RUNTIME_DIR "D:/ffmpeg-dev/ffmpeg-8.1.2-full_build-shared/bin" CACHE PATH
        "Path to FFmpeg runtime DLL directory")

file(GLOB FFMPEG_RUNTIME_DLLS "${FFMPEG_RUNTIME_DIR}/*.dll")
foreach(FFMPEG_RUNTIME_DLL ${FFMPEG_RUNTIME_DLLS})
    add_custom_command(TARGET testV POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FFMPEG_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:testV>")
endforeach()
