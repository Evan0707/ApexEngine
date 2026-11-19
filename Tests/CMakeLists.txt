find_package(Catch2 3 CONFIG REQUIRED)

add_executable(ApexTests
    Core/AssertTests.cpp
    Core/ErrorTests.cpp
    Core/LanguageTests.cpp
    Core/LoggerTests.cpp
    Core/VersionTests.cpp
)

target_link_libraries(ApexTests PRIVATE Apex::Engine Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(ApexTests)

# Phase 0 smoke test. Remove in phase 1: CI has no display once ApexDemo opens a window.
add_test(NAME Smoke.ApexDemo COMMAND ApexDemo)
set_tests_properties(Smoke.ApexDemo PROPERTIES
    PASS_REGULAR_EXPRESSION "Apex Engine ${PROJECT_VERSION_MAJOR}\\.${PROJECT_VERSION_MINOR}\\.${PROJECT_VERSION_PATCH}"
)
