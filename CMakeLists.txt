cmake_minimum_required(VERSION 3.18)
project(J2ME_DevSpace VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(GNUInstallDirs)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(yaml-cpp CONFIG QUIET)
if(NOT yaml-cpp_FOUND)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
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

set(J2ME_CLI_VERSION "0.1b")
set(J2ME_PACKAGE_VERSION "0.1.0")
set(J2ME_INSTALL_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/j2me-devspace")

add_executable(j2me src/main.cpp)
target_compile_definitions(j2me PRIVATE
    J2ME_CLI_VERSION="${J2ME_CLI_VERSION}"
    J2ME_INSTALL_DATADIR="${J2ME_INSTALL_DATADIR}"
)
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

install(TARGETS j2me RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES j2me.devspace.yml LICENSE README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/j2me-devspace")
install(DIRECTORY templates DESTINATION "${CMAKE_INSTALL_DATADIR}/j2me-devspace")
install(DIRECTORY completions DESTINATION "${CMAKE_INSTALL_DATADIR}/j2me-devspace")
install(FILES completions/bash/j2me DESTINATION "${CMAKE_INSTALL_DATADIR}/bash-completion/completions")
install(FILES completions/zsh/_j2me DESTINATION "${CMAKE_INSTALL_DATADIR}/zsh/site-functions")
install(FILES completions/fish/j2me.fish DESTINATION "${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d")

set(CPACK_PACKAGE_NAME "j2me-devspace")
set(CPACK_PACKAGE_VENDOR "IlyaBOT")
set(CPACK_PACKAGE_CONTACT "IlyaBOT")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Portable CLI workspace manager for Java ME / J2ME MIDP development")
set(CPACK_PACKAGE_VERSION "${J2ME_PACKAGE_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "IlyaBOT")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
include(CPack)
