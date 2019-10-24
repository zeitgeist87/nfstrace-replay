target_sources(nfsreplay
    PRIVATE
        nfsreplay.cpp
)

add_subdirectory(parser)
add_subdirectory(tree)
add_subdirectory(replay)
add_subdirectory(display)
