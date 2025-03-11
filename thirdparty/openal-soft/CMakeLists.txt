add_library(OpenAL SHARED IMPORTED GLOBAL)
set_target_properties(OpenAL PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/thirdparty/openal-soft/bin/OpenAL32.dll"
    IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/thirdparty/openal-soft/libs/OpenAL32.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/thirdparty/openal-soft/include"
)