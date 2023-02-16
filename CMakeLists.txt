cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    # If no deployment target has been set default to the minimum supported
    # OS version (this has to be set before the first project() call)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "iOS Deployment Target")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10 CACHE STRING "macOS Deployment Target")
    endif()
endif()


project(Capgen)


# Could also be set by -DCMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "third_party/libtorch/")
find_package(Torch REQUIRED)

# Request the required wxWidgets libs
# TODO: Specify wxWidgets version.
find_package(wxWidgets COMPONENTS core base REQUIRED)

# Include the wxWidgets use file to initialize various settings
if(wxWidgets_USE_FILE)
    include(${wxWidgets_USE_FILE})
endif()


# Collect all the source files.
file(GLOB_RECURSE SRC_FILES src/*.c src/*.cpp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/)

add_executable(Capgen ${SRC_FILES})

#target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/libs/libav/include/")

#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libavformat.a")
#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libavcodec.a")
#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libswscale.a")
#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libavutil.a")
#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libavfilter.a")
#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libswresample.a")
#target_link_libraries(capgen "${CMAKE_SOURCE_DIR}/libs/libav/lib/libavdevice.a")

# Math and compression libraries.
#target_link_libraries(${PROJECT_NAME} m)
#target_link_libraries(${PROJECT_NAME} z)

# Torch libs
target_link_libraries(${PROJECT_NAME} ${TORCH_LIBRARIES})

# Wxwidgets libs
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)