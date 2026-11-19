# Graphics

**Rôle :** renderer Vulkan 1.3, seul module qui émet des appels GPU (cahier des charges, sections 4.3 et 5.4, garde-fou G-01).

**État (phase 2, tâche 8) :** périphérique, mémoire GPU (VMA), swapchain sRGB, deux frames en vol, shaders compilés pendant le build. Chaque frame efface la fenêtre en bleu nuit et dessine un triangle coloré. Notes de décision `0006-chaine-vulkan.md` et `0009-frontieres-platform-graphics-debug.md`.

## Interface publique

Sauf `VulkanBackendAccess.h`, aucun en-tête public n'inclut Vulkan : `Game` peut inclure tous les autres.

| En-tête | Contenu |
|---|---|
| `Graphics/ValidationLog.h` | `ValidationSeverity`, `ValidationLog` : `report()`, `messageCount()` |
| `Graphics/FrameCounters.h` | `FrameCounters` : appels de dessin et triangles de la dernière frame |
| `Graphics/GpuMemoryStats.h` | `GpuMemoryStats` : mémoire GPU utilisée et budget |
| `Graphics/RenderOverlay.h` | `RenderOverlay` : ce qui est dessiné par-dessus la scène (l'interface de debug) |
| `Graphics/VulkanBackendAccess.h` | `OverlayCommands`, `VulkanBackendInfo`, `vulkanBackendInfo()`. **Seul en-tête public avec des types Vulkan, réservé à `Debug/ImGuiBackend.cpp`** (note 0009) |
| `Graphics/Renderer.h` | `RendererConfig`, `Renderer` : `create()`, `renderFrame()`, `presentFrame()`, `memoryStats()`, `lastFrameCounters()`, `setOverlay()` |

En-têtes internes, inclus uniquement par les fichiers `.cpp` de `Graphics` :

- `Graphics/Device.h` (`DeviceConfig`, `Device`), qui inclut Vulkan et vk-bootstrap ;
- `Graphics/DeferredDeleter.h` (`DeferredDeleter`), sans Vulkan, testé sans GPU ;
- `Graphics/Swapchain.h` (`SwapchainConfig`, `Swapchain`) ;
- `Graphics/SwapchainState.h` (`FrameDecision`, `SwapchainState`), sans Vulkan, testé sans GPU ;
- `Graphics/ShaderBinaries.h` : le SPIR-V des shaders, embarqué dans le binaire ;
- `Graphics/GpuAllocator.h` (`GpuBuffer`, `GpuAllocator`), seul utilisateur de VMA. L'implémentation de VMA est compilée dans `VmaImplementation.cpp` ;
- `Graphics/RenderTargetDesc.h` (`RenderTargetDesc`) : format et nombre d'échantillons de la cible, passés à la création des pipelines (G-09) ;
- `Graphics/TrianglePipeline.h` (`TriangleVertex`, `TrianglePipeline`).
- `Graphics/FrameSync.h` (`FrameResources`, `FrameSync`).

## Renderer

- `Renderer::create(window, logger, validation, config)` crée le périphérique. Échec : `ErrorCode::GpuFailure` (ou `PlatformFailure` si SDL ne peut pas créer la surface), avec un message qui dit ce qui manque. Le message est aussi écrit dans le log.
- La fenêtre, le logger et le `ValidationLog` doivent vivre plus longtemps que le renderer. Le renderer n'est ni copiable ni déplaçable : il est tenu par un `std::unique_ptr`.
- Le destructeur attend la fin du travail du GPU, puis détruit la swapchain et les objets des frames, exécute les destructions différées restantes, et détruit enfin le périphérique, la surface et l'instance.
- `Renderer.h` n'expose aucun type Vulkan : l'état du renderer est une structure privée, définie dans `Renderer.cpp`.
- La swapchain est créée dès `create()` quand la fenêtre a une taille : le backend Vulkan d'ImGui a besoin de son format à l'initialisation.
- `setOverlay()` : l'overlay enregistré dessine à la fin de la passe principale de chaque frame, après la scène (note 0008).

## Frame

- `renderFrame()` (phase de rendu) : attend la fence du créneau de frame, acquiert une image de la swapchain, enregistre et soumet les commandes. `presentFrame()` (phase de présentation) présente l'image acquise. Une erreur renvoyée par l'une ou l'autre signifie que le périphérique est inutilisable : la démo s'arrête avec le code de sortie 1.
- Deux frames en vol (section 5.4) : chacune a son pool et son command buffer, sa fence et son sémaphore « image disponible ».
- Le sémaphore « rendu terminé », attendu par la présentation, existe une fois par **image** de la swapchain, et non par frame en vol. Avec un sémaphore par frame, il pourrait être signalé de nouveau alors que la présentation précédente l'attend encore.
- Le rendu se fait par dynamic rendering, sans render pass. Les transitions d'image utilisent synchronization2.
- Fenêtre réduite (taille nulle) : `renderFrame()` et `presentFrame()` ne font rien (section 5.2).

## Triangle (phase 2)

- Trois sommets en dur dans `Renderer.cpp` (position déjà dans l'espace de clip de Vulkan, couleur linéaire), copiés une fois dans un vertex buffer créé par VMA.
- Pipeline sans descripteur ni push constant (G-02), viewport et scissor dynamiques, dynamic rendering. Pas d'élimination des faces avant la caméra de la phase 3 ; faces avant dans le sens antihoraire, comme glTF.
- Le pipeline est créé avec la swapchain, pour son format. Si une recréation change le format, l'ancien pipeline passe par la destruction différée.
- `lastFrameCounters()` : un appel de dessin et un triangle par frame rendue.
- Remplacé en phase 3 par les meshes, le format de sommet unique et la `RenderScene`.

## Swapchain

- Format sRGB : `B8G8R8A8_SRGB`, sinon `R8G8B8A8_SRGB`. Si le pilote ne propose aucun des deux, un avertissement est écrit : les couleurs seront trop sombres.
- Vsync activé : mode FIFO. Désactivé (`RendererConfig::vsync`, option `--no-vsync` de la démo) : mailbox, sinon immédiat, sinon FIFO.
- `SwapchainState` décide au début de chaque frame : sauter la frame (fenêtre sans surface), dessiner, ou recréer puis dessiner. La swapchain est créée à la première frame, puis recréée quand la taille de la fenêtre change ou quand l'acquisition ou la présentation renvoie `VK_ERROR_OUT_OF_DATE_KHR` ou `VK_SUBOPTIMAL_KHR`. Seule la taille au moment de la décision compte : plusieurs redimensionnements entre deux frames donnent une seule recréation.
- Piège : la fenêtre peut être réduite avant que l'événement soit lu. Elle annonce encore son ancienne taille alors que la surface Vulkan n'en a plus (`currentExtent` nul). Avant de recréer, le renderer interroge donc la surface ; si elle est vide, la frame est sautée et la recréation retentée à la frame suivante. La recréation attend que le GPU soit inactif : simple et toujours correct, pour un événement rare. Le format, l'extent et le nombre d'images sont écrits dans le log.

## Périphérique

- Instance Vulkan 1.3. Ses extensions sont celles que SDL demande pour présenter dans la fenêtre (`Window::vulkanInstanceExtensions()`), vk-bootstrap ne choisit pas les siennes.
- GPU retenu : le premier qui accepte Vulkan 1.3, `dynamicRendering`, `synchronization2` et la présentation dans la surface, un GPU dédié de préférence. Le nom du GPU et sa version de Vulkan sont écrits dans le log.
- Une file graphique et une file de présentation (souvent la même).

## Mémoire GPU

- Toute la mémoire GPU passe par `GpuAllocator`, qui encapsule VMA (note 0007). Il est créé après le périphérique, détruit après tous les buffers et avant le périphérique.
- `createBufferWithData(name, usage, data)` crée un buffer visible par le CPU et le remplit une fois : suffisant pour de petites données statiques (le triangle). Les transferts par buffer intermédiaire viendront avec les meshes. Un échec renvoie `ErrorCode::GpuFailure` avec le nom du buffer (section 5.4).
- `memoryStats()` additionne l'usage et le budget des tas `DEVICE_LOCAL` (section 5.9). Le budget est exact quand `VK_EXT_memory_budget` est disponible (activée si le pilote la propose), estimé sinon. La valeur est écrite dans le log au démarrage.
- L'implémentation de VMA compile sans avertissement en `/W4` : aucun avertissement n'est désactivé.

## Destruction différée

- Un objet GPU peut encore être lu par une frame en vol quand le code n'en a plus besoin (ancienne swapchain, buffer remplacé). Sa destruction est confiée au `DeferredDeleter` sous forme de fonction (section 5.4).
- `advanceFrame()` est appelé une fois par frame, juste après l'attente de la fence du créneau de frame qui va être réutilisé. Une destruction ajoutée pendant la frame N s'exécute au début de la frame N + 2 (deux frames en vol), après l'attente de la fence de la frame N : le GPU a fini tout ce qui pouvait l'utiliser.
- Piège : quand une frame est sautée après l'attente de sa fence (swapchain périmée), la frame suivante attend la même fence. Le renderer n'appelle alors pas `advanceFrame()` une seconde fois pour ce créneau : sinon le compteur avancerait sans que le GPU ait progressé, et un objet encore lu pourrait être détruit.
- Les destructions s'exécutent dans l'ordre d'ajout. Une destruction peut en ajouter une autre, qui attend les frames suivantes.
- `flushAll()` exécute tout ce qui reste, après une attente du GPU (fermeture). Le destructeur l'appelle aussi : le `DeferredDeleter` doit donc être détruit avant le périphérique.

## Shaders

- Sources GLSL 4.60 dans `Graphics/Shaders/`. `glslc`, fourni par le SDK Vulkan, les compile pendant le build : `--target-env=vulkan1.3 -Werror`, avec les informations de débogage en Debug (`-g`, utile dans RenderDoc) et optimisées ailleurs (`-O`). Une erreur ou un avertissement de shader fait échouer le build (section 3.1).
- `apex_add_shaders()` (`cmake/ApexShaders.cmake`) produit pour chaque shader un fichier `Shaders/<nom>.spv.inc` dans le dossier de build : la liste des mots SPIR-V au format d'un initialiseur C, incluse par `ShaderBinaries.cpp`. Aucun fichier de shader n'est cherché au lancement.
- Les fichiers inclus par un shader (`#include`, à venir avec les matériaux, G-08) sont suivis : les modifier recompile le shader.
- `glslc` est cherché dans `%VULKAN_SDK%\Bin`, puis dans le `PATH`. Sans lui, la configuration CMake échoue avec un message qui dit quoi installer.

## Validation

- Demandée par `RendererConfig::enableValidation`, que `Game` active en Debug. Si les layers ne sont pas installées (SDK Vulkan absent), un avertissement est écrit et le renderer continue sans elles.
- Messages reçus : avertissements et erreurs, de type général, validation et performance. Chaque message est écrit dans le log, préfixé par `[vulkan]`, et compté par `ValidationLog`. Aucun message n'est filtré ni ignoré (`AGENTS.md`, section 7).
- Le `ValidationLog` est créé par l'application avant le renderer et lu après sa destruction : les messages de la fermeture sont comptés. `ApexDemo` renvoie le code de sortie 2 si au moins un message a été émis.
- `report()` peut être appelé depuis n'importe quel thread : les layers appellent depuis le thread qui a fait l'appel Vulkan fautif.

## Dépendances

Autorisé : `Core`, `Platform`, `Resources`. Interdit : EnTT, Jolt, `World`. Bibliothèques : Vulkan-Headers, Vulkan-Loader et vk-bootstrap 1.4.363, Vulkan Memory Allocator 3.5.0 (liés en `PRIVATE` à `ApexEngine`). Les en-têtes Vulkan ne sont inclus que dans `Graphics` et dans `Debug/ImGuiBackend.cpp`.
