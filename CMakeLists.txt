cmake_minimum_required(VERSION 3.13)
project(nfstrace-replay)

include(CheckIPOSupported)
include(CheckCXXCompilerFlag)

find_package(Curses)

add_executable(nfsreplay "")

target_sources(nfsreplay
    PRIVATE
        ConsoleDisplay.cpp
        FileSystemTree.cpp
        Parser.cpp
        TransactionMgr.cpp
        TreeNode.cpp
        nfsreplay.cpp
)

target_link_libraries(nfsreplay PRIVATE ${CURSES_LIBRARIES})
target_compile_definitions(nfsreplay PRIVATE _FILE_OFFSET_BITS=64)
target_include_directories(nfsreplay PRIVATE ${CURSES_INCLUDE_DIRS})

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT OUTPUT_MSG)

    if(IPO_SUPPORTED)
        set_property(TARGET nfsreplay PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO is not supported: ${OUTPUT_MSG}")
    endif()

    check_cxx_compiler_flag("-march=native" ARCH_NATIVE_SUPPORTED)
    if (ARCH_NATIVE_SUPPORTED)
        target_compile_options(nfsreplay PRIVATE -march=native)
        target_link_options(nfsreplay PRIVATE -march=native)
    endif()

    check_cxx_compiler_flag("-fwhole-program" WHOLE_PROGRAM_SUPPORTED)
    if (WHOLE_PROGRAM_SUPPORTED)
        target_link_options(nfsreplay PRIVATE -fwhole-program)
    endif()
endif()
