cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_language(CXX)
enable_language(C)

find_package (Threads)


# Library project
project(pleb)

file(GLOB PLEB_HEADERS include/pleb/*.h)

#add_library(pleb STATIC ${TELLING_HEADERS})
#target_include_directories(pleb PUBLIC "include")


# TEST project
project(pleb_test)

# add the executable
file(GLOB TEST_SOURCES test/*.*)
add_executable(pleb_test ${PLEB_HEADERS} ${TEST_SOURCES})

target_include_directories(pleb_test PUBLIC "include")


# Solution name
project(pleb)



# Installation
#install(TARGETS pleb DESTINATION lib)
install(FILES ${TELLING_HEADERS} DESTINATION include)