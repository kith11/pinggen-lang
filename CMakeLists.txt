cmake_minimum_required(VERSION 3.20)
project(pinggen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pinggen
    src/main.cpp
    src/token.cpp
    src/diagnostics.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/llvm_ir.cpp
    src/project.cpp
)

target_include_directories(pinggen PRIVATE include)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME pinggen-check-example
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/hello
    )
    add_test(
        NAME pinggen-build-example
        COMMAND pinggen build ${CMAKE_SOURCE_DIR}/examples/hello
    )
endif()
