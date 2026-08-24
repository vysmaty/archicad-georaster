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

# MDID values identify the Add-On embedded in its binary. They are intentionally
# read from the build environment so the checked-in resource files never contain
# the Graphisoft-issued values. Forks and local developer builds have no secrets
# and therefore use the reserved development identity 1/1.
set(GEORASTER_DEVELOPER_ID "$ENV{GRAPHISOFT_DEVELOPER_ID}")
set(GEORASTER_LOCAL_ID "$ENV{GRAPHISOFT_LOCAL_ID}")

if(GEORASTER_DEVELOPER_ID STREQUAL "" AND GEORASTER_LOCAL_ID STREQUAL "")
    set(GEORASTER_DEVELOPER_ID 1)
    set(GEORASTER_LOCAL_ID 1)
elseif(GEORASTER_DEVELOPER_ID STREQUAL "" OR GEORASTER_LOCAL_ID STREQUAL "")
    message(FATAL_ERROR "GRAPHISOFT_DEVELOPER_ID and GRAPHISOFT_LOCAL_ID must either both be set or both be empty.")
endif()

foreach(GEORASTER_MDID_VALUE IN ITEMS "${GEORASTER_DEVELOPER_ID}" "${GEORASTER_LOCAL_ID}")
    if(NOT GEORASTER_MDID_VALUE MATCHES "^[0-9]+$")
        message(FATAL_ERROR "GeoRaster MDID values must be decimal integers.")
    endif()
endforeach()

# The Graphisoft resource compiler accepts one resource root. Copy the ordinary
# resources to a build-only root and configure the MDID placeholders there.
set(GEORASTER_GENERATED_RESOURCES_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/GeneratedResources")
file(COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/RFIX.win"
    DESTINATION "${GEORASTER_GENERATED_RESOURCES_FOLDER}"
)
foreach(GEORASTER_LANGUAGE IN ITEMS CZE INT)
    file(MAKE_DIRECTORY "${GEORASTER_GENERATED_RESOURCES_FOLDER}/R${GEORASTER_LANGUAGE}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/R${GEORASTER_LANGUAGE}/AddOn.grc"
        "${GEORASTER_GENERATED_RESOURCES_FOLDER}/R${GEORASTER_LANGUAGE}/AddOn.grc"
        @ONLY
    )
endforeach()
file(
    RELATIVE_PATH
    GEORASTER_GENERATED_RESOURCES_FOLDER_RELATIVE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GEORASTER_GENERATED_RESOURCES_FOLDER}"
)

set(AddOnSourcesFolder src)
set(AddOnResourcesFolder "${GEORASTER_GENERATED_RESOURCES_FOLDER_RELATIVE}")
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
