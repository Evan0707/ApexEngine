add_library(ApexEngine STATIC
    Core/Assert.cpp
    Core/Assert.h
    Core/ConsoleLogSink.cpp
    Core/ConsoleLogSink.h
    Core/Error.h
    Core/FileLogSink.cpp
    Core/FileLogSink.h
    Core/FixedTimestep.cpp
    Core/FixedTimestep.h
    Core/FramePhases.h
    Core/Logger.cpp
    Core/Logger.h
    Core/LogSink.cpp
    Core/LogSink.h
    Core/MainLoop.cpp
    Core/MainLoop.h
    Core/Version.cpp
    Core/Version.h
    Platform/Event.h
    Platform/Key.cpp
    Platform/Key.h
)
add_library(Apex::Engine ALIAS ApexEngine)

# Include root: #include "Core/Version.h"
target_include_directories(ApexEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ApexEngine PUBLIC Apex::Options)

target_compile_definitions(ApexEngine PRIVATE
    APEX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    APEX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    APEX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
