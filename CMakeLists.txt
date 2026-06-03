cmake_minimum_required(VERSION 3.18)
project(J2ME_DevSpace VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(yaml-cpp CONFIG QUIET)
if(NOT yaml-cpp_FOUND)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

add_executable(j2me src/main.cpp)
target_compile_definitions(j2me PRIVATE J2ME_CLI_VERSION="0.1b")
if(TARGET yaml-cpp::yaml-cpp)
    set(J2ME_YAML_CPP_TARGET yaml-cpp::yaml-cpp)
else()
    set(J2ME_YAML_CPP_TARGET yaml-cpp)
endif()

target_link_libraries(j2me PRIVATE nlohmann_json::nlohmann_json ${J2ME_YAML_CPP_TARGET})

# Keep the CLI binary in a predictable repo-local folder.
# Multi-config generators may still append Debug/Release subdirs.
set_target_properties(j2me PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
