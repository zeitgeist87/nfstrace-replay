cmake_minimum_required(VERSION 3.13)
project(nfstrace-replay)

include(CheckIPOSupported)
include(CheckCXXCompilerFlag)
include(CTest)

find_package(Curses)

set(MAIN_EXE nfsreplay)
set(TEST_EXE ${MAIN_EXE}_test)

add_executable(${MAIN_EXE} "")
add_executable(${TEST_EXE} "")

add_subdirectory(src)
add_subdirectory(test)

target_link_libraries(${MAIN_EXE} PRIVATE ${CURSES_LIBRARIES})
target_compile_definitions(${MAIN_EXE} PRIVATE _FILE_OFFSET_BITS=64)
target_include_directories(${MAIN_EXE} PRIVATE src ${CURSES_INCLUDE_DIRS})
target_compile_features(${MAIN_EXE} PRIVATE cxx_std_17)

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT OUTPUT_MSG)

    if(IPO_SUPPORTED)
        set_property(TARGET ${MAIN_EXE} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO is not supported: ${OUTPUT_MSG}")
    endif()

    check_cxx_compiler_flag("-march=native" ARCH_NATIVE_SUPPORTED)
    if (ARCH_NATIVE_SUPPORTED)
        target_compile_options(${MAIN_EXE} PRIVATE -march=native)
        target_link_options(${MAIN_EXE} PRIVATE -march=native)
    endif()

    check_cxx_compiler_flag("-fwhole-program" WHOLE_PROGRAM_SUPPORTED)
    if (WHOLE_PROGRAM_SUPPORTED)
        target_link_options(${MAIN_EXE} PRIVATE -fwhole-program)
    endif()
endif()
