# Graphics

**Rôle :** renderer Vulkan 1.3, seul module qui émet des appels GPU (cahier des charges, sections 4.3 et 5.4, garde-fou G-01).

**État (phase 3) :** périphérique, mémoire GPU (VMA), swapchain sRGB, deux frames en vol, shaders compilés pendant le build, tampon de profondeur, meshes et matériaux, données par frame, `RenderScene` dessinée en une passe principale, interface dans une seconde passe. Notes de décision 0006, 0009 et 0011.

## Interface publique

Sauf `VulkanBackendAccess.h`, aucun en-tête public n'inclut Vulkan : `Game` peut inclure tous les autres.

| En-tête | Contenu |
|---|---|
| `Graphics/ValidationLog.h` | `ValidationSeverity`, `ValidationLog` : `report()`, `messageCount()` |
| `Graphics/FrameCounters.h` | `FrameCounters` : objets dessinés (un appel de dessin chacun) et triangles de la dernière frame, temps d'attente du CPU, temps GPU |
| `Graphics/GpuMemoryStats.h` | `GpuMemoryStats` : mémoire GPU utilisée et budget |
| `Graphics/RenderScene.h` | `RenderCamera`, `RenderLight`, `RenderShadow`, `RenderToneMapping`, `RenderEnvironment`, `RenderLine`, `RenderObject`, `RenderScene` : tout ce que `Graphics` dessine dans une frame (G-01) |
| `Graphics/RenderOverlay.h` | `RenderOverlay` : ce qui est dessiné par-dessus la scène (l'interface de debug) |
| `Graphics/VulkanBackendAccess.h` | `OverlayCommands`, `VulkanBackendInfo`, `vulkanBackendInfo()`. **Seul en-tête public avec des types Vulkan, réservé à `Debug/ImGuiBackend.cpp`** (note 0009) |
| `Graphics/Renderer.h` | `RendererConfig` (dont la résolution de la shadow map), `Renderer` : `create()`, `syncResources()`, `renderFrame()`, `presentFrame()`, `memoryStats()`, `lastFrameCounters()`, `setOverlay()` |

En-têtes internes, inclus uniquement par les fichiers `.cpp` de `Graphics` :

- `Graphics/Device.h` (`DeviceConfig`, `Device`), qui inclut Vulkan et vk-bootstrap ;
- `Graphics/DeferredDeleter.h` (`DeferredDeleter`), sans Vulkan, testé sans GPU ;
- `Graphics/Swapchain.h` (`SwapchainConfig`, `Swapchain`) ;
- `Graphics/SwapchainState.h` (`FrameDecision`, `SwapchainState`), sans Vulkan, testé sans GPU ;
- `Graphics/ShaderBinaries.h` : le SPIR-V des shaders, embarqué dans le binaire ;
- `Graphics/GpuAllocator.h` (`GpuBuffer`, `GpuAllocator`), seul utilisateur de VMA. L'implémentation de VMA est compilée dans `VmaImplementation.cpp` ;
- `Graphics/RenderTargetDesc.h` (`RenderTargetDesc`) : formats de couleur et de profondeur et nombre d'échantillons de la cible, passés à la création des pipelines (G-09) ;
- `Graphics/DepthTarget.h` (`DepthTarget`) : une image de profondeur et sa vue (tampon de profondeur de la passe principale, shadow map) ;
- `Graphics/GraphicsPipeline.h` (`MeshAttribute`, `PipelineDesc`, `GraphicsPipeline`) : un pipeline et son layout, décrit par ce qui change d'une passe à l'autre (attributs de sommet, topologie, élimination des faces, profondeur, biais) ;
- `Graphics/ShadowMap.h` (`ShadowMap`) : la shadow map et son sampler de comparaison ;
- `Graphics/ColorTarget.h` (`ColorTarget`) : une image de couleur dessinée puis lue par une autre passe (l'image HDR de la scène) ;
- `Graphics/ImageInputSet.h` (`ImageInputSet`) : un jeu de descripteurs d'une seule image lue, celui de la passe de tonemapping ;
- `Graphics/MipLevels.h` (`mipLevelCount()`, `mipSize()`), sans Vulkan, testé sans GPU ;
- `Graphics/Frustum.h` (`Frustum`, `frustumFromMatrix()`, `transformSphere()`, `intersects()`), sans Vulkan, testé sans GPU ;
- `Graphics/LightingSets.h` (`LightingSets`) : le jeu 2 de la scène et du ciel, un par environnement ;
- `Graphics/EnvironmentPrefilter.h` (`PrefilterParams`, `EnvironmentPrefilter`) : le pipeline qui préfiltre la cubemap spéculaire ;
- `Graphics/GpuEnvironment.h` (`GpuEnvironment`, `createGpuEnvironment()`, `createBrdfTable()`, `createBlackCube()`) : un environnement sur le GPU ;
- `Graphics/RendererState.h` : l'état privé du `Renderer` et ses limites (frames en vol, objets, matériaux).
- `Graphics/FrameSync.h` (`FrameResources`, `FrameSync`) ;
- `Graphics/ResourceMirror.h` (`ResourceMirror`), sans Vulkan, testé sans GPU ;
- `Graphics/GpuTexture.h` (`GpuTexture`, `textureFormat()`, `createGpuTexture()`, `destroyGpuTexture()`) ;
- `Graphics/MaterialSets.h` (`MaterialSets`) : jeux de descripteurs par matériau et sampler ;
- `Graphics/Projection.h` (`perspectiveProjection()`, `orthographicProjection()`), sans Vulkan, testé sans GPU ;
- `Graphics/DrawList.h` (`DrawItem`, `sortDrawItems()`), sans Vulkan, testé sans GPU ;
- `Graphics/UploadContext.h` (`BufferCopy`, `UploadContext`) : copies vers la mémoire GPU par buffers de transit ;
- `Graphics/GpuMesh.h` (`GpuMesh`, `createGpuMesh()`, `destroyGpuMesh()`) ;
- `Graphics/MaterialTable.h` (`MaterialGpu`, `MaterialTable`) : reflet des matériaux du cache, sans Vulkan, testé sans GPU ;
- `Graphics/FrameData.h` (`FrameUniforms`, `ObjectGpu`, `LineVertex`, `makeFrameUniforms()`, `exposureMultiplier()`, `shadowViewProjection()`, `makeObjectGpu()`), sans Vulkan, testé sans GPU ;
- `Graphics/FrameBuffers.h` (`FrameBufferLimits`, `FrameBuffers`) : buffers et jeu de descripteurs de chaque frame en vol ;
- `Graphics/GpuTimestamps.h` (`TimestampFormat`, `elapsedSeconds()`), sans Vulkan, testé sans GPU ;
- `Graphics/EnvironmentLighting.h` (`shBasis()`, `IrradianceSh`, `projectIrradiance()`, `irradianceAt()`, `computeBrdfTable()`) : la part de l'IBL calculée sur le CPU (décision 0022), sans Vulkan, testée sans GPU ;
- `Graphics/GpuTimer.h` (`GpuTimer`) : les horodatages de chaque frame en vol.

## Renderer

- `Renderer::create(window, logger, validation, config)` crée le périphérique. Échec : `ErrorCode::GpuFailure` (ou `PlatformFailure` si SDL ne peut pas créer la surface), avec un message qui dit ce qui manque. Le message est aussi écrit dans le log.
- La fenêtre, le logger et le `ValidationLog` doivent vivre plus longtemps que le renderer. Le renderer n'est ni copiable ni déplaçable : il est tenu par un `std::unique_ptr`.
- Le destructeur attend la fin du travail du GPU, puis détruit la swapchain et les objets des frames, exécute les destructions différées restantes, et détruit enfin le périphérique, la surface et l'instance.
- `Renderer.h` n'expose aucun type Vulkan : l'état du renderer est une structure privée, définie dans l'en-tête interne `RendererState.h` et partagée par trois fichiers. `Renderer.cpp` tient le périphérique, la swapchain et la boucle de frame ; `RendererResources.cpp` le reflet GPU du cache de ressources ; `RendererRecording.cpp` la liste de dessin et l'enregistrement des commandes.
- La swapchain est créée dès `create()` quand la fenêtre a une taille : le backend Vulkan d'ImGui a besoin de son format à l'initialisation.
- `setOverlay()` : l'overlay enregistré dessine dans une seconde passe, après la scène et sans profondeur (notes 0008 et 0011).

## Frame

- `renderFrame()` (phase de rendu) : attend la fence du créneau de frame, acquiert une image de la swapchain, enregistre et soumet les commandes. `presentFrame()` (phase de présentation) présente l'image acquise. Une erreur renvoyée par l'une ou l'autre signifie que le périphérique est inutilisable : la démo s'arrête avec le code de sortie 1.
- Deux frames en vol (section 5.4) : chacune a son pool et son command buffer, sa fence et son sémaphore « image disponible ».
- Le sémaphore « rendu terminé », attendu par la présentation, existe une fois par **image** de la swapchain, et non par frame en vol. Avec un sémaphore par frame, il pourrait être signalé de nouveau alors que la présentation précédente l'attend encore.
- Le rendu se fait par dynamic rendering, sans render pass. Les transitions d'image utilisent synchronization2.
- Cinq passes (notes 0011, 0021 et 0023) : l'ombre, dans la shadow map ; la scène, dans l'image HDR, qui efface la couleur et la profondeur ; le tonemapping, de l'image HDR vers l'image de la swapchain ; les lignes des colliders, s'il y en a, avec la profondeur de la scène ; l'interface, qui charge la couleur et n'a pas de profondeur. Des barrières séparent chaque écriture de la lecture suivante. Le backend ImGui n'a pas à connaître le format de profondeur ni l'image HDR.
- Profondeur : `D32_SFLOAT` (section 5.4), choisi en un seul endroit (`Renderer.cpp`) et passé aux pipelines (G-09). Test « inférieur », effacement à 1. Le tampon est recréé avec la swapchain, à sa taille ; son contenu n'est pas conservé après la passe. Un seul tampon sert aux deux frames en vol : la barrière du début de frame attend les écritures de profondeur de la frame précédente.
- Fenêtre réduite (taille nulle) : `renderFrame()` et `presentFrame()` ne font rien (section 5.2).

## Temps GPU (phase 7, note 0023)

- Deux horodatages par frame en vol, dans un pool de requêtes : le premier au début des commandes de la frame, le second à la fin. Les deux requêtes sont remises à zéro par le command buffer lui-même, juste avant.
- Lecture sans attente, juste après l'attente de la fence du créneau : le GPU a fini, les résultats sont là. `lastFrameCounters().gpuSeconds` donne donc le temps de la dernière frame passée par ce créneau, deux frames plus tôt. Une frame sautée après l'attente de sa fence ne relit pas la même valeur.
- Le compteur du GPU n'a que `timestampValidBits` bits utiles (ceux de la famille de la file graphique) : la différence est calculée modulo cette taille, ce qui compte juste un compteur repassé par zéro. Les ticks sont convertis en secondes par `timestampPeriod`.
- Une file sans horodatages (`timestampValidBits` nul) : pas de pool, pas de valeur. Le log dit au démarrage si les horodatages sont disponibles.

## Passe de scène (phase 3)

- `renderFrame(scene)` résout les handles de chaque objet (voir « Ressources »), puis élimine ce qui sort du champ (voir « Frustum culling »). Au-delà de 4096 objets, les suivants ne sont pas dessinés (avertissement écrit une fois).
- Les objets sont triés par matériau puis par mesh, puis leurs données sont écrites dans le buffer de la frame dans cet ordre. Le dessin `i` utilise `firstInstance = i` : le vertex shader lit l'objet par `gl_InstanceIndex` (G-02). Les buffers de sommets et d'indices ne sont liés qu'au changement de mesh.
- Pipeline de scène (`GraphicsPipeline`) : jeu de descripteurs 0 de la frame et jeu 1 du matériau, aucun push constant ; position, normale, tangente et UV lues dans le format de sommet unique. Un attribut que le shader ne lit pas n'est pas déclaré : les layers le signaleraient ; faces arrière éliminées, faces avant antihoraires ; test de profondeur « inférieur » ; viewport et scissor dynamiques. Il est créé avec la swapchain, pour ses formats de couleur et de profondeur. Si une recréation change un format, l'ancien pipeline passe par la destruction différée.
- `lastFrameCounters()` : objets dessinés et triangles de la dernière frame rendue, et le temps passé bloqué sur la fence, l'acquisition et la présentation (`waitSeconds`), qui permet au panneau de debug de calculer le temps CPU.
- Le triangle de la phase 2 est retiré ; le tag `phase-2` le conserve.

## Lignes des colliders (phase 7, décision 0023)

- `RenderScene::lines` : des segments en espace de rendu, avec leur couleur. Ils sont copiés à chaque frame dans un vertex buffer de la frame (`FrameBuffers::writeLines()`, 65 536 sommets au plus ; au-delà, avertissement écrit une fois).
- Passe dessinée après le tonemapping, dans l'image de la swapchain : la couleur ne dépend pas de l'exposition. Elle charge le tampon de profondeur de la scène (gardé en fin de passe de scène seulement quand il y a des lignes) et teste « inférieur ou égal », sans écrire : un collider caché derrière un mur reste caché.
- `Shaders/Line.vert` rapproche chaque sommet de la caméra de 0,5 % de sa distance (la caméra est à l'origine de l'espace de rendu) : une arête posée sur une surface reste devant elle au lieu de clignoter.
- Pipeline : topologie en lignes, sommets `LineVertex` (position, couleur), jeu 0 de la frame pour la vue-projection, aucun push constant ; recréé avec la swapchain.

## Frustum culling (phase 7, décision 0021)

- Chaque mesh envoyé garde sa sphère englobante (`resources::boundingSphere()`, centrée sur la boîte de ses sommets). À chaque frame, elle est placée par la matrice de l'objet, son rayon multiplié par la plus grande échelle des trois axes.
- Deux volumes : celui de la caméra (vue-projection) et, ombres actives, celui de la lumière (la boîte orthographique de la zone). Plans extraits de la matrice (Gribb et Hartmann), pour la profondeur de 0 à 1 de Vulkan. Une sphère n'est éliminée que si elle est entièrement hors d'un plan : près d'un coin, un objet invisible peut être gardé, jamais l'inverse.
- Un objet vu par l'un ou l'autre entre dans le buffer de la frame (une seule fois) ; chaque passe ne dessine que les siens (`sceneDraws`, `shadowDraws`). « Objets dessinés » du panneau : ceux de la passe principale (AC-P1-4) ; la passe d'ombre a son compteur.

## IBL et ciel (phase 7, décision 0022)

- `syncResources()` reflète aussi les environnements du cache. À l'envoi d'un environnement : irradiance projetée sur neuf harmoniques sphériques (CPU), image équirectangulaire envoyée en flottants 16 bits avec ses mipmaps (blits), cubemap spéculaire de 256 × 256 par face et 6 niveaux préfiltrée sur le GPU, puis l'image équirectangulaire est libérée. Le log donne la durée (environ 1,2 s en Debug sur la machine de référence). Un échec est écrit avec le nom de l'environnement ; la scène est alors éclairée par le terme ambiant.
- Préfiltrage (`Shaders/Prefilter.frag`) : un triangle plein écran par face et par niveau, l'instance donnant le niveau et la face. Niveau 0 : copie de l'image source ; niveau n : rugosité n / 5, 128 échantillons GGX autour de la normale (vue le long de la normale, approximation de Karis), chacun lisant le niveau de la source qui correspond à son angle solide (filtered importance sampling). Paramètres dans un uniform buffer, aucun push constant.
- Table BRDF du « split sum » : 64 × 64, calculée une fois sur le CPU à la création du renderer, envoyée en `R16G16_SFLOAT`.
- Jeu 2 de la scène (`LightingSets`) : cubemap spéculaire (binding 0) et table BRDF (binding 1), un seul sampler trilinéaire. Sans environnement prêt, ou l'IBL coupée : le jeu par défaut, avec une cubemap noire de 1 × 1, et le terme ambiant constant (G-03).
- `FrameUniforms` : les neuf coefficients (multipliés par l'intensité dans le shader), un indicateur, l'intensité et le dernier niveau de la cubemap. `Shaders/Environment.glsl` : irradiance par les harmoniques, spéculaire par la cubemap au niveau `rugosité × 5` et la table BRDF. Il remplace le terme ambiant quand l'environnement est actif.
- Ciel (`Shaders/Sky.vert`, `Sky.frag`) : un triangle plein écran au plan lointain, dessiné après les objets dans la passe de scène, test de profondeur « inférieur ou égal », sans écriture de profondeur. La direction vient de l'inverse de la vue-projection ; il lit le niveau 0 de la cubemap. Même layout que le pipeline de scène : les jeux liés pour les objets le restent.
- Convention des directions : celle de `resources::environmentUv()` pour l'image source, l'ordre des faces de Vulkan pour la cubemap (+X, −X, +Y, −Y, +Z, −Z).
- Génération des mipmaps : `ImageCopy::mipLevels` ; chaque niveau est fait par un blit linéaire du précédent, et tous finissent lisibles par les fragment shaders. `UploadContext::run()` exécute un travail ponctuel sur le GPU et l'attend (préfiltrage, cubemap noire).

## HDR et tonemapping (phase 7, décision 0021)

- La scène est dessinée dans une image `R16G16B16A16_SFLOAT` (section 5.4), à la taille de la swapchain et recréée avec elle. Format choisi dans `Renderer.cpp` seulement, passé au pipeline de scène (G-09).
- Passe de tonemapping : un triangle qui couvre l'écran, fait par le vertex shader sans buffer de sommets (`Shaders/Fullscreen.vert`). `Shaders/Tonemap.frag` lit l'image HDR texel par texel (`texelFetch` aux coordonnées du pixel), la multiplie par l'exposition, puis applique l'approximation ACES de Narkowicz ; l'écriture dans la swapchain sRGB fait l'encodage.
- `RenderScene::toneMapping` : exposition en stops (multiplicateur `2^stops`, `exposureMultiplier()`) et courbe active ou non. Sans la courbe, l'exposition reste appliquée et les valeurs au-dessus de 1 saturent : la comparaison isole la courbe (AC-P1-5).
- Le pipeline de tonemapping lit le jeu 0 de la frame (exposition dans `FrameUniforms`) et un jeu 1 d'une seule image (`ImageInputSet`), réécrit à chaque recréation de la swapchain : le GPU est alors inactif. Il est recréé si le format de la swapchain change.
- Une seule image HDR pour les deux frames en vol : la barrière du début de frame attend que le tonemapping de la frame précédente ait fini de la lire.

## Ombres (phase 7, décision 0021)

- Passe d'ombre avant la passe principale : profondeur seule, dans une shadow map carrée dont la résolution vient de `RendererConfig::shadowMapSize` (2048 par défaut, section 5.4, G-09). Format `D16_UNORM`, choisi dans `Renderer.cpp` seulement : sur 16 bits fixes, une unité de biais constant vaut `depthRange / 65536` mètres, ce qui permet de régler ce biais en mètres. Sur 100 m, la précision est de 1,5 mm.
- La zone est cadrée par `World` (`RenderShadow`) : une vue de la lumière relative à la caméra, une demi-largeur, une profondeur. `shadowViewProjection()` y ajoute la projection orthographique, mêmes conventions que la perspective (profondeur de 0 à 1, Y retourné).
- Pipeline d'ombre : position seule, aucun fragment shader, pas d'élimination des faces (un mesh mince ou ouvert projette quand même son ombre), biais de profondeur dynamique : `vkCmdSetDepthBias` à chaque frame avec les biais de la scène.
- Dessins : les mêmes objets, dans le même buffer par frame, que la passe principale (`gl_InstanceIndex`). Leur nombre est compté à part (`FrameCounters::shadowDrawCalls`).
- La passe tourne même quand les ombres sont coupées : elle efface alors la carte sans rien dessiner, pour que le descripteur du shader de scène désigne toujours une image valide.
- Une seule shadow map pour les deux frames en vol : la barrière du début de passe attend que la passe principale de la frame précédente ait fini de la lire ; celle de la fin la rend lisible au fragment shader.
- Échantillonnage (`Shaders/Shadow.glsl`, jeu 0, binding 3) : sampler de comparaison « inférieur ou égal », filtré linéairement si le format le permet (chaque lecture compare alors 2 × 2 texels), bordure blanche : hors de la carte, tout est éclairé. PCF 3 × 3. Hors de la zone, ni ombre ni coupure : l'ombre s'estompe sur le dernier dixième de la demi-largeur (`kShadowFadeStart`). Seule la lumière directionnelle est atténuée ; le terme ambiant ne l'est pas.

## Scène et projection (phase 3)

- `RenderScene` (G-01) : caméra, lumière directionnelle, terme ambiant et liste d'objets, copiés par l'application à chaque frame. Aucun pointeur vers les objets du jeu.
- Espace de rendu : l'espace monde translaté pour que la caméra soit à l'origine (G-07). `RenderCamera::view` n'est qu'une rotation, et `RenderObject::modelRelativeToCamera` porte la position de l'objet relative à la caméra. La conversion depuis `WorldPosition` se fait dans l'extraction, jamais dans `Graphics`.
- `perspectiveProjection()` est le seul endroit qui connaît l'espace de clip de Vulkan (section 3.3) : vue main droite tournée vers −Z, profondeur de 0 (plan proche) à 1 (plan lointain), Y inversé pour que le haut de la vue soit en haut de l'écran. Une face antihoraire vue de la caméra reste une face avant pour Vulkan : un test le vérifie avec la formule d'aire de la spécification Vulkan.
- La liste d'objets est triée par matériau, puis par mesh, en gardant l'ordre de la scène dans un groupe (`sortDrawItems()`, section 5.4).

## Ressources (décision 0014)

- Les meshes et les matériaux appartiennent au cache de `Resources` ; le renderer n'en garde qu'un reflet GPU, désigné par les handles de `Resources`. `syncResources(cache)`, appelé à chaque frame avant `renderFrame()`, compare chaque pool du cache à ce reflet (`ResourceMirror`) : ce qui est devenu prêt est envoyé au GPU, ce qui a été déchargé ou dont l'emplacement a été réutilisé est libéré par la destruction différée. Un pool qui n'a pas changé (`revision()`) n'est pas parcouru.
- Envoi d'un mesh : buffers de sommets et d'indices en mémoire GPU, remplis par des buffers de transit ; l'appel attend la fin de la copie. Un échec est écrit dans le log avec le nom du mesh, et n'est pas retenté tant que le mesh existe (section 5.4).
- Résolution à chaque frame (G-03) : un mesh pas encore envoyé, en échec, périmé ou absent du GPU donne le mesh de secours du cache (cube de 1 m). Un matériau dans le même cas donne l'entrée 0 de la table, le matériau par défaut. Aucun pointeur vers les données du cache n'est gardé d'une frame à l'autre.
- Table des matériaux (note 0011) : l'emplacement `i` du pool de matériaux du cache est l'entrée `i + 1` de la table ; au-delà de 256 entrées, un avertissement est écrit et les matériaux suivants reçoivent le matériau par défaut. La table est reconstruite quand le pool change, puis recopiée dans le buffer de chaque frame : retirer un matériau ne gêne pas une frame en vol.
- Textures : une image GPU par texture prête, avec toute sa chaîne de mipmaps faite sur le GPU par blits linéaires depuis le premier niveau (phase 7, décision 0021), RGBA 8 bits, format `R8G8B8A8_SRGB` pour une couleur et `R8G8B8A8_UNORM` pour des données (section 3.3). La copie passe de `UNDEFINED` à `SHADER_READ_ONLY_OPTIMAL` et une barrière la rend visible aux fragment shaders des frames suivantes. Un échec est écrit dans le log avec le nom de la texture.
- Jeu de descripteurs par matériau (section 5.4, note 0014), le jeu 1 : texture de couleur de base (binding 0), texture métal-rugosité (binding 1) et normal map (binding 2, phase 7), par un seul sampler trilinéaire et répétant, anisotrope 8× quand le GPU propose `samplerAnisotropy` (activée alors à la création du périphérique ; le log le dit au démarrage). Un handle de texture par défaut (« pas de texture ») donne la texture blanche ; une texture en chargement, en échec ou périmée donne le damier magenta (G-03). Pour la normal map, les deux cas donnent la normale plate du cache (décision 0021).
- Les jeux renvoient à des vues d'image : quand le pool de textures change, tous les jeux de matériau sont refaits, et les anciens libérés par la destruction différée. Le matériau par défaut a son propre jeu (deux textures blanches). Si le pool de descripteurs est plein, un avertissement est écrit et le matériau reçoit le jeu par défaut.
- Les dessins sont triés par matériau : le jeu 1 n'est lié qu'au changement de matériau.
- Après chaque synchronisation qui a envoyé quelque chose, une ligne de log donne le nombre de meshes et de textures envoyés et la mémoire GPU utilisée : de quoi suivre la dérive mémoire (AC-14, AC-15).
- À la destruction du renderer, tous les meshes et toutes les textures encore présents sur le GPU sont libérés.

## Données par frame (G-02)

- Un jeu de descripteurs par frame en vol, le jeu 0 (section 5.4, note 0011) :
  - binding 0, uniform buffer `FrameUniforms` : matrice vue-projection, matrice de la lumière (espace de rendu vers la shadow map), direction vers la lumière, radiance de la lumière (couleur × intensité), terme ambiant, réglages des ombres ;
  - binding 1, storage buffer d'`ObjectGpu` : matrice du modèle relative à la caméra, matrice des normales, index de matériau. Un élément par objet dessiné, dans l'ordre des appels de dessin ; le shader le lit par `gl_InstanceIndex` ;
  - binding 2, storage buffer de `MaterialGpu` : la table des matériaux (couleur, métal, rugosité, facteur de normal map) ;
  - binding 3, la shadow map et son sampler de comparaison (phase 7), écrit une fois à la création.
- Aucune matrice d'objet en push constant.
- Les buffers sont visibles par le CPU, mappés pendant toute l'exécution et réécrits à chaque frame après l'attente de sa fence. `flush()` les rend visibles au GPU quand la mémoire n'est pas cohérente.
- Les structures C++ (`FrameData.h`, `MaterialTable.h`) et GLSL (`Shaders/Frame.glsl`, `Shaders/Material.glsl`) ont la même disposition : des `static_assert` et des tests vérifient tailles et décalages. Les shaders accèdent aux matériaux uniquement par `Material.glsl` (G-08).
- Matrice des normales : inverse transposée de la partie 3 × 3 du modèle, pour garder les normales perpendiculaires aux surfaces sous une échelle non uniforme.

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
- `createImage(name, info, dedicated)` crée une image en mémoire GPU ; dans son propre bloc pour une cible de rendu (`dedicated`), sous-allouée pour une texture. `destroyImage()` la libère immédiatement.
- `memoryStats()` additionne l'usage et le budget des tas `DEVICE_LOCAL` (section 5.9). Avec `VK_EXT_memory_budget`, l'usage est celui de tout le processus dans ces tas : blocs réservés par VMA (plus grands que les ressources qu'ils contiennent), swapchain et allocations du pilote. Sur la machine de référence, la scène de la phase 4 (environ 40 Mio de textures et de meshes) affiche environ 220 Mio ; la valeur à surveiller est sa stabilité dans le temps, pas son niveau. Le budget est exact quand `VK_EXT_memory_budget` est disponible (activée si le pilote la propose), estimé sinon. La valeur est écrite dans le log au démarrage.
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
- Les fichiers inclus par un shader (`#include`) sont suivis par les fichiers de dépendances de `glslc` : les modifier recompile le shader. `Frame.glsl` déclare les données par frame, `Material.glsl` la table des matériaux, leurs textures et `sampleMaterial()` (seul accès des shaders aux matériaux, G-08), `Pbr.glsl` l'éclairage.
- Shaders de la scène : `Scene.vert` lit l'objet par `gl_InstanceIndex` et passe la position, la normale (matrice des normales) et la tangente (matrice du modèle, signe gardé) en espace de rendu ; `Scene.frag` courbe la normale par la normal map (`sampleNormal()` de `Material.glsl` : tangente réorthogonalisée, bitangente `cross(n, t) × w`, facteur `normalScale` sur X et Y) puis calcule l'éclairage.

## Éclairage (phase 3)

- Modèle metallic-roughness de glTF 2.0, simplifié (section 5.5), dans `Pbr.glsl` : diffus de Lambert, distribution GGX, visibilité de Smith corrélée en hauteur, Fresnel de Schlick. Réflectance des diélectriques en incidence normale : 0,04. Rugosité perceptuelle bornée à 0,045 en bas, pour qu'un matériau parfaitement lisse garde un reflet visible.
- Une lumière directionnelle (couleur × intensité) et un terme ambiant constant, appliqué aux parts diffuse et spéculaire. Avec un environnement, l'IBL le remplace (voir « IBL et ciel »).
- Calcul en espace linéaire, dans l'image HDR ; exposition, tonemapping puis encodage sRGB après (voir « HDR et tonemapping »).
- La vue est calculée en espace de rendu : la caméra est à l'origine, la direction vers l'observateur est `-position`.
- `glslc` est cherché dans `%VULKAN_SDK%\Bin`, puis dans le `PATH`. Sans lui, la configuration CMake échoue avec un message qui dit quoi installer.

## Validation

- Demandée par `RendererConfig::enableValidation`, que `Game` active en Debug. Si les layers ne sont pas installées (SDK Vulkan absent), un avertissement est écrit et le renderer continue sans elles.
- Messages reçus : avertissements et erreurs, de type général, validation et performance. Chaque message est écrit dans le log, préfixé par `[vulkan]`, et compté par `ValidationLog`. Aucun message n'est filtré ni ignoré (`AGENTS.md`, section 7).
- Le `ValidationLog` est créé par l'application avant le renderer et lu après sa destruction : les messages de la fermeture sont comptés. `ApexDemo` renvoie le code de sortie 2 si au moins un message a été émis.
- `report()` peut être appelé depuis n'importe quel thread : les layers appellent depuis le thread qui a fait l'appel Vulkan fautif.

## Dépendances

Autorisé : `Core`, `Platform`, `Resources` (structures CPU, handles et cache, en lecture). Interdit : EnTT, Jolt, `World`. Bibliothèques : Vulkan-Headers, Vulkan-Loader et vk-bootstrap 1.4.363, Vulkan Memory Allocator 3.5.0 (liés en `PRIVATE` à `ApexEngine`). Les en-têtes Vulkan ne sont inclus que dans `Graphics` et dans `Debug/ImGuiBackend.cpp`.
