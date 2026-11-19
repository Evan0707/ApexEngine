find_package(SDL3 CONFIG REQUIRED)

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
    Input/ActionMap.cpp
    Input/ActionMap.h
    Platform/Event.h
    Platform/Key.cpp
    Platform/Key.h
    Platform/Window.cpp
    Platform/Window.h
)
add_library(Apex::Engine ALIAS ApexEngine)

# Include root: #include "Core/Version.h"
target_include_directories(ApexEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ApexEngine PUBLIC Apex::Options)
# SDL is included only by Platform/*.cpp (decision 0002).
target_link_libraries(ApexEngine PRIVATE SDL3::SDL3)

target_compile_definitions(ApexEngine PRIVATE
    APEX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    APEX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    APEX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
