cmake_minimum_required(VERSION 3.20)
project(url-shortener VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

add_executable(url-shortener
    src/main.cpp
)

target_include_directories(url-shortener PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(url-shortener PRIVATE
    Drogon::Drogon
    jwt-cpp::jwt-cpp
    PostgreSQL::PostgreSQL
)
