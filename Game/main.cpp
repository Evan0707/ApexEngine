#include "Core/Version.h"

#include <cstdio>
#include <format>

int main() {
    const std::string line = std::format("Apex Engine {}\n", apex::core::engineVersion().toString());
    std::fputs(line.c_str(), stdout);
    return 0;
}
