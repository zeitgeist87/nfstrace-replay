include(FetchContent)

FetchContent_Declare(
  catch
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.7.2
)

FetchContent_GetProperties(catch)
if(NOT catch_POPULATED)
  FetchContent_Populate(catch)
  add_subdirectory(${catch_SOURCE_DIR} ${catch_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

list(APPEND CMAKE_MODULE_PATH ${catch_SOURCE_DIR}/contrib/)
include(ParseAndAddCatchTests)

target_sources(${TEST_EXE}
    PRIVATE
        basic_test.cpp
)

target_link_libraries(${TEST_EXE} PRIVATE ${CURSES_LIBRARIES} Catch2::Catch2)
target_compile_definitions(${TEST_EXE} PRIVATE _FILE_OFFSET_BITS=64)
target_include_directories(${TEST_EXE} PRIVATE ../src ${CURSES_INCLUDE_DIRS})
target_compile_features(${TEST_EXE} PRIVATE cxx_std_17)

ParseAndAddCatchTests(${TEST_EXE})
