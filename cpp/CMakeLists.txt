cmake_minimum_required(VERSION 3.19)
project(picpicker VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
# libraw_r is the thread-safe build; we decode on several worker threads.
pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw_r)

add_executable(picpicker
    src/main.cpp
    src/MainWindow.cpp
    src/ShotModel.cpp
    src/ImageLoader.cpp
    src/ThumbDelegate.cpp
    src/ViewerWidget.cpp
    src/ApplyDialog.cpp
    resources.qrc
)

target_link_libraries(picpicker PRIVATE Qt6::Widgets PkgConfig::LIBRAW)
install(TARGETS picpicker RUNTIME DESTINATION bin)
