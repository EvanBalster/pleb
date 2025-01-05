cmake_minimum_required(VERSION 3.18)

include(CMakePackageConfigHelpers)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_language(CXX)
enable_language(C)

find_package (Threads)


# Library project
project(pleb)

file(GLOB_RECURSE pleb.headers include/pleb/*.hpp include/pleb/*.h)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/pleb PREFIX pleb FILES ${pleb.headers})

add_library(pleb INTERFACE)

target_sources(pleb
    PUBLIC FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES ${pleb.headers})

configure_package_config_file(
    pleb-config.cmake.in
    pleb-config.cmake
    INSTALL_DESTINATION lib/cmake/pleb)
install(TARGETS pleb EXPORT pleb-targets FILE_SET HEADERS)
install(             EXPORT pleb-targets                    DESTINATION lib/cmake/pleb)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pleb-config.cmake DESTINATION lib/cmake/pleb)

#add_library(pleb STATIC ${TELLING_HEADERS})
#target_include_directories(pleb PUBLIC "include")


# TEST project
project(pleb_test)

# add the executable
file(GLOB pleb-test.sources test/*.*)
add_executable(pleb_test ${pleb.headers} ${pleb-test.sources})

target_include_directories(pleb_test PUBLIC "include")


# Solution name
project(pleb)



# Installation
#install(TARGETS pleb DESTINATION lib)
install(FILES ${TELLING_HEADERS} DESTINATION include)