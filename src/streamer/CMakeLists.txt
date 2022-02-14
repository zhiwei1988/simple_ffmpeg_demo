add_executable(streamer ./main.cpp)

target_include_directories(streamer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../FFmpeg/include)