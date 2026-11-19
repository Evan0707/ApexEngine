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
