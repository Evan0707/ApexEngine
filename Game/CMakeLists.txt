add_executable(ApexDemo
    DefaultBindings.cpp
    DefaultBindings.h
    DemoApp.cpp
    DemoApp.h
    main.cpp
)

target_link_libraries(ApexDemo PRIVATE Apex::Engine)
