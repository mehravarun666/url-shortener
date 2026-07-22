cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use a build/ directory.")
endif()

project(url-shortener
    VERSION 0.1.0
    DESCRIPTION "Production-style URL shortener service"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

find_package(Drogon CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

add_executable(url-shortener
    src/main.cpp
    src/HealthController.cpp
    controllers/UrlController.cpp
    models/Urls.cc
    utils/ShortCodeGenerator.cpp
)

target_include_directories(url-shortener PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_compile_options(url-shortener PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# Generated Drogon models still use std::codecvt (removed from the C++17+ default libc++ API).
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    target_compile_definitions(url-shortener PRIVATE
        _LIBCPP_ENABLE_CXX17_REMOVED_CODECVT
        _LIBCPP_ENABLE_CXX26_REMOVED_CODECVT_HEADER
        _LIBCPP_ENABLE_CXX26_REMOVED_WSTRING_CONVERT
    )
    set_source_files_properties(models/Urls.cc PROPERTIES
        COMPILE_OPTIONS "-include;codecvt;-include;locale"
    )
endif()

target_link_libraries(url-shortener PRIVATE
    Drogon::Drogon
    jwt-cpp::jwt-cpp
    PostgreSQL::PostgreSQL
)
