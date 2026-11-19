# 0009 — Frontières entre `Platform`, `Graphics` et `Debug`

**Date :** 19 novembre 2026
**Statut :** acceptée (choix délégués à l'agent par le développeur le 19 novembre 2026)

## Contexte

Les règles de dépendance (cahier des charges, section 4.2) confinent SDL à `Platform` et Vulkan à `Graphics`, avec une exception pour le backend ImGui de `Debug`. Trois besoins de la phase 2 traversent ces frontières :

1. créer la surface Vulkan de la fenêtre demande à la fois la fenêtre SDL et l'instance Vulkan ;
2. le backend SDL3 d'ImGui a besoin de la fenêtre SDL et des événements SDL bruts, que `Platform` traduit aujourd'hui en `platform::Event` ;
3. le backend Vulkan d'ImGui a besoin des handles Vulkan du renderer et d'un command buffer dans lequel dessiner.

## Décision

**Lecture de la règle.** « Interdit : Vulkan » pour `Platform` et « Interdit : SDL » pour `Graphics` signifient : aucun en-tête inclus, aucun type de la bibliothèque dans une interface. Faire transiter un handle opaque ne crée pas de dépendance.

**`Platform` → `Graphics` : la surface.** `platform::Window` gagne :

- `sizeInPixels()` : taille de la zone de dessin, en pixels ;
- `vulkanInstanceExtensions()` : les extensions d'instance demandées par SDL ;
- `createVulkanSurface(void* vkInstance)` : renvoie la surface sous forme de `std::uint64_t`.

`Platform` appelle `SDL_Vulkan_CreateSurface` sans inclure d'en-tête Vulkan : `SDL_vulkan.h` déclare lui-même les handles dont il a besoin. `Graphics` reconvertit le handle et détruit la surface. La fenêtre est créée avec l'option Vulkan de SDL.

**`Platform` → `Debug` : fenêtre et événements bruts.** `platform::Window` gagne :

- `nativeHandle()` : la fenêtre SDL sous forme de `void*` ;
- `setNativeEventObserver(...)` : une fonction appelée pour chaque événement SDL, avant sa traduction, avec un `const void*`.

Seul `Debug/ImGuiBackend.cpp` reconvertit ces pointeurs.

**`Graphics` → `Debug` : handles et dessin.**

- `Graphics/RenderOverlay.h`, sans type Vulkan : une interface à une méthode, `record`, que le renderer appelle à la fin de la passe principale.
- `Graphics/VulkanBackendAccess.h` : le seul en-tête public de `Graphics` qui contient des types Vulkan (instance, périphérique, file, format de la swapchain, command buffer de la frame). Il n'est inclus que par `Debug/ImGuiBackend.cpp`.
- Tous les autres en-têtes publics de `Graphics` restent sans type Vulkan : `Game` peut les inclure.

## Alternatives écartées

- **Surface créée dans `Graphics` par `vkCreateWin32SurfaceKHR` :** demande le `HWND` et `windows.h` dans `Graphics`, ce qui enfreint G-10 (aucune API Windows hors de `Platform`).
- **Types SDL ou Vulkan dans les interfaces :** enfreint le confinement et le propage à tous les fichiers qui incluent ces en-têtes.
- **Événements bruts conservés dans une liste publique :** expose la taille et la disposition d'une structure SDL ; une fonction de rappel ne montre qu'un pointeur.
- **ImGui alimenté par les `platform::Event` :** voir la note 0008.

## Conséquences

- **Interface publique de `Platform` modifiée :** cinq ajouts à `Window`, aucun retrait. `Game` et les tests ne changent pas. `Platform/README.md` est mis à jour.
- **Garde-fous :** G-01 et G-10. La vérification par recherche des `#include` reste valable telle quelle : SDL dans `Platform/*.cpp` et `Debug/ImGuiBackend.cpp`, Vulkan dans `Graphics` et `Debug/ImGuiBackend.cpp`.
- **Sécurité de type :** un `void*` ou un `std::uint64_t` mal employé n'est pas détecté par le compilateur. Le risque est limité à deux fichiers, et chaque fonction documente ce que le handle désigne.
- **Portage :** rien de propre à Windows n'est ajouté.
- **`AGENTS.md`, section 4 :** une ligne renvoie à cette note.
- **Coût de retour en arrière :** faible, les ajouts sont isolés.
