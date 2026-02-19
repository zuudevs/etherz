cmake_minimum_required(VERSION 3.20)
project(
	etherz
	VERSION 1.0.0
	DESCRIPTION "A modern C++23 header-only networking library"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# ─── Options ────────────────────
option(ETHERZ_BUILD_TESTS    "Build unit tests"      OFF)
option(ETHERZ_BUILD_EXAMPLES "Build example programs" OFF)

# ─── Compiler Flags ─────────────
if(MSVC)
    add_compile_options(
        /W4 
        /permissive-
        /utf-8
    )
else()
    add_compile_options(
        -Wall 
        -Wextra 
        -Wpedantic 
        -Wconversion 
        -Wshadow
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2)
    else()
        add_compile_options(-O3)
    endif()
endif()

# ─── Main Demo ──────────────────
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_SOURCE_DIR}/include"
)

# ─── Link libraries (Windows) ───
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 secur32 iphlpapi)
endif()

# ─── Tests ──────────────────────
if(ETHERZ_BUILD_TESTS)
    add_executable(etherz_tests
        tests/test_main.cpp
        tests/test_ip.cpp
        tests/test_subnet.cpp
        tests/test_url.cpp
        tests/test_http.cpp
        tests/test_websocket.cpp
        tests/test_certificate.cpp
    )
    target_include_directories(etherz_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/tests"
    )
    if(WIN32)
        target_link_libraries(etherz_tests PRIVATE ws2_32 secur32 iphlpapi)
    endif()
endif()

# ─── Examples ───────────────────
if(ETHERZ_BUILD_EXAMPLES)
    set(ETHERZ_EXAMPLES
        echo_server
        dns_lookup
        ping_tool
        subnet_calc
    )
    foreach(example ${ETHERZ_EXAMPLES})
        add_executable(${example} examples/${example}.cpp)
        target_include_directories(${example} PRIVATE
            "${CMAKE_SOURCE_DIR}/include"
        )
        if(WIN32)
            target_link_libraries(${example} PRIVATE ws2_32 secur32 iphlpapi)
        endif()
    endforeach()
endif()

# ─── Install ────────────────────
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

# ─── Print build info ───────────
message(STATUS "")
message(STATUS "═══════════════════════════════════")
message(STATUS "  Project: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${ETHERZ_BUILD_TESTS}")
message(STATUS "  Examples: ${ETHERZ_BUILD_EXAMPLES}")
message(STATUS "═══════════════════════════════════")
message(STATUS "")