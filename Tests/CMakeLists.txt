find_package(Catch2 3 CONFIG REQUIRED)

add_executable(ApexTests
    Core/AssertTests.cpp
    Core/ErrorTests.cpp
    Core/FixedTimestepTests.cpp
    Core/LanguageTests.cpp
    Core/LoggerTests.cpp
    Core/MainLoopTests.cpp
    Core/VersionTests.cpp
    Debug/FrameTimeStatsTests.cpp
    Debug/LogHistoryTests.cpp
    Game/CommandLineTests.cpp
    Game/DefaultBindingsTests.cpp
    Graphics/DeferredDeleterTests.cpp
    Graphics/ValidationLogTests.cpp
    Input/ActionMapTests.cpp
    Input/ActionStateTests.cpp
    Platform/KeyTests.cpp
    # The demo's binding table is plain data: tested here so that a typo fails in CI, not at launch.
    "${PROJECT_SOURCE_DIR}/Game/CommandLine.cpp"
    "${PROJECT_SOURCE_DIR}/Game/DefaultBindings.cpp"
)

target_include_directories(ApexTests PRIVATE "${PROJECT_SOURCE_DIR}/Game")
target_link_libraries(ApexTests PRIVATE Apex::Engine Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(ApexTests)
