add_executable(testQtTimerAccuracy
        testQtTimerAccuracy.cpp)

target_link_libraries(testQtTimerAccuracy PRIVATE
        Qt5::Core)
