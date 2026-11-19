add_executable(ApexDemo
    CommandLine.cpp
    CommandLine.h
    DefaultBindings.cpp
    DefaultBindings.h
    DemoApp.cpp
    DemoApp.h
    DemoScene.cpp
    DemoScene.h
    FreeCamera.cpp
    FreeCamera.h
    main.cpp
    SceneExtraction.cpp
    SceneExtraction.h
)

target_link_libraries(ApexDemo PRIVATE Apex::Engine)

# The demo finds its assets next to it (decision 0014). Checked at every build, changed files copied.
add_custom_target(ApexDemoAssets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${PROJECT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:ApexDemo>/Assets"
    COMMENT "Copying Assets/ next to ApexDemo"
    VERBATIM)
add_dependencies(ApexDemo ApexDemoAssets)
