add_executable(ApexDemo
    CommandLine.cpp
    CommandLine.h
    DefaultBindings.cpp
    DefaultBindings.h
    DemoApp.cpp
    DemoApp.h
    main.cpp
)

target_link_libraries(ApexDemo PRIVATE Apex::Engine)
