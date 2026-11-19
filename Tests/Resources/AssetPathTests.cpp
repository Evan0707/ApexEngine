#include "Resources/AssetPath.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using apex::resources::AssetPath;
using Catch::Matchers::ContainsSubstring;

namespace {

[[nodiscard]] std::string normalized(std::string_view path) {
    const auto parsed = AssetPath::parse(path);
    REQUIRE(parsed.has_value());
    return parsed->str();
}

} // namespace

TEST_CASE("Asset paths use '/' and keep their case", "[resources]") {
    REQUIRE(normalized("Models/crate.glb") == "Models/crate.glb");
    REQUIRE(normalized("Models\\Crate.GLB") == "Models/Crate.GLB");
}

TEST_CASE("Asset paths drop '.', empty segments and resolve '..'", "[resources]") {
    REQUIRE(normalized("./Models//crate.glb") == "Models/crate.glb");
    REQUIRE(normalized("Models/../Textures/wood.png") == "Textures/wood.png");
    REQUIRE(normalized("Models/sub/../crate.glb/") == "Models/crate.glb");
}

TEST_CASE("Two spellings of one file give equal asset paths", "[resources]") {
    REQUIRE(*AssetPath::parse("Models\\.\\crate.glb") == *AssetPath::parse("Models/crate.glb"));
}

TEST_CASE("Absolute, empty and escaping asset paths are refused", "[resources]") {
    for (const char* path : {"", "/Models/crate.glb", "\\Models", "C:/Assets/crate.glb", "../crate.glb",
                             "Models/../../crate.glb", ".", "./"}) {
        CAPTURE(path);
        REQUIRE_FALSE(AssetPath::parse(path).has_value());
    }
    REQUIRE_THAT(AssetPath::parse("../secret.png").error().message, ContainsSubstring("'../secret.png'"));
}

TEST_CASE("A sibling path is relative to the asset's folder", "[resources]") {
    const AssetPath model = *AssetPath::parse("Models/crate.gltf");
    REQUIRE(model.resolveSibling("crate_color.png")->str() == "Models/crate_color.png");
    REQUIRE(model.resolveSibling("../Textures/wood.jpg")->str() == "Textures/wood.jpg");
    REQUIRE_FALSE(model.resolveSibling("../../wood.jpg").has_value());

    const AssetPath root = *AssetPath::parse("crate.gltf");
    REQUIRE(root.resolveSibling("color.png")->str() == "color.png");
}
