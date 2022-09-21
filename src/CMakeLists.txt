aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ SRC)
add_executable(av_demo ${SRC})

target_include_directories(av_demo PRIVATE /usr/local/ffmpeg-5.0/include)
target_link_directories(av_demo PRIVATE /usr/local/ffmpeg-5.0/include/lib)

target_link_libraries(av_demo avformat avcodec avutil)

# 设置可执行文件及动态库的输出路径
set_target_properties(av_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../output)
