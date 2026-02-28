cmake_minimum_required(VERSION 3.20)
project(ebilanz-submit VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(WITH_ERIC "Build with ERiC-SDK support" ON)
option(WITH_ERIC_TESTS "Enable ERiC-SDK integration tests" ON)

# Dependencies
# xlnt is not available via Homebrew — fetch from source
include(FetchContent)
FetchContent_Declare(
    xlnt
    GIT_REPOSITORY https://github.com/tfussell/xlnt.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(XLNT_STATIC ON CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(xlnt)

find_package(pugixml REQUIRED)
find_package(CLI11 REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# Source files (without main.cpp for library target)
set(LIB_SOURCES
    src/logger.cpp
    src/utils.cpp
    src/steuernummer.cpp
    src/taxonomie_config.cpp
    src/excel_parser.cpp
    src/validator.cpp
    src/xbrl_generator.cpp
    src/elster_wrapper.cpp
    src/eric_connector.cpp
)

# Library target (for tests and main)
add_library(ebilanz_lib STATIC ${LIB_SOURCES})
target_include_directories(ebilanz_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ebilanz_lib PUBLIC
    xlnt
    pugixml::pugixml
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

if(WITH_ERIC)
    target_compile_definitions(ebilanz_lib PUBLIC WITH_ERIC)
    target_include_directories(ebilanz_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/eric-sdk/include)

    set(ERIC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/eric-sdk/lib)

    if(APPLE)
        # Use full paths to ERiC libraries
        set(ERIC_LIBRARIES
            ${ERIC_LIB_DIR}/libericapi.dylib
            ${ERIC_LIB_DIR}/libericxerces.dylib
            ${ERIC_LIB_DIR}/libeSigner.dylib
            ${ERIC_LIB_DIR}/libotto.dylib
        )
        target_link_libraries(ebilanz_lib PUBLIC ${ERIC_LIBRARIES})

        # Set rpath so dylibs can find each other at runtime
        set_target_properties(ebilanz_lib PROPERTIES
            BUILD_RPATH "${ERIC_LIB_DIR}"
            INSTALL_RPATH "${ERIC_LIB_DIR}"
        )
    elseif(WIN32)
        target_link_directories(ebilanz_lib PUBLIC ${ERIC_LIB_DIR})
        target_link_libraries(ebilanz_lib PUBLIC ericapi)
    endif()

    # Copy plugins directory to build dir (required by ERiC at runtime)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/external/eric-sdk/lib/plugins
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/lib)
endif()

# Main executable
add_executable(ebilanz-submit src/main.cpp)
target_link_libraries(ebilanz-submit PRIVATE
    ebilanz_lib
    CLI11::CLI11
)

# Set rpath for executable to find ERiC libraries
if(WITH_ERIC AND APPLE)
    set_target_properties(ebilanz-submit PROPERTIES
        BUILD_RPATH "${ERIC_LIB_DIR}"
        INSTALL_RPATH "${ERIC_LIB_DIR}"
    )
endif()

# Tests
find_package(Catch2 3 REQUIRED)
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
include(Catch)

add_executable(ebilanz_tests
    tests/test_steuernummer.cpp
    tests/test_validator.cpp
    tests/test_xbrl_generator.cpp
    tests/test_elster_wrapper.cpp
    tests/test_excel_parser.cpp
    tests/test_eric.cpp
)
target_link_libraries(ebilanz_tests PRIVATE
    ebilanz_lib
    Catch2::Catch2WithMain
)

if(WITH_ERIC_TESTS)
    target_compile_definitions(ebilanz_tests PRIVATE WITH_ERIC_TESTS)
endif()

# Set rpath for tests executable to find ERiC libraries
if(WITH_ERIC AND APPLE)
    set_target_properties(ebilanz_tests PROPERTIES
        BUILD_RPATH "${ERIC_LIB_DIR}"
        INSTALL_RPATH "${ERIC_LIB_DIR}"
    )
endif()

catch_discover_tests(ebilanz_tests
    TEST_PREFIX ""
    PROPERTIES TIMEOUT 60  # ERiC SDK tests can be slow
)

# Test data generator
add_executable(create_test_data tests/create_test_data.cpp)
target_link_libraries(create_test_data PRIVATE ebilanz_lib)

# Copy config files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
