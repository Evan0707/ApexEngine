add_executable(ApexDemo
    Bindings.cpp
    Bindings.h
    CommandLine.cpp
    CommandLine.h
    DemoApp.cpp
    DemoApp.h
    FreeCamera.cpp
    FreeCamera.h
    GameComponents.h
    GameSystems.cpp
    GameSystems.h
    main.cpp
    OrbitCamera.cpp
    OrbitCamera.h
    PlayerMotion.cpp
    PlayerMotion.h
)

target_link_libraries(ApexDemo PRIVATE Apex::Engine)

# The demo finds its assets and its configuration next to it (decisions 0014 and 0017). Checked at every
# build, changed files copied.
add_custom_target(ApexDemoAssets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${PROJECT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:ApexDemo>/Assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${PROJECT_SOURCE_DIR}/Config" "$<TARGET_FILE_DIR:ApexDemo>/Config"
    COMMENT "Copying Assets/ and Config/ next to ApexDemo"
    VERBATIM)
add_dependencies(ApexDemo ApexDemoAssets)
