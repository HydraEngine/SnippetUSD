cmake_minimum_required(VERSION 3.29)
project(SnippetUSD)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 COMPONENTS
        Core Widgets Test
        REQUIRED)

# Add path for local cmake scripts
list(APPEND CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/
        ${CMAKE_CURRENT_SOURCE_DIR}/../USDInstall)

# usd
include(pxrConfig)
include_directories(${PXR_INCLUDE_DIRS})

# Include USD plugin building CMake utilities.
include(USDPluginTools)

set(PXR_LIBRARY_NAMES)
foreach (_item ${PXR_LIBRARIES}) # Iterate over list of absolute paths
    get_filename_component(_t ${_item} NAME)
    list(APPEND PXR_LIBRARY_NAMES ${_t})
endforeach ()

add_subdirectory(pxr)