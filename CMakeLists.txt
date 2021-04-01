# Copyright 2021 The LsmDB Authors. All rights reserved.

cmake_minimum_required(VERSION 3.13.0)
project(lsmdb VERSION 1.0.0 LANGUAGES C CXX)

# This project requires C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
    set(LSMDB_PLATFORM_NAME LSMDB_PLATFORM_WINDOWS)
else (WIN32)
    set(LSMDB_PLATFORM_NAME LSMDB_PLATFORM_POSIX)
endif (WIN32)

option(LSMDB_BUILD_TESTS "Build LsmDB's unit tests" ON)
# option(LSMDB_BUILD_BENCHMARKS "Build LsmDB's benchmarks" ON)
# option(LSMDB_INSTALL "Install LsmDB's header and library" ON)

#
# check_include_file("<includes>" <variable>)
#
# check if the given <includes> list may be included
# together in a source file and store the result in
# an internal cache entry named <variable>.
include(CheckIncludeFile)
check_include_file("unistd.h" HAVE_UNISTD_H)

#
# check_library_exists(LIBRARY FUNCTION LOCATION VARIABLE)
# LIBRARY - the name of the library you are looking for
# FUNCTION - the name of the function
# LOCATION - location where the library should be found
# VARIABLE - variable to store the result
#            will be created as an internal cache variable
#
include(CheckLibraryExists)
check_library_exists(crc32c crc32c_value "" HAVE_CRC32C)
check_library_exists(snappy snappy_compress "" HAVE_SNAPPY)
check_library_exists(tcmalloc malloc "" HAVE_TCMALLOC)

#
# check_cxx_symbols_exists(<symbol> <files> <variable>)
#
# check that the <symbol> is available after including
# given header <files> and store the result in a <variable>
#
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(fdatasync "unistd.h" HAVE_FDATASYNC)
check_cxx_symbol_exists(F_FULLFSYNC "fcntl.h" HAVE_FULLFSYNC)
check_cxx_symbol_exists(O_CLOEXEC "fcntl.h" HAVE_O_CLOEXEC)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Disable C++ exceptions.
    string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs-c-")
    add_definitions(-D_HAS_EXCEPTIONS=0)

    # Disable RTTI.
    string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Enable strict prototype warnings for C code in clang and gcc.
    if(NOT CMAKE_C_FLAGS MATCHES "-Wstrict-prototypes")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")
    endif(NOT CMAKE_C_FLAGS MATCHES "-Wstrict-prototypes")

    # Disable C++ exceptions.
    string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

    # Disable RTTI.
    string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

#
# check_cxx_compiler_flag(<flag> <var>)
#
# check that the <flag> is accepted by the compiler without a diagnostic.
# Stores the result in an internal cache entry named <var>
#
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)
check_cxx_compiler_flag(-Wno-missing-field-initializers
        LSMDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)

#
# check_cxx_source_compiles(<code> <resultVar>)
#
# check that source supplied in <code> can be compiled
# as a C++ source file and linked as an executable
# The result will be stored in the internal cache
# variable specified by <resultVar>, with a boolean
# true value for success and false value for failure.
#
# Test whether C++17 __has_include is available.
check_cxx_source_compiles("
#if defined(__has_include) &&  __has_include(<string>)
#include <string>
#endif
int main() { std::string str; return 0; }
" HAVE_CXX17_HAS_INCLUDE)

set(LSMDB_PUBLIC_INCLUDE_DIR "include/lsmdb")
set(LSMDB_PORT_CONFIG_DIR "include/port")

#
# configure_file(<input> <output>)
#
# copies an <input> file to an <output> file and substitutes variable values referenced
# as @VAR@ or ${VAR} in the input file content.
#
configure_file(
        "port/port_config.h.in"
        "${PROJECT_BINARY_DIR}/${LSMDB_PORT_CONFIG_DIR}/port_config.h"
)
include_directories(
        "${PROJECT_BINARY_DIR}/include"
        "."
)

if (BUILD_SHARED_LIBS)
    add_compile_options(-fvisibility=hidden)
endif(BUILD_SHARED_LIBS)

# Must be included before CMAKE_INSTALL_INCLUDEDIR is used.
include(GNUInstallDirs)

#
# target_source(<target>
# <INTERFACE|PUBLIC|PRIVATE [items1...]
# <INTERFACE|PUBLIC|PRIVATE [items2...]...
#
# Specifies sources to use when building a target and/or
# its dependents. The name <target> must be have been
# created by a command such as add_executable() or
# add_library() or add_custom_target() and must not
# be a ALIAS target.
#
# 这里解释一下PRIVATE, PUBLIC, INTERFACE的区别
# PRIVATE：只用于该target的构建，不用于使用该target的其他对象
# INTERFACE：只用于使用该target的其他对象
# PUBLIC：既用于该target的构建，也用于使用该target的其他对象
# 举个例子：header_a1和header_a2都用于lib_a，其中header_a1是库
# 的对外接口设为PUBLIC，而header_a2则是内部使用的头文件设为
# PRIVATE，当lib_b使用lib_a时，header_a1作为接口应该被lib_b可
# 见，而header_a2是lib_a的内部头文件不应可见
#
add_library(lsmdb "")
target_sources(lsmdb
        PRIVATE
        "${PROJECT_BINARY_DIR}/${LSMDB_PORT_CONFIG_DIR}/port_config.h"
        "util/cache.cc"
        "util/env.cc"
        "util/hash.cc"
        "util/hash.h"
        "util/coding.cc"
        "util/coding.h"
        "util/mutexlock.h"
        "util/status.cc"
        "port/port.h"
        "port/port_stdcxx.h"
        "port/thread_annotations.h"

        PUBLIC
        "${LSMDB_PUBLIC_INCLUDE_DIR}/c.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/cache.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/comparator.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/db.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/dumpfile.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/env.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/export.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/filter_policy.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/iterator.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/options.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/slice.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/status.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/table.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/table_builder.h"
        "${LSMDB_PUBLIC_INCLUDE_DIR}/write_batch.h"
        )

if (WIN32)
    target_sources(lsmdb
            PRIVATE
            "util/env_windows.cc"
            "util/windows_logger.h"
            )
else (WIN32)
    target_sources(lsmdb
            PRIVATE
            "util/env_posix.cc"
            "util/posix_logger.h"
            )
endif (WIN32)

#
# PUBLIC, PRIVATE, INTERFACE
# refer to this answer:https://stackoverflow.com/questions/26243169/cmake-target-include-directories-meaning-of-scope
#
# Therefore, suppose you are creating a library A that uses some Boost headers. You would do:
#
# target_include_directories(A PRIVATE ${Boost_INCLUDE_DIRS})
# if you only use those Boost headers inside your source files (.cpp) or private header files (.h).
#
# target_include_directories(A INTERFACE ${Boost_INCLUDE_DIRS})
# if you don't use those Boost headers inside your source files (therefore, not needing them to compile A).
# I can't actually think of a real-world example for this.
#
# target_include_directories(A PUBLIC ${Boost_INCLUDE_DIRS})
# if you use those Boost headers in your public header files, which are included BOTH in some of A's source files
# and might also be included in any other client of your A library.
#
target_include_directories(lsmdb
        PUBLIC
# Include directories usage requirements commonly differ between the build-tree
# and the install-tree. The BUILD_INTERFACE and INSTALL_INTERFACE generator expressions
# can be used to describe separate usage requirements based on the usage location
# Relative paths are allowed within the INSTALL_INTERFACE expression and are
# interpreted relative to the installation prefix.
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )

set_target_properties(lsmdb
        PROPERTIES VERSION ${PROJECT_VERSION}
        # 设置库版本号为工程主版本号
        SOVERSION ${PROJECT_VERSION_MAJOR})

#
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        target_compile_options(lsmdb
                PRIVATE
                -g)
    endif(${CMAKE_BUILD_TYPE} MATCHES "Debug")
endif(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

# 为target设置编译标志
target_compile_definitions(lsmdb
        PRIVATE
        # Used by include/export.h when building shared libraries.
        LSMDB_COMPILE_LIBRARY
        # Used by port/port.h.
        ${LSMDB_PLATFORM_NAME}=1
        )

if (NOT HAVE_CXX17_HAS_INCLUDE)
    # 为target设置编译标志
    target_compile_definitions(lsmdb
            PRIVATE
            LSMDB_HAS_PORT_CONFIG_H=1
            )
endif(NOT HAVE_CXX17_HAS_INCLUDE)

if (BUILD_SHARED_LIBS)
    # 为target设置编译标志
    target_compile_definitions(lsmdb
            PUBLIC
            # Used by include/export.h
            LSMDB_SHARED_LIBRARY)
endif(BUILD_SHARED_LIBS)

if (HAVE_CLANG_THREAD_SAFETY)
    target_compile_options(lsmdb
            PUBLIC
            -Werror -Wthread-safety)
endif(HAVE_CLANG_THREAD_SAFETY)

if(HAVE_CRC32C)
    target_link_libraries(lsmdb crc32c)
endif(HAVE_CRC32C)
if(HAVE_SNAPPY)
    target_link_libraries(lsmdb snappy)
endif(HAVE_SNAPPY)
if(HAVE_TCMALLOC)
    target_link_libraries(lsmdb tcmalloc)
endif(HAVE_TCMALLOC)

# Needed by port_stdcxx.h
find_package(Threads REQUIRED)
target_link_libraries(lsmdb Threads::Threads)

add_executable(lsmdb_util
        "db/lsmdb_util.cc")
target_link_libraries(lsmdb_util lsmdb)

if (LSMDB_BUILD_TESTS)
    enable_testing()

    # Prevent overriding the parent project's compiler/linker settings on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(install_gtest OFF)
    set(install_gmock OFF)
    set(build_gmock ON)

    # This project is tested using GoogleTest
    add_subdirectory("third_party/googletest")

    # GoogleTest triggers a missing field initializers warning.
    if (LSMDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)
        set_property(TARGET gtest
                APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-field-initializers)
        set_property(TARGET gmock
                APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-field-initializers)
    endif(LSMDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)

    function(lsmdb_test test_file)
        get_filename_component(test_target_name "${test_file}" NAME_WE)

        add_executable("${test_target_name}" "")
        target_sources("${test_target_name}"
                PRIVATE
                "${PROJECT_BINARY_DIR}/${LSMDB_PORT_CONFIG_DIR}/port_config.h"
                "util/test_util.cc"
                "util/test_util.h"
                "${test_file}"
                )
        target_link_libraries("${test_target_name}" lsmdb gmock gtest)
        target_compile_definitions("${test_target_name}"
                PRIVATE
                ${LSMDB_PLATFORM_NAME}=1)
        target_compile_options("${test_target_name}" PUBLIC -g)
        if (NOT HAVE_CXX17_HAS_INCLUDE)
            target_compile_definitions("${test_target_name}"
                    PRIVATE
                    LSMDB_HAS_PORT_CONFIG_H=1)
        endif(NOT HAVE_CXX17_HAS_INCLUDE)
        add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
    endfunction(lsmdb_test)

    lsmdb_test("util/cache_test.cc")
    lsmdb_test("util/status_test.cc")
    lsmdb_test("util/hash_test.cc")

endif(LSMDB_BUILD_TESTS)

# get_property(dirs DIRECTORY PROPERTY SUBDIRECTORIES)
# message(STATUS "${dirs}")
# get_property(dirs TARGET lsmdb PROPERTY INCLUDE_DIRECTORIES)
# message(STATUS "build lsmdb include_directories: ${dirs}")





