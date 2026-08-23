cmake_minimum_required(VERSION 3.19)

# Official Graphisoft tooling is intentionally provided as a pinned Git submodule.
include(external/archicad-addon-cmake-tools/CMakeCommon.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(AC_API_DEVKIT_DIR "$ENV{AC_API_DEVKIT_DIR}" CACHE PATH "Archicad API DevKit Support directory")

if(NOT DEFINED AC_VERSION)
    message(FATAL_ERROR "AC_VERSION is not set. Use a named CMake preset or tools/build.ps1.")
endif()

set(SUPPORTED_AC_VERSIONS 27 28 29)
if(NOT AC_VERSION IN_LIST SUPPORTED_AC_VERSIONS)
    message(FATAL_ERROR "Archicad ${AC_VERSION} is unsupported. Supported versions: ${SUPPORTED_AC_VERSIONS}.")
endif()

message(STATUS "Archicad Version: ${AC_VERSION}")
SetGlobalCompilerDefinitions(${AC_VERSION})

ReadConfigJson()
set(AC_ADDON_LANGUAGE "${addOnDefaultLanguage}" CACHE STRING "Add-On language code")
project(${addOnName}-${AC_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(georaster_core STATIC
    core/GeoRasterCore.cpp
    core/RasterReader.cpp
)
target_include_directories(georaster_core PUBLIC .)
target_compile_features(georaster_core PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(georaster_core PRIVATE /EHsc)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(georaster_core_tests tests/cpp/GeoRasterCoreTests.cpp)
    target_link_libraries(georaster_core_tests PRIVATE georaster_core)
    if(MSVC)
        target_compile_options(georaster_core_tests PRIVATE /EHsc)
    endif()
    add_test(NAME georaster_core_tests COMMAND georaster_core_tests)
endif()

# Local builds are private by default. A release pipeline must explicitly enable this.
set(AC_ADDON_FOR_DISTRIBUTION OFF CACHE BOOL "Mark the Add-On for distribution")

set(AddOnSourcesFolder src)
set(AddOnResourcesFolder .)
set(PCH_HEADER "src/GeoRasterPrecompiledHeader.hpp")

# The Graphisoft resource compiler requires a non-empty positional path even
# when this Add-On has no library parts. Callers can override it for ACLib use.
set(
    LP_XML_CONVERTER_FOLDER
    "${AC_API_DEVKIT_DIR}/LP_XMLConverter"
    CACHE PATH
    "Optional Graphisoft LP_XMLConverter directory"
)

GenerateAddOnProject(
    GeoRasterAddOn
    ${AC_VERSION}
    "${AC_API_DEVKIT_DIR}"
    ${AddOnSourcesFolder}
    ${AddOnResourcesFolder}
    ${AC_ADDON_LANGUAGE}
    ${addOnDefaultLanguage}
    ${PCH_HEADER}
)

target_link_libraries(GeoRasterAddOn georaster_core)
