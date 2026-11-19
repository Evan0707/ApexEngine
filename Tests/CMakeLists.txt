find_package(Catch2 3 CONFIG REQUIRED)

add_executable(ApexTests
    Core/AssertTests.cpp
    Core/ErrorTests.cpp
    Core/FixedTimestepTests.cpp
    Core/LanguageTests.cpp
    Core/LoggerTests.cpp
    Core/MainLoopTests.cpp
    Core/MathTests.cpp
    Core/VersionTests.cpp
    Debug/BenchmarkRecorderTests.cpp
    Debug/ColorSpaceTests.cpp
    Debug/FrameTimeStatsTests.cpp
    Debug/InputCaptureTests.cpp
    Debug/LogHistoryTests.cpp
    Game/BindingsTests.cpp
    Game/CommandLineTests.cpp
    Game/FreeCameraTests.cpp
    Game/GameSystemsTests.cpp
    Graphics/DeferredDeleterTests.cpp
    Graphics/DrawListTests.cpp
    Graphics/FrameDataTests.cpp
    Graphics/MaterialTableTests.cpp
    Graphics/ProjectionTests.cpp
    Graphics/ResourceMirrorTests.cpp
    Graphics/ShaderBinariesTests.cpp
    Graphics/SwapchainStateTests.cpp
    Graphics/ValidationLogTests.cpp
    Input/ActionMapTests.cpp
    Input/ActionStateTests.cpp
    Platform/KeyTests.cpp
    Resources/AssetPathTests.cpp
    Resources/GltfImporterTests.cpp
    Resources/GltfMaterialTests.cpp
    Resources/ImageDecoderTests.cpp
    Resources/MeshDataTests.cpp
    Resources/PrimitivesTests.cpp
    Resources/ResourceCacheTests.cpp
    Resources/ResourcePoolTests.cpp
    Serialization/ConfigTests.cpp
    Serialization/DemoSceneFileTests.cpp
    Serialization/SceneJsonTests.cpp
    Serialization/SceneLoaderTests.cpp
    Serialization/SceneRoundTripTests.cpp
    World/RenderExtractionTests.cpp
    World/RotationTests.cpp
    World/SceneQueriesTests.cpp
    # The demo's binding table is plain data: tested here so that a typo fails in CI, not at launch.
    "${PROJECT_SOURCE_DIR}/Game/Bindings.cpp"
    "${PROJECT_SOURCE_DIR}/Game/CommandLine.cpp"
    "${PROJECT_SOURCE_DIR}/Game/FreeCamera.cpp"
    "${PROJECT_SOURCE_DIR}/Game/GameSystems.cpp"
)

target_include_directories(ApexTests PRIVATE "${PROJECT_SOURCE_DIR}/Game")
# Small files written by Tools/generate_assets.py; read in place, never written by the tests.
target_compile_definitions(ApexTests PRIVATE APEX_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/Tests/Data"
                                             APEX_ASSETS_DIR="${PROJECT_SOURCE_DIR}/Assets"
                                             APEX_CONFIG_DIR="${PROJECT_SOURCE_DIR}/Config")
target_link_libraries(ApexTests PRIVATE Apex::Engine Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(ApexTests)
