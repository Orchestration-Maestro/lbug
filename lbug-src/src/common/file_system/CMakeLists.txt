add_library(lbug_file_system
        OBJECT
        compressed_file_system.cpp
        file_info.cpp
        file_system.cpp
        local_file_system.cpp
        root_directory.cpp
        virtual_file_system.cpp
        gzip_file_system.cpp)

if(WIN32)
    target_sources(lbug_file_system PRIVATE root_directory_windows_security.cpp)
endif()

target_link_libraries(lbug_file_system Glob)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:lbug_file_system>
        PARENT_SCOPE)
