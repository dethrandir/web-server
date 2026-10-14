cmake_minimum_required(VERSION 3.16)

project(web-server
    VERSION 0.1.0
    DESCRIPTION "Web server"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Core library: every source except the application entry point.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(web_server_core STATIC ${CORE_SOURCES})

target_compile_definitions(web_server_core PUBLIC
    WEB_SERVER_VERSION="${PROJECT_VERSION}"
)

target_include_directories(web_server_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/cxxopts"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/tomlplusplus"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/spdlog"
)

if(MSVC)
    target_compile_options(web_server_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(web_server_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Application.
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE web_server_core)

# Tests.
enable_testing()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
)

add_executable(web_server_tests
    ${TEST_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/catch2/catch_amalgamated.cpp"
)
target_link_libraries(web_server_tests PRIVATE web_server_core)
target_include_directories(web_server_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/catch2"
)

add_test(NAME web_server_tests COMMAND web_server_tests)

# Benchmarks (built but not run by ctest; run manually).
option(WEB_SERVER_BUILD_BENCH "Build the benchmark harness" ON)

if(WEB_SERVER_BUILD_BENCH)
    file(GLOB BENCH_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp"
    )

    add_executable(web_server_bench ${BENCH_SOURCES})
    target_link_libraries(web_server_bench PRIVATE web_server_core)
    target_include_directories(web_server_bench PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/bench"
    )

    if(NOT MSVC)
        target_compile_options(web_server_bench PRIVATE -O2)
    endif()
endif()
