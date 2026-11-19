# Architecture

Ce document décrit l'architecture réellement en place. Il est complété à la fin de chaque phase. La cible est décrite dans `cahier_des_charges.md`, section 4.

## État actuel (phase 2)

| Cible CMake | Type | Contenu |
|---|---|---|
| `ApexEngine` | Bibliothèque statique | Modules `Core`, `Platform`, `Input`, `Graphics`, `Debug`, et le SPIR-V des shaders |
| `ApexDemo` | Exécutable | Dessine un triangle par Vulkan, affiche le panneau de debug (F1), fait tourner la boucle à pas fixe, logue les événements et les actions |
| `ApexTests` | Exécutable de tests | Tests de `Core`, `Platform`, `Input`, `Graphics`, `Debug` et de la démo, sans fenêtre ni GPU |
| `Apex::Options` | Cible d'interface | Options de compilation communes : C++23, avertissements en erreurs |

Presets : `windows-debug`, `windows-release`, et `windows-profile` (Release instrumenté par Tracy, décision 0003). Les trois sont compilés et testés par la CI, qui installe aussi le SDK Vulkan pour `glslc`.

Les shaders GLSL sont compilés en SPIR-V par `glslc` pendant le build (`cmake/ApexShaders.cmake`) et embarqués dans la bibliothèque (décision 0006).

## Modules en place

| Module | Rôle | Dépend de | Bibliothèque tierce | Documentation |
|---|---|---|---|---|
| Core | Erreurs, assertions, logger, pas fixe, boucle principale, macros de profilage | Bibliothèque standard | Tracy, dans `Core/Profiling.h` uniquement et seulement avec `windows-profile` | `Engine/Core/README.md` |
| Platform | Touches physiques, événements, fenêtre, pont vers Vulkan et ImGui par handles opaques | Core | SDL3, dans `Platform/*.cpp` uniquement | `Engine/Platform/README.md` |
| Input | Table des actions, état des actions | Core, Platform | Aucune | `Engine/Input/README.md` |
| Graphics | Périphérique Vulkan 1.3, mémoire GPU, swapchain, frames en vol, destruction différée, triangle | Core, Platform | Vulkan-Headers, Vulkan-Loader, vk-bootstrap, VMA, dans `Graphics` uniquement | `Engine/Graphics/README.md` |
| Debug | Statistiques de frame, historique des logs, interface Dear ImGui, panneau de la section 5.9 | Core, Platform, Graphics | Dear ImGui dans `Debug` ; SDL et Vulkan dans `Debug/ImGuiBackend.cpp` uniquement | `Engine/Debug/README.md` |

`Game` contient la démonstration : la table d'actions par défaut (`DefaultBindings`), les options de ligne de commande (`CommandLine`) et l'implémentation des phases de la frame (`DemoApp`). Il n'inclut ni SDL, ni Vulkan, ni ImGui.

## Frontières entre modules (décision 0009)

- `Platform` crée la surface Vulkan sans inclure Vulkan : `Window::createVulkanSurface()` reçoit l'instance et renvoie la surface sous forme de handles opaques.
- `Graphics` n'expose aucun type Vulkan, sauf dans `Graphics/VulkanBackendAccess.h`, réservé au backend ImGui de `Debug`.
- L'interface de debug est dessinée par l'intermédiaire de `graphics::RenderOverlay`, à la fin de la passe principale du renderer.

## Déroulement d'une frame (phase 2)

`Game/main.cpp` crée les systèmes dans l'ordre de la section 5.1 : historique des logs, logger, journal de validation, fenêtre, table d'actions, renderer, interface de debug, application. Il lance ensuite `core::MainLoop::run`. L'arrêt se fait dans l'ordre inverse, par la destruction des objets ; les messages de validation de la fermeture sont donc comptés.

1. **Entrées.** `DemoApp::processInput` mesure la durée de la frame précédente pour le panneau, puis vide la file d'événements de la fenêtre. Les événements bruts passent aussi à ImGui. Ceux qu'ImGui utilise sont retirés avant que l'état `frame` applique le reste. Les actions qui ne relèvent pas de la simulation (capture du curseur, panneau de debug) sont traitées ici. L'état `frame` est ensuite fusionné dans l'état `simulation`. Une demande de fermeture arrête la boucle.
2. **Simulation.** De zéro à quatre pas de 1/60 s. Chaque pas lit uniquement l'état `simulation`, puis vide ses fronts.
3. **Extraction.** Vide en phase 2.
4. **Rendu.** `DebugUi::buildFrame` construit l'interface. `Renderer::renderFrame` attend la fence du créneau de frame, acquiert une image, enregistre la passe (effacement, triangle, interface) et la soumet. Fenêtre réduite : rien n'est rendu.
5. **Présentation.** `Renderer::presentFrame` présente l'image. Avec la vsync, l'attente se fait ici ou dans l'acquisition suivante.

Logs : console, `Logs/apex.log` relatif au dossier de lancement, et console du panneau de debug pour les avertissements et les erreurs.

Codes de sortie de `ApexDemo` : 0 fermeture normale, 1 échec, 2 fermeture normale avec au moins un message des validation layers.

## Décisions prises

| Note | Phase | Objet |
|---|---|---|
| 0002 | 1 | Ajout de SDL3 |
| 0003 | 1 | Ajout de Tracy, preset `windows-profile` |
| 0004 | 1 | Deux agents en parallèle dès la phase 1 (écart assumé avec la section 10.1 du cahier des charges) |
| 0005 | 1 | Inclusion de `SDL_main.h` avec `SDL_MAIN_HANDLED` ; licence SDL par renvoi au fichier installé par vcpkg |
| 0006 | 2 | Chaîne Vulkan : en-têtes et loader par vcpkg, SDK pour la validation et `glslc`, vk-bootstrap, code de sortie 2, GLM reporté en phase 3 |
| 0007 | 2 | Ajout de Vulkan Memory Allocator |
| 0008 | 2 | Ajout de Dear ImGui, backends SDL3 et Vulkan |
| 0009 | 2 | Frontières entre `Platform`, `Graphics` et `Debug` : handles opaques, en-tête réservé au backend ImGui |

## Revue des garde-fous (AC-17)

À remplir à la fin de chaque phase. Un garde-fou sans objet à ce stade est noté « sans objet ».

| ID | Phase 0 | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 | Phase 6 | Phase 7 | Phase 8 |
|---|---|---|---|---|---|---|---|---|---|
| G-01 | sans objet | sans objet | conforme | | | | | | |
| G-02 | sans objet | sans objet | conforme | | | | | | |
| G-03 | sans objet | sans objet | sans objet | | | | | | |
| G-04 | sans objet | sans objet | sans objet | | | | | | |
| G-05 | sans objet | conforme | conforme | | | | | | |
| G-06 | conforme | conforme | conforme | | | | | | |
| G-07 | sans objet | sans objet | sans objet | | | | | | |
| G-08 | sans objet | sans objet | sans objet | | | | | | |
| G-09 | sans objet | sans objet | conforme | | | | | | |
| G-10 | conforme | conforme | conforme | | | | | | |

Détail de la phase 1 :

- **G-05 :** `FramePhases::simulate` ne reçoit que `FixedStep` (numéro et durée du pas). Seul `MainLoop::run` lit l'horloge de la frame ; `Input` ne voit jamais le temps. Couvert par les tests du pas fixe et de la boucle.
- **G-06 :** aucune variable `static` modifiable dans `Engine/` ni `Game/` (recherche faite). Le logger est une instance passée par référence, protégée par un mutex, et testée depuis 8 threads. Tracy garde un état interne à la bibliothèque ; le moteur n'en ajoute aucun.
- **G-10 :** `#include <SDL3/…>` seulement dans `Engine/Platform/Window.cpp`, `#include <tracy/…>` seulement dans `Engine/Core/Profiling.h`, aucune API Windows (recherches faites). Touches désignées par codes physiques (section 3.3), temps en secondes.

Détail de la phase 2 :

- **G-01 :** les appels Vulkan ne sont émis que par `Graphics` et par `Debug/ImGuiBackend.cpp`, le backend ImGui que la section 4.2 autorise (recherche faite). Le backend n'enregistre que dans le command buffer que `Graphics` lui passe, au moment choisi par `Graphics`. Il n'existe pas encore de `RenderScene` : `Graphics` ne reçoit rien du jeu. Elle arrive en phase 3.
- **G-02 :** le seul pipeline du moteur (triangle) n'a ni push constant ni descripteur (recherche de `PushConstant` faite). Le buffer par frame lu par `gl_InstanceIndex` arrive avec les objets, en phase 3.
- **G-05 :** le temps réel n'est lu que par `MainLoop::run`, par `DemoApp` (durée de frame pour le panneau de debug) et par le renderer (temps bloqué sur le GPU, pour le même panneau). Aucune de ces valeurs n'atteint `simulate`.
- **G-06 :** toujours aucune variable `static` modifiable dans `Engine/` ni `Game/` (recherche faite). Renderer, allocateur GPU, interface de debug et historique des logs sont des objets possédés par `main` et passés par référence. `ValidationLog` est utilisable depuis plusieurs threads (compteur atomique, testé depuis 8 threads), comme `LogHistory` (mutex, testé pendant que 4 threads loguent). Dear ImGui garde un pointeur global vers son contexte, un état interne à la bibliothèque comme celui de Tracy (décision 0008).
- **G-09 :** le format de la cible et le nombre d'échantillons sont passés à la création des pipelines : `RenderTargetDesc` pour le triangle, format de la swapchain pour ImGui. Le format de la swapchain est choisi en un seul endroit (`Swapchain.cpp`), et les pipelines suivent ce format quand une recréation le change. Pas encore de tampon de profondeur (phase 3).
- **G-10 :** `#include <SDL3/…>` dans `Platform/Window.cpp` et `Debug/ImGuiBackend.cpp` ; `#include <vulkan/…>`, `VkBootstrap.h` et `vk_mem_alloc.h` dans `Graphics`, plus `vulkan.h` dans `Debug/ImGuiBackend.cpp` ; `#include <imgui…>` dans `Debug` ; aucun dans `Game` ; aucune API Windows (recherches faites). Espace de clip de Vulkan et couleurs linéaires sur une swapchain sRGB (section 3.3) ; l'inversion de Y par la projection arrive avec la caméra en phase 3.
