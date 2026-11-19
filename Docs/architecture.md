# Architecture

Ce document décrit l'architecture réellement en place. Il est complété à la fin de chaque phase. La cible est décrite dans `cahier_des_charges.md`, section 4.

## État actuel (phase 4)

| Cible CMake | Type | Contenu |
|---|---|---|
| `ApexEngine` | Bibliothèque statique | Modules `Core`, `Platform`, `Input`, `Resources`, `Graphics`, `Debug`, et le SPIR-V des shaders |
| `ApexDemo` | Exécutable | Dessine une scène éclairée (sol, cubes, sphères, modèles glTF texturés lus dans `Assets/`) vue par une caméra libre, affiche le panneau de debug (F1), fait tourner la boucle à pas fixe, logue les événements et les actions, mesure les performances (`--benchmark`) |
| `ApexTests` | Exécutable de tests | Tests de `Core`, `Platform`, `Input`, `Resources`, `Graphics`, `Debug` et de la démo, sans fenêtre ni GPU |
| `Apex::Options` | Cible d'interface | Options de compilation communes : C++23, avertissements en erreurs |

Presets : `windows-debug`, `windows-release`, et `windows-profile` (Release instrumenté par Tracy, décision 0003). Les trois sont compilés et testés par la CI, qui installe aussi le SDK Vulkan pour `glslc`.

Les shaders GLSL sont compilés en SPIR-V par `glslc` pendant le build (`cmake/ApexShaders.cmake`) et embarqués dans la bibliothèque (décision 0006).

Le dossier `Assets/` est copié à côté de `ApexDemo` à chaque build (cible `ApexDemoAssets`, décision 0014). Ses fichiers générés viennent de `Tools/generate_assets.py`, que le build ne lance pas ; les tests lisent `Assets/` et `Tests/Data/` en place.

## Modules en place

| Module | Rôle | Dépend de | Bibliothèque tierce | Documentation |
|---|---|---|---|---|
| Core | Erreurs, assertions, logger, pas fixe, boucle principale, macros de profilage, mathématiques, `WorldPosition` | Bibliothèque standard | GLM, inclus par `Core/Math.h` uniquement ; Tracy, dans `Core/Profiling.h` uniquement et seulement avec `windows-profile` | `Engine/Core/README.md` |
| Platform | Touches physiques, événements, fenêtre, pont vers Vulkan et ImGui par handles opaques, dossier de l'exécutable | Core | SDL3, dans `Platform/*.cpp` uniquement | `Engine/Platform/README.md` |
| Input | Table des actions, état des actions | Core, Platform | Aucune | `Engine/Input/README.md` |
| Resources | Données CPU (meshes, matériaux, textures, modèles), import glTF, décodage PNG et JPG, chemins d'assets, handles avec état, cache, assets de secours, primitives | Core | cgltf dans `Resources/GltfImporter.cpp`, stb_image dans `Resources/ImageDecoder.cpp`, uniquement | `Engine/Resources/README.md` |
| Graphics | Périphérique Vulkan 1.3, mémoire GPU, swapchain, frames en vol, destruction différée, profondeur, reflet GPU du cache (meshes, textures, jeux de matériau), données par frame, `RenderScene`, éclairage PBR | Core, Platform, Resources | Vulkan-Headers, Vulkan-Loader, vk-bootstrap, VMA, dans `Graphics` uniquement | `Engine/Graphics/README.md` |
| Debug | Statistiques de frame, mesure de performance, historique des logs, interface Dear ImGui, panneau de la section 5.9 | Core, Platform, Graphics | Dear ImGui dans `Debug` ; SDL et Vulkan dans `Debug/ImGuiBackend.cpp` uniquement | `Engine/Debug/README.md` |

`Game` contient la démonstration : la table d'actions par défaut (`DefaultBindings`), les options de ligne de commande (`CommandLine`), la scène écrite en code jusqu'à la phase 5 (`DemoScene`, dont les modèles viennent du cache de ressources), la caméra libre (`FreeCamera`), l'extraction vers l'espace de rendu (`SceneExtraction`, qui passera dans `World` en phase 5) et l'implémentation des phases de la frame (`DemoApp`). Il n'inclut ni SDL, ni Vulkan, ni ImGui.

## Frontières entre modules (décision 0009)

- `Platform` crée la surface Vulkan sans inclure Vulkan : `Window::createVulkanSurface()` reçoit l'instance et renvoie la surface sous forme de handles opaques.
- `Graphics` n'expose aucun type Vulkan, sauf dans `Graphics/VulkanBackendAccess.h`, réservé au backend ImGui de `Debug`.
- L'interface de debug est dessinée par l'intermédiaire de `graphics::RenderOverlay`, dans une seconde passe, après la scène (décision 0011).
- `Graphics` ne reçoit du jeu qu'une `RenderScene` (G-01) : caméra, lumière, terme ambiant et objets (handles de mesh et de matériau de `Resources`, matrice relative à la caméra), copiés à chaque frame.
- Un seul système de handles, celui de `Resources` (décision 0014). `Graphics` lit le cache une fois par frame (`Renderer::syncResources`) et en garde un reflet GPU désigné par ces handles ; il ne garde aucun pointeur vers les données du cache.
- Le panneau de debug ne connaît pas la caméra : il modifie une `DebugControls` que l'application applique (décision 0011).

## Déroulement d'une frame (phase 4)

`Game/main.cpp` crée les systèmes dans l'ordre de la section 5.1 : historique des logs, logger, journal de validation, fenêtre, table d'actions, renderer, cache de ressources (dossier `Assets` à côté de l'exécutable, ou `--assets`), interface de debug, scène de démonstration (primitives, matériaux et modèles chargés dans le cache), application. Il lance ensuite `core::MainLoop::run`. L'arrêt se fait dans l'ordre inverse, par la destruction des objets ; les messages de validation de la fermeture sont donc comptés.

1. **Entrées.** `DemoApp::processInput` mesure la durée de la frame précédente pour le panneau, puis vide la file d'événements de la fenêtre. Les événements bruts passent aussi à ImGui. Ceux qu'ImGui utilise sont retirés avant que l'état `frame` applique le reste. Les actions qui ne relèvent pas de la simulation (capture du curseur, panneau de debug) sont traitées ici. L'état `frame` est ensuite fusionné dans l'état `simulation`. Une demande de fermeture arrête la boucle.
2. **Simulation.** De zéro à quatre pas de 1/60 s. Chaque pas lit uniquement l'état `simulation`, puis vide ses fronts. La caméra libre (axes, course, souris si le curseur est capturé) et le cube tournant avancent d'un pas ; l'état précédent est gardé pour l'interpolation.
3. **Extraction.** Caméra et cube tournant sont interpolés entre les deux derniers pas (`alpha`, section 5.1). Les positions monde deviennent des matrices relatives à la caméra (G-07). Chaque objet donne un `RenderObject` par partie de son modèle ; un modèle pas prêt donne les parties du modèle de secours.
4. **Rendu.** `DebugUi::buildFrame` construit l'interface. `Renderer::syncResources(cache)` envoie au GPU ce qui est devenu prêt et libère ce qui a été déchargé. `Renderer::renderFrame(scene)` attend la fence du créneau de frame, acquiert une image, résout les handles (assets de secours si besoin, G-03), trie les objets par matériau, écrit les données de la frame (caméra, lumière, objets, matériaux), enregistre la passe de scène (couleur et profondeur, un jeu de descripteurs par matériau) puis la passe de l'interface, et soumet. Fenêtre réduite : rien n'est rendu.
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
| 0010 | 3 | Ajout de GLM, point d'inclusion unique `Core/Math.h` |
| 0011 | 3 | Rendu de la phase 3 : module `Resources` dès la phase 3, table des matériaux, champ de vision dans le panneau, caméra par pas fixes |
| 0012 | 4 | Ajout de cgltf |
| 0013 | 4 | Ajout de stb_image |
| 0014 | 4 | Ressources : un seul système de handles, nœuds aplatis dans les sommets, assets générés par script, `Assets/` à côté de l'exécutable |

## Revue des garde-fous (AC-17)

À remplir à la fin de chaque phase. Un garde-fou sans objet à ce stade est noté « sans objet ».

| ID | Phase 0 | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 | Phase 6 | Phase 7 | Phase 8 |
|---|---|---|---|---|---|---|---|---|---|
| G-01 | sans objet | sans objet | conforme | conforme | conforme | | | | |
| G-02 | sans objet | sans objet | conforme | conforme | conforme | | | | |
| G-03 | sans objet | sans objet | sans objet | sans objet | conforme | | | | |
| G-04 | sans objet | sans objet | sans objet | conforme | conforme | | | | |
| G-05 | sans objet | conforme | conforme | conforme | conforme | | | | |
| G-06 | conforme | conforme | conforme | conforme | conforme | | | | |
| G-07 | sans objet | sans objet | sans objet | conforme | conforme | | | | |
| G-08 | sans objet | sans objet | sans objet | conforme | conforme | | | | |
| G-09 | sans objet | sans objet | conforme | conforme | conforme | | | | |
| G-10 | conforme | conforme | conforme | conforme | conforme | | | | |

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

Détail de la phase 3 :

- **G-01 :** `Graphics` ne reçoit du jeu qu'une `RenderScene` copiée à chaque frame : handles de mesh et de matériau, matrices, caméra, lumière. Aucun pointeur vers les objets du jeu. Les appels Vulkan restent dans `Graphics` et dans `Debug/ImGuiBackend.cpp` (recherche faite).
- **G-02 :** les données par objet (matrice du modèle, matrice des normales, index de matériau) sont dans un storage buffer par frame en vol ; le dessin `i` utilise `firstInstance = i`, et `Scene.vert` lit l'objet par `gl_InstanceIndex`. Aucun push constant (recherche faite).
- **G-03 :** sans objet jusqu'à la phase 4 (cache et handles avec état de `Resources`). Les handles GPU de `Graphics` ont déjà une génération : un handle périmé ne désigne jamais un autre objet ; un mesh périmé n'est pas dessiné, un matériau périmé donne le matériau par défaut. Aucun pointeur de ressource n'est gardé d'une frame à l'autre.
- **G-04 :** `Graphics` ne connaît que `resources::MeshData` et `resources::MaterialData`, indépendants de tout format de fichier. Pas encore d'importeur : cgltf et stb arrivent en phase 4.
- **G-05 :** la caméra libre et le cube tournant avancent dans `simulate`, par pas fixes ; l'extraction les interpole avec `alpha`. Le temps réel n'est lu que par `MainLoop::run`, par `DemoApp` (panneau de debug et mesure de performance) et par le renderer (temps bloqué sur le GPU) ; aucune de ces valeurs n'atteint `simulate`.
- **G-06 :** aucune variable `static` modifiable dans `Engine/` ni `Game/` (recherche faite). La scène de démonstration, la caméra, les réglages du panneau et l'enregistreur de mesure sont des objets possédés par l'application.
- **G-07 :** les positions monde sont des `WorldPosition` (`Core/WorldPosition.h`). `Game/SceneExtraction.cpp` est le seul endroit qui les convertit : la soustraction de la position de la caméra précède tout produit de matrices (test à 10 km de l'origine). `RenderCamera::view` n'est qu'une rotation.
- **G-08 :** un matériau est un jeu de données (`MaterialData`), copié dans une table par frame ; un seul pipeline pour tous les objets. Les shaders accèdent aux matériaux par `Shaders/Material.glsl` uniquement.
- **G-09 :** formats de couleur et de profondeur et nombre d'échantillons passent par `RenderTargetDesc` à la création du pipeline ; le format de profondeur est choisi en un seul endroit (`Renderer.cpp`), la taille du tampon de profondeur suit la swapchain.
- **G-10 :** confinement vérifié par recherche : Vulkan, vk-bootstrap et VMA dans `Graphics` et `Debug/ImGuiBackend.cpp` ; SDL dans `Platform/Window.cpp` et `Debug/ImGuiBackend.cpp` ; ImGui dans `Debug` ; GLM par `Core/Math.h` uniquement ; aucune API Windows. Conventions de la section 3.3 testées : repère main droite, +Y vers le haut, rotation positive vers la gauche, faces avant antihoraires des primitives, profondeur de 0 à 1 et inversion de Y dans la seule projection, face antihoraire toujours avant pour Vulkan.

Détail de la phase 4 :

- **G-01 :** `Graphics` reçoit toujours une `RenderScene` copiée, désormais avec les handles de `Resources`. Il lit le cache par `syncResources`, en lecture seule. Vulkan reste dans `Graphics` et `Debug/ImGuiBackend.cpp` (recherche faite).
- **G-02 :** inchangé : données par objet dans le buffer de la frame, lues par `gl_InstanceIndex`. Les textures passent par le jeu de descripteurs du matériau, pas par des push constants (recherche faite).
- **G-03 :** toute ressource est désignée par un handle typé avec état (`Loading`, `Ready`, `Failed`) et génération. Un modèle pas prêt donne le cube de secours en damier ; un mesh pas prêt, le cube de secours ; une texture pas prête, le damier magenta ; un matériau pas prêt, le matériau par défaut. Le renderer retrouve ses objets GPU par handle à chaque frame et ne garde aucun pointeur vers les données du cache ; `ResourcePool::find()` documente qu'un pointeur obtenu ne se garde pas. Tests automatisés : `ResourcePoolTests`, `ResourceCacheTests` (fichier manquant, texture manquante, déchargement), `ResourceMirrorTests`, `MaterialTableTests`.
- **G-04 :** l'importeur produit `ModelData`, `MeshData`, `MaterialData` et des images en octets ; le décodeur produit `TextureData`. `Graphics` ne connaît que ces structures. cgltf n'est inclus que par `GltfImporter.cpp`, stb_image que par `ImageDecoder.cpp` (recherche faite).
- **G-05 :** inchangé. Le chargement se fait avant la boucle ; aucun système de simulation ne lit le temps réel.
- **G-06 :** aucune variable `static` modifiable dans `Engine/` ni `Game/` (recherche faite). Le cache est un objet possédé par `main` et passé par référence. stb_image et cgltf ne gardent aucun état global modifiable par le moteur : aucune de leurs options globales n'est changée.
- **G-07 :** inchangé. Les nœuds glTF sont aplatis dans l'espace du modèle à l'import ; la conversion vers l'espace de rendu reste dans `Game/SceneExtraction.cpp`.
- **G-08 :** un matériau reste un jeu de données (facteurs et handles de texture), un seul pipeline pour tous les objets. Les shaders lisent facteurs et textures par `Shaders/Material.glsl` (`sampleMaterial()`), seul point d'accès.
- **G-09 :** inchangé ; le format des textures dépend de leur espace de couleur, choisi en un seul endroit (`textureFormat()`).
- **G-10 :** confinement vérifié par recherche : cgltf et stb_image dans `Resources` ; SDL dans `Platform/Window.cpp`, `Platform/Paths.cpp` et `Debug/ImGuiBackend.cpp` ; aucune API Windows. Conventions : chemins relatifs à `Assets/` avec `/` (normalisation testée), couleurs de base en sRGB et données en linéaire, repère de glTF conservé sans conversion (cube de référence et modèle de Khronos importés à leur taille et dans leur orientation, testés), faces antihoraires gardées sous un nœud miroir (testé).
