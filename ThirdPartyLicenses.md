# Licences des dépendances tierces

Toute dépendance ajoutée à `vcpkg.json` doit figurer ici, avec sa licence vérifiée au moment de l'intégration.

| Bibliothèque | Usage | Licence | Depuis | Source |
|---|---|---|---|---|
| Catch2 v3 | Tests automatisés | BSL-1.0 | Phase 0 | https://github.com/catchorg/Catch2 |
| SDL 3.4.20 | Fenêtre et événements (`Platform`) | Zlib ; le port vcpkg déclare « Zlib AND MIT AND Apache-2.0 AND BSD-3-Clause » pour le code tiers embarqué (texte complet : `share/sdl3/copyright` installé par vcpkg) | Phase 1 | https://github.com/libsdl-org/SDL |
| Tracy 0.15.0 | Profilage CPU (`Core/Profiling.h`), installé et lié avec le preset `windows-profile` uniquement | BSD-3-Clause ; le code tiers embarqué est sous BSD-2-Clause et MIT (texte complet : `share/tracy/copyright` installé par vcpkg) | Phase 1 | https://github.com/wolfpld/tracy |
| vk-bootstrap 1.4.363 | Création de l'instance et du périphérique Vulkan, swapchain (`Graphics`) | MIT, Copyright © 2020 Charles Giessen (texte : `share/vk-bootstrap/copyright` installé par vcpkg) | Phase 2 | https://github.com/charles-lunarg/vk-bootstrap |
| Vulkan-Headers 1.4.363.0 | En-têtes Vulkan (`Graphics`) | Apache-2.0 OR MIT, Copyright The Khronos Group Inc. (texte complet : `share/vulkan-headers/copyright` installé par vcpkg) | Phase 2 | https://github.com/KhronosGroup/Vulkan-Headers |
| Vulkan-Loader 1.4.363.0 | Loader Vulkan, `vulkan-1.dll` copiée à côté de l'exécutable (`Graphics`) | Apache-2.0 ; le port vcpkg déclare « Apache-2.0 AND HPND-Kevlin-Henney AND MIT » pour quelques fichiers (texte complet : `share/vulkan-loader/copyright` installé par vcpkg) | Phase 2 | https://github.com/KhronosGroup/Vulkan-Loader |
| Vulkan Memory Allocator 3.5.0 | Mémoire GPU (`Graphics/GpuAllocator`) | MIT, Copyright (c) 2017-2026 Advanced Micro Devices, Inc. (texte : `share/vulkan-memory-allocator/copyright` installé par vcpkg) | Phase 2 | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator |
| Dear ImGui 1.93.0 | Interface de debug, backends SDL3 et Vulkan (`Debug`) | MIT, Copyright (c) 2014-2026 Omar Cornut (texte : `share/imgui/copyright` installé par vcpkg) | Phase 2 | https://github.com/ocornut/imgui |
| GLM 1.0.3 | Mathématiques (`Core/Math.h`, types utilisés par tous les modules) | MIT, au choix avec la « Happy Bunny License » ; le projet retient la licence MIT. Copyright (c) 2005 G-Truc Creation (texte : `share/glm/copyright` installé par vcpkg) | Phase 3 | https://github.com/g-truc/glm |
| stb (2026-08-02), dont stb_image 2.30 | Décodage PNG et JPG (`Resources/ImageDecoder.cpp`) ; seul `stb_image.h` est utilisé | MIT ou domaine public, au choix ; le projet retient la licence MIT. Copyright (c) 2017 Sean Barrett (texte : `share/stb/copyright` installé par vcpkg) | Phase 4 | https://github.com/nothings/stb |
