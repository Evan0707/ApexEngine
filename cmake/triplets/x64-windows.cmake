# Overlay of vcpkg's x64-windows triplet (decision 0020). Same settings, plus one option for Jolt.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(PORT STREQUAL "joltphysics")
    # Jolt's default /fp:fast gives an unstable Release library with MSVC 19.52: a box resting on a box
    # starts to spin. This option builds Jolt with /fp:precise, and makes it deterministic across machines.
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCROSS_PLATFORM_DETERMINISTIC=ON")
endif()
