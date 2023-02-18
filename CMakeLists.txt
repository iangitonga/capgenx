cmake_minimum_required(VERSION 3.0 FATAL_ERROR)


project(Capgen)


# Torch library. Could also be set by -DCMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/libtorch/")
find_package(Torch REQUIRED)


# Wxwidgets library
set(wxBUILD_SHARED OFF)
set(wxUSE_AUI 0)
set(wxUSE_PROPGRID 0)
set(wxUSE_RIBBON 0)
set(wxUSE_RICHTEXT 0)
set(wxUSE_WEBVIEW 0)
set(wxUSE_STC 0)
set(wxUSE_XRC 0)
set(wxUSE_MEDIACTRL 0)
set(wxUSE_OPENGL 0)
set(wxUSE_DEBUGREPORT 0)
set(wxUSE_XML 0)
set(wxUSE_PROTOCOL_FTP 0)
add_subdirectory("third_party/wxWidgets")


# Collect all the source files.
file(GLOB_RECURSE SRC_FILES src/*.c src/*.cpp)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")


add_executable(Capgen ${SRC_FILES})

# Capgen include
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src/")

# FFmpeg library.
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/include")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libavformat.a")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libavcodec.a")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libswscale.a")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libavutil.a")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libavfilter.a")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libswresample.a")
target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/build_capgen/lib/libavdevice.a")

# Math and compression libraries required by FFmpeg.
target_link_libraries(${PROJECT_NAME} m)
target_link_libraries(${PROJECT_NAME} lzma)
target_link_libraries(${PROJECT_NAME} z)

# Torch library
target_link_libraries(${PROJECT_NAME} ${TORCH_LIBRARIES})

# Wxwidgets
target_link_libraries(${PROJECT_NAME} wx::core wx::base)


set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)