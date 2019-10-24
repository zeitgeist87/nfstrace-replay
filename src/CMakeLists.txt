target_sources(nfsreplay
    PRIVATE
        console_display.cpp
        nfsreplay.cpp
)

add_subdirectory(parser)
add_subdirectory(file_system)
