# 0006 — Chaîne Vulkan : SDK, en-têtes, loader, `glslc`, vk-bootstrap

**Date :** 19 novembre 2026
**Statut :** acceptée (choix délégués à l'agent par le développeur le 19 novembre 2026)

## Contexte

La phase 2 initialise Vulkan 1.3 et affiche un triangle (cahier des charges, sections 5.4 et 9.1). Le cahier des charges retient vk-bootstrap pour l'initialisation, des shaders GLSL compilés par `glslc` pendant le build, et un SDK LunarG installé à part, qui fournit les validation layers et `glslc` (sections 3.1 et 3.2).

Trois points ne sont pas tranchés par le cahier des charges : d'où viennent les en-têtes et le loader Vulkan, comment la CI obtient `glslc`, et quoi faire d'un message de validation.

État de la machine du développeur au 19 novembre 2026 : GPU Intel Iris Xe, pilote Vulkan 1.4.323 ; SDK LunarG non installé.

## Décision

**Dépendances vcpkg** (versions de la baseline épinglée, à confirmer à l'installation) :

- `vulkan-headers` 1.4.363.0 et `vulkan-loader` 1.4.363.0 ;
- `vk-bootstrap` 1.4.363 ;
- `sdl3` avec la fonctionnalité `vulkan`, annoncée par la note 0002 : sans elle, le port compile SDL sans prise en charge de Vulkan.

Les en-têtes et le loader utilisés par le build sont toujours ceux de vcpkg, jamais ceux du SDK. Les cibles sont liées en `PRIVATE` à `ApexEngine`.

**SDK LunarG**, prérequis à partir de la phase 2, version 1.4.363.1 (celle des en-têtes) :

- il fournit les validation layers, utilisées en configuration Debug ;
- il fournit `glslc`, cherché par CMake dans `%VULKAN_SDK%\Bin`. S'il est introuvable, la configuration échoue avec un message qui dit quoi installer.

**Shaders :** `glslc --target-env=vulkan1.3`, avertissements traités comme des erreurs. Le SPIR-V est généré dans le dossier de build et embarqué dans la bibliothèque (sortie `-mfmt=c` de `glslc`) : aucun fichier de shader à trouver au lancement.

**CI :** une étape installe le SDK 1.4.363.1 avec l'installateur officiel, en mode silencieux, et le met en cache. La version est écrite dans `ci.yml`. La CI compile les shaders et le code Vulkan, mais n'exécute aucun appel Vulkan : le runner n'a pas de GPU.

**Périphérique :** Vulkan 1.3 avec `dynamicRendering` et `synchronization2`. Si aucun GPU ne convient, l'application logue la raison et s'arrête avec un code d'erreur.

**Validation :**

- activée en Debug, désactivée en Release ;
- si elle est demandée mais que les layers ne sont pas installées, l'application logue un avertissement et continue ;
- chaque message passe par le logger (avertissement ou erreur) et est compté. Aucun message n'est filtré ;
- `ApexDemo` renvoie le code de sortie 2 si au moins un message a été émis, fermeture comprise.

**Vérification par l'agent :** `ApexDemo --frames N` quitte seul après N frames. L'agent peut ainsi lancer la démonstration deux secondes après chaque tâche graphique et lire le code de sortie.

**GLM :** reporté en phase 3. Un triangle n'a besoin d'aucune matrice, et `AGENTS.md` demande de n'ajouter une dépendance que lorsque la phase en a besoin. La liste de la section 8 d'`AGENTS.md` est corrigée.

## Alternatives écartées

- **`glslc` par le port vcpkg `shaderc` :** version épinglée par la baseline, mais c'est une dépendance de plus, qui compile glslang et SPIRV-Tools, alors que le SDK est de toute façon requis pour la validation. Reste la solution de repli si l'installation du SDK en CI se révèle fragile.
- **En-têtes et loader du SDK :** leur version dépendrait de ce que chaque machine a installé. vk-bootstrap et Dear ImGui tirent déjà `vulkan-headers` par vcpkg : il y aurait deux jeux d'en-têtes.
- **Validation layers par vcpkg (`vulkan-validationlayers`) :** compilation longue, et le cahier des charges désigne le SDK.
- **Initialisation écrite à la main, sans vk-bootstrap :** plusieurs centaines de lignes de sélection de périphérique et de swapchain, écartées par le cahier des charges (section 11, risque de blocage en phase 2).
- **Fichiers `.spv` chargés au lancement :** oblige à retrouver un dossier relatif à l'exécutable, pour un gain nul tant que les shaders ne sont pas rechargés à chaud.
- **Arrêt immédiat au premier message de validation :** empêche de lire les messages suivants, souvent utiles pour comprendre le premier.

## Conséquences

- **Modules touchés :** `Graphics` (nouveau), `Platform` (voir la note 0009), `Game`, `cmake/`, `Engine/CMakeLists.txt`, `vcpkg.json`, `.github/workflows/ci.yml`, `README.md`, `ThirdPartyLicenses.md`, `AGENTS.md`.
- **Garde-fous :** G-01 et G-10. Les en-têtes Vulkan ne sont inclus que dans `Graphics` et dans le backend ImGui de `Debug`.
- **Précision par rapport au cahier des charges :** la section 3.2 dit que le SDK fournit les validation layers et `glslc`. C'est respecté ; la note ajoute que les en-têtes et le loader viennent de vcpkg.
- **Guide d'installation (AC-01) :** le SDK passe de « à partir de la phase 2 » à « requis » dans `README.md`.
- **Licences :** vk-bootstrap, MIT ; Vulkan-Headers, « Apache-2.0 OR MIT » ; Vulkan-Loader, « Apache-2.0 AND MIT AND HPND-Kevlin-Henney » selon le port. À vérifier dans les fichiers `copyright` installés. Le SDK est un outil installé à part, non redistribué.
- **Exécution :** vcpkg copie `vulkan-1.dll` à côté de l'exécutable.
- **CI :** téléchargement d'environ 300 Mo à la première exécution, puis cache.
- **Coût de retour en arrière :** vk-bootstrap n'est utilisé que dans deux fichiers internes de `Graphics` (périphérique et swapchain).
